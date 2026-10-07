#pragma once
#include "runtime/IScriptRuntime.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/GameRuntime.hpp"
#include <memory>
namespace azurerender {
class ManagedScriptRuntime final:public IScriptRuntime {
public:
    ManagedScriptRuntime(RuntimeLifecycle&,GameRuntime&,AssetDatabase&,const nlohmann::json&);
    ~ManagedScriptRuntime() override;
    void initialize() override;
    void fixedStep(double) override;
    void dispatch(const PhysicsEvent&) override;
    void dispatchInteraction(const InteractionTarget&) override;
    void reloadChanged() override;
    void shutdown() noexcept override;
    void setLevelHandler(std::function<void(std::string)>) override;
    void setAudioHandler(std::function<void(ecs::Entity)>) override;
    void setUiHandler(std::function<void(std::string,std::string)>) override;
    std::size_t activeCount() const override;
    const std::vector<std::string>& errors() const override;
    ScriptRuntimeCapabilities capabilities() const override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
