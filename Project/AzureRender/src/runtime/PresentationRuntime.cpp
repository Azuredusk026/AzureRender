#include "runtime/PresentationRuntime.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
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
        const auto key=value?value->asset+std::to_string(value->volume)+std::to_string(value->loop):"";
        if(!value||!value->enabled||runtime_.nodeId(it->first)!=it->second.node||key!=it->second.key){audio_.release(it->second.handle);it=sounds_.erase(it);}else ++it;
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
            animations_.emplace(entity,Animation{value.asset,value.state,runtime_.nodeId(entity),std::move(machine)});
        }catch(const std::exception& error){failure(key,error);}
    });
    runtime_.world().each<game::AudioSource>([&](auto entity,const auto& value){
        const auto key=value.asset+std::to_string(value.volume)+std::to_string(value.loop);
        if(!value.enabled||value.asset.empty()||sounds_.count(entity)||failed_.count(key))return;
        try{auto handle=audio_.load(assets_.resolveReference(value.asset),value.loop,value.volume);sounds_.emplace(entity,Sound{key,runtime_.nodeId(entity),handle});
            if(value.autoplay){audio_.play(handle);++starts_;}
        }catch(const std::exception& error){failure(key,error);}
    });
}
void PresentationRuntime::play(ecs::Entity entity){synchronize();auto found=sounds_.find(entity);if(found==sounds_.end())throw std::runtime_error("Entity has no playable audio source");audio_.play(found->second.handle);++starts_;}
void PresentationRuntime::update(double delta){
    if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Invalid presentation delta");
    synchronize();audio_.pauseAll(runtime_.state()==RuntimeLifecycle::State::Paused);frames_.clear();
    for(auto& item:animations_){auto& value=*runtime_.world().tryGet<game::Animator>(item.first);auto& animation=item.second;
        try{if(value.state!=animation.state){animation.machine.select(value.state,value.locomotion?value.crossfade:0);animation.state=value.state;}animation.machine.advance(delta,value.playbackRate);
            frames_.push_back({animation.node,animation.machine.clip(),animation.machine.time(),animation.machine.loop(),
                animation.machine.previousClip(),animation.machine.previousTime(),animation.machine.blend(),animation.machine.previousLoop(),{value.morph0,value.morph1}});
        }catch(const std::exception& error){const auto key=animation.node+value.state;if(failed_.insert(key).second)errors_.push_back(error.what());}
    }
    if(!audio_.hasDevice()&&delta>0){const auto samples=audio_.mix(static_cast<std::uint32_t>(std::min(delta,0.25)*48000));for(float sample:samples)energy_+=sample*sample;}
}
std::string PresentationRuntime::uiDocument(){std::string result;runtime_.world().each<game::GameUiDocument>([&](auto,const auto& value){if(value.enabled&&result.empty())result=value.asset;});return result;}
}
