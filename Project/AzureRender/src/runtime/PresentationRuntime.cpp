#include "runtime/PresentationRuntime.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "animation/ExpressionPose.hpp"
#include <fstream>
namespace azurerender {
PresentationRuntime::PresentationRuntime(RuntimeLifecycle& runtime,AssetDatabase& assets,bool device):runtime_(runtime),assets_(assets),audio_(device){}
PresentationRuntime::~PresentationRuntime()=default;
void PresentationRuntime::synchronize(){
    if(revision_!=runtime_.sceneRevision()){
        for(const auto& sound:sounds_)audio_.release(sound.second.handle);
        sounds_.clear();animations_.clear();failed_.clear();revision_=runtime_.sceneRevision();
    }
    for(auto it=sounds_.begin();it!=sounds_.end();){
        auto* value=runtime_.world().tryGet<game::AudioSource>(it->first);
        auto& sound=it->second;
        if(!value||!value->enabled||runtime_.nodeId(it->first)!=sound.node||
            value->asset!=sound.asset||value->loop!=sound.loop) {
            audio_.release(sound.handle);
            it=sounds_.erase(it);
        }else{
            if(value->volume!=sound.volume) {
                audio_.setVolume(sound.handle,value->volume);
                sound.volume=value->volume;
            }
            ++it;
        }
    }
    for(auto it=animations_.begin();it!=animations_.end();){auto* value=runtime_.world().tryGet<game::Animator>(it->first);
        if(!value||!value->enabled||value->asset!=it->second.asset||runtime_.nodeId(it->first)!=it->second.node)it=animations_.erase(it);else ++it;}
    auto failure=[this](const std::string& key,const std::exception& error){if(failed_.insert(key).second){errors_.push_back(error.what());RuntimeDiagnostics::instance().warning("presentation",error.what());}};
    runtime_.world().each<game::Animator>([&](auto entity,const auto& value){
        const auto key="anim:"+runtime_.nodeId(entity)+value.asset;
        if(!value.enabled||value.asset.empty()||animations_.count(entity)||failed_.count(key))return;
        try{std::ifstream file(assets_.resolveReference(value.asset));nlohmann::json graph;file>>graph;
            auto machine=AnimationStateMachine::parse(graph);machine.select(value.state);
            machine.advance(value.startTime);
            LocomotionBlendState locomotion;
            if(machine.locomotionProfile())evaluateLocomotion(*machine.locomotionProfile(),0,value.startTime,locomotion);
            animations_.emplace(entity,Animation{value.asset,value.state,runtime_.nodeId(entity),std::move(machine),std::move(locomotion)});
        }catch(const std::exception& error){failure(key,error);}
    });
    runtime_.world().each<game::AudioSource>([&](auto entity,const auto& value){
        const auto key=value.asset+std::to_string(value.volume)+std::to_string(value.loop);
        if(!value.enabled||value.asset.empty()||sounds_.count(entity)||failed_.count(key))return;
        try{auto handle=audio_.load(assets_.resolveReference(value.asset),value.loop,value.volume);sounds_.emplace(entity,Sound{value.asset,runtime_.nodeId(entity),handle,value.volume,value.loop});
            if(value.autoplay){audio_.play(handle);++starts_;}
        }catch(const std::exception& error){failure(key,error);}
    });
}
AudioRuntime::Handle PresentationRuntime::requireAudioSource(ecs::Entity entity) {
    synchronize();
    const auto found = sounds_.find(entity);
    if (found == sounds_.end()) throw std::runtime_error("Entity has no playable audio source");
    return found->second.handle;
}
void PresentationRuntime::play(ecs::Entity entity) {
    // Explicit playback retries the current source; frame synchronization stays bounded.
    if (const auto* source=runtime_.world().tryGet<game::AudioSource>(entity))
        failed_.erase(source->asset+std::to_string(source->volume)+std::to_string(source->loop));
    audio_.play(requireAudioSource(entity));
    ++starts_;
}
void PresentationRuntime::stop(ecs::Entity entity) {
    audio_.stop(requireAudioSource(entity));
}
void PresentationRuntime::pause(ecs::Entity entity, bool paused) {
    audio_.setPaused(requireAudioSource(entity), paused);
}
const PresentationRuntime::Sound* PresentationRuntime::activeAudioSource(ecs::Entity entity) const {
    if (revision_ != runtime_.sceneRevision()) return nullptr;
    const auto found = sounds_.find(entity);
    const auto* source = runtime_.world().tryGet<game::AudioSource>(entity);
    if (found == sounds_.end() || !source || !source->enabled ||
        source->asset != found->second.asset || source->loop != found->second.loop ||
        runtime_.nodeId(entity) != found->second.node) return nullptr;
    return &found->second;
}
bool PresentationRuntime::playing(ecs::Entity entity) const {
    const auto* source = activeAudioSource(entity);
    return source && audio_.playing(source->handle);
}
double PresentationRuntime::audioPosition(ecs::Entity entity) const {
    const auto* source = activeAudioSource(entity);
    if (!source) return 0;
    return static_cast<double>(audio_.position(source->handle)) / audio_.description(source->handle).sampleRate;
}
void PresentationRuntime::update(double delta){
    if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Invalid presentation delta");
    synchronize();audio_.pauseAll(runtime_.state()==RuntimeLifecycle::State::Paused);frames_.clear();
    for(auto& item:animations_){auto& value=*runtime_.world().tryGet<game::Animator>(item.first);auto& animation=item.second;
        try{if(value.state!=animation.state){animation.machine.select(value.state,value.locomotion?value.crossfade:0);animation.state=value.state;}animation.machine.advance(delta,value.playbackRate);
            frames_.push_back({animation.node,animation.machine.clip(),animation.machine.time(),animation.machine.loop(),
                animation.machine.previousClip(),animation.machine.previousTime(),animation.machine.blend(),animation.machine.previousLoop(),{value.morph0,value.morph1}});
            auto& frame=frames_.back();frame.morphOverrides=morphOverridesFromJson(value.morphOverrides);
            if(value.locomotion&&animation.machine.locomotionProfile()){
                frame.samples=evaluateLocomotion(*animation.machine.locomotionProfile(),value.locomotionSpeed,
                    delta,animation.locomotion).samples;
                const auto dominant=std::max_element(frame.samples.begin(),frame.samples.end(),
                    [](const auto& a,const auto& b){return a.weight<b.weight;});
                frame.clip=dominant->clip;frame.time=dominant->time;frame.loop=true;frame.blend=1;
            }else animation.locomotion={};
            if(!value.expression.empty())frame.expression=std::make_shared<ExpressionPose>(expressionPoseFromJson(nlohmann::json::parse(value.expression)));
            frame.expressionWeight=value.expressionWeight;
        }catch(const std::exception& error){const auto key=animation.node+value.state;if(failed_.insert(key).second)errors_.push_back(error.what());}
    }
    if(!audio_.hasDevice()&&delta>0){const auto samples=audio_.mix(static_cast<std::uint32_t>(std::min(delta,0.25)*48000));for(float sample:samples)energy_+=sample*sample;}
}
std::string PresentationRuntime::uiDocument(){std::string result;runtime_.world().each<game::GameUiDocument>([&](auto,const auto& value){if(value.enabled&&result.empty())result=value.asset;});return result;}
}
