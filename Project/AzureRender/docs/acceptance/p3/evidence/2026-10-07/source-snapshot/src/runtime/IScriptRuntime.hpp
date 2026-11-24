#pragma once
#include "ecs/Entity.hpp"
#include <functional>
#include <string>
#include <vector>
namespace azurerender {
struct PhysicsEvent;
struct InteractionTarget;
struct ScriptRuntimeCapabilities {
    std::string backend;
    bool hotReload=false;
    bool trustedCompiledExtension=false;
};
class IScriptRuntime {
public:
    virtual ~IScriptRuntime()=default;
    virtual void initialize()=0;
    virtual void fixedStep(double delta)=0;
    void update(double delta) {fixedStep(delta);}
    virtual void dispatch(const PhysicsEvent&)=0;
    virtual void dispatchInteraction(const InteractionTarget&)=0;
    virtual void reloadChanged()=0;
    virtual void shutdown() noexcept=0;
    virtual void setLevelHandler(std::function<void(std::string)>)=0;
    virtual void setAudioHandler(std::function<void(ecs::Entity)>)=0;
    virtual void setUiHandler(std::function<void(std::string,std::string)>)=0;
    virtual std::size_t activeCount() const=0;
    virtual const std::vector<std::string>& errors() const=0;
    virtual ScriptRuntimeCapabilities capabilities() const=0;
};
}
