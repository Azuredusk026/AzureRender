#include "scripting/ScriptBindingHost.hpp"
#include "runtime/ComponentCodec.hpp"
#include <cmath>
#include <stdexcept>
namespace azurerender {
using Json=nlohmann::json;
void ScriptBindingHost::checkThread() const {
    if(std::this_thread::get_id()!=owner_)throw std::logic_error("Script host called outside owner thread");
}
ScriptObject ScriptBindingHost::object(const std::string& node) {
    checkThread();if(closed_)throw std::logic_error("Script binding host is closed");
    const auto entity=runtime_.entity(node);
    return {runtime_.world().handle(entity),node,runtime_.sceneRevision()};
}
bool ScriptBindingHost::valid(const ScriptObject& value) {
    checkThread();return !closed_ && value.revision==runtime_.sceneRevision()
        && runtime_.world().valid(value.identity) && runtime_.entity(value.node)==value.identity.entity;
}
void ScriptBindingHost::guard(const ScriptObject& value,bool mutating) {
    if(!valid(value))throw std::runtime_error("Script entity handle is stale");
    if(mutating && !callback_)throw std::runtime_error("Gameplay mutation requires a runtime callback");
}
void ScriptBindingHost::beginCallback(bool initialization) {
    checkThread();if(closed_ || callback_)throw std::logic_error("Script callback lifecycle violation");
    if(initialization && pending_)throw std::logic_error("Script initialization already pending");
    callback_=true;staging_=initialization;
    if(initialization){cancelInitialization();callback_=true;staging_=true;pending_=true;}
}
void ScriptBindingHost::endCallback(bool success) {
    checkThread();if(!callback_)throw std::logic_error("Script callback is not active");
    const bool initialization=staging_;callback_=false;staging_=false;
    if(initialization && !success)cancelInitialization();
}
void ScriptBindingHost::cancelInitialization() noexcept {
    stagedComponents_.clear();stagedEffects_.clear();stagedStructural_.clear();stagedSpawns_.clear();pending_=false;staging_=false;
}
void ScriptBindingHost::commitInitialization(const std::function<void()>& retirement) {
    checkThread();if(!pending_ || callback_ || closed_)throw std::logic_error("No completed script initialization");
    auto& components=runtimeComponentRegistry();
    for(const auto& entry:stagedComponents_) {
        guard(entry.second.first,false);
        if(!components.contains(entry.first.second,runtime_.world(),entry.first.first))
            throw std::runtime_error("Staged script component is unavailable");
        components.validate(entry.first.second,entry.second.second);
    }
    std::map<Key,Json> originals;
    for(const auto& entry:stagedComponents_)
        originals.emplace(entry.first,components.encode(entry.first.second,runtime_.world(),entry.first.first));
    try {
        for(const auto& entry:stagedComponents_)
            components.install(entry.first.second,runtime_.world(),entry.first.first,entry.second.second);
        runtime_.transactionalQueue([&] {
        for(const auto& operation:stagedStructural_)operation();
        if(!stagedEffects_.empty()) {
            auto effects=stagedEffects_;auto* lifecycle=&runtime_;const auto revision=runtime_.sceneRevision();
            runtime_.defer([effects=std::move(effects),lifecycle,revision](auto&) {
                if(lifecycle->sceneRevision()!=revision)return;
                for(const auto& operation:effects)operation();
            });
        }
        });
    }catch(...) {
        for(const auto& entry:originals)components.install(entry.first.second,runtime_.world(),entry.first.first,entry.second);
        cancelInitialization();throw;
    }
    auto committed=std::move(stagedComponents_);cancelInitialization();
    if(retirement) {
        std::exception_ptr failure;
        try{retirement();}catch(...){failure=std::current_exception();}
        // Retirement starts only after a valid commit. Candidate component
        // values retain priority over the retired instance's cleanup writes.
        for(const auto& entry:committed) {
            guard(entry.second.first,false);
            components.install(entry.first.second,runtime_.world(),entry.first.first,entry.second.second);
        }
        if(failure)std::rethrow_exception(failure);
    }
}
void ScriptBindingHost::close() noexcept {
    cancelInitialization();callback_=false;closed_=true;level_={};audio_={};ui_={};
}
void ScriptBindingHost::effect(std::function<void()> action,bool structural) {
    if(staging_)(structural?stagedStructural_:stagedEffects_).push_back(std::move(action));else action();
}
void ScriptBindingHost::setLevelHandler(std::function<void(std::string)> handler) {checkThread();level_=std::move(handler);}
void ScriptBindingHost::setAudioHandler(std::function<void(ecs::Entity)> handler) {checkThread();audio_=std::move(handler);}
void ScriptBindingHost::setUiHandler(std::function<void(std::string,std::string)> handler) {checkThread();ui_=std::move(handler);}
Json ScriptBindingHost::encode(const ScriptObject& object) {
    return {{"entity",object.identity.entity},{"generation",object.identity.generation},
            {"node",object.node},{"revision",object.revision}};
}
ScriptObject ScriptBindingHost::decode(const Json& value) {
    if(!value.is_object() || value.size()!=4 || !value.at("entity").is_number_unsigned()
        || !value.at("generation").is_number_unsigned() || !value.at("revision").is_number_unsigned()
        || !value.at("node").is_string())throw std::invalid_argument("Invalid script object identity");
    const auto entity=value.at("entity").get<std::uint64_t>();
    if(!entity || entity>std::numeric_limits<ecs::Entity>::max())throw std::invalid_argument("Invalid entity range");
    return {{static_cast<ecs::Entity>(entity),value.at("generation").get<std::uint64_t>()},
            value.at("node").get<std::string>(),value.at("revision").get<std::uint64_t>()};
}
Json ScriptBindingHost::invoke(const ScriptObject& object,const std::string& method,const Json& args) {
    checkThread();
    if(method=="id") {
        if(!args.is_array() || !args.empty())throw std::invalid_argument("Script identity is read only");
        guard(object,false);return object.node;
    }
    const auto& binding=scriptBinding(method);validateScriptArguments(binding,args);
    if(method=="alive")return valid(object);
    guard(object,binding.mutating);
    const auto entity=object.identity.entity;
    auto& components=runtimeComponentRegistry();
    if(method=="find") {
        const auto node=args[0].get<std::string>();
        return runtime_.entity(node)==ecs::kInvalidEntity?Json(nullptr):encode(this->object(node));
    }
    if(method=="interaction_target") {
        const auto& target=game_.interactionTarget();
        return target && target->revision==object.revision && target->actor==entity
            && runtime_.entity(target->node)==target->target ? target->node : std::string();
    }
    if(method=="has")return components.contains(args[0].get<std::string>(),runtime_.world(),entity);
    if(method=="action")return game_.input().down(args[0].get<std::string>());
    if(method=="pressed")return game_.input().pressed(args[0].get<std::string>());
    if(method=="get" || method=="set") {
        const auto type=args[0].get<std::string>(),field=args[1].get<std::string>();
        const Key key{entity,type};
        auto data=staging_ && stagedComponents_.count(key)?stagedComponents_.at(key).second
            :components.encode(type,runtime_.world(),entity);
        if(method=="get")return data.at("data").at(field);
        components.validateWrite(type,field,args[2],false);data["data"][field]=args[2];components.validate(type,data);
        if(staging_)stagedComponents_[key]={object,std::move(data)};
        else components.install(type,runtime_.world(),entity,data);
    } else if(method=="move") {
        const float x=args[0].get<float>(),z=args[1].get<float>();const bool jump=args[2].get<bool>();
        if(!std::isfinite(x) || !std::isfinite(z))throw std::invalid_argument("Character motion must fit finite floats");
        auto* game=&game_;effect([game,entity,x,z,jump]{game->move(entity,{x,z,jump});});
    } else if(method=="spawn") {
        SceneNode node;node.id=args[0].get<std::string>();node.resourceId=args[1].get<std::string>();node.visible=!node.resourceId.empty();
        ecs::TransformComponent defaults;const auto& registry=components.metadata();
        auto data=registry.encode("azure.transform",&defaults);data["data"]["translation"]=args[2];
        registry.decode("azure.transform",&defaults,data);node.translation=defaults.translation;
        runtime_.validateSpawn(node);
        if(staging_ && !stagedSpawns_.insert(node.id).second)throw std::invalid_argument("Duplicate staged spawn identity");
        auto* lifecycle=&runtime_;effect([lifecycle,node]{lifecycle->deferSpawn(node);},true);
    } else if(method=="destroy" || method=="remove_component") {
        const auto type=method=="remove_component"?args[0].get<std::string>():std::string();
        if(!type.empty()) {
            if(type=="azure.transform" || type=="azure.renderable")throw std::invalid_argument("Structural identity components are required");
            static_cast<void>(components.describe(type));
        }
        auto* lifecycle=&runtime_;
        effect([lifecycle,object,type,method]{
            lifecycle->defer([lifecycle,object,type,method](auto& world){
                if(lifecycle->sceneRevision()!=object.revision || !world.valid(object.identity)
                    || lifecycle->entity(object.node)!=object.identity.entity)return;
                if(method=="destroy")world.destroyEntity(object.identity.entity);
                else runtimeComponentRegistry().remove(type,world,object.identity.entity);
            });
        },true);
    } else if(method=="load_level") {
        if(!level_)throw std::runtime_error("Level request handler unavailable");
        const auto reference=args[0].get<std::string>();effect([handler=level_,reference]{handler(reference);});
    } else if(method=="audio_play") {
        if(!audio_)throw std::runtime_error("Audio handler unavailable");effect([handler=audio_,entity]{handler(entity);});
    } else if(method=="ui_text") {
        if(!ui_)throw std::runtime_error("UI handler unavailable");
        const auto id=args[0].get<std::string>(),text=args[1].get<std::string>();effect([handler=ui_,id,text]{handler(id,text);});
    } else throw std::logic_error("Binding description has no host operation: "+method);
    return nullptr;
}
}
