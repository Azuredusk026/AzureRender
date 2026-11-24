#pragma once
#include "runtime/IScriptRuntime.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/GameRuntime.hpp"
#include <functional>
#include <memory>
namespace azurerender {
class ScriptRuntime final:public IScriptRuntime {
public:
    ScriptRuntime(RuntimeLifecycle& runtime, GameRuntime& game, AssetDatabase& assets);
    ~ScriptRuntime() override;
    void initialize() override;
    void fixedStep(double delta) override;
    void dispatch(const PhysicsEvent& event) override;
    void dispatchInteraction(const InteractionTarget& event) override;
    void reloadChanged() override;
    void shutdown() noexcept override;
    void setLevelHandler(std::function<void(std::string)> handler) override;
    void setAudioHandler(std::function<void(ecs::Entity)> handler) override;
    void setUiHandler(std::function<void(std::string,std::string)> handler) override;
    std::size_t activeCount() const override;
    const std::vector<std::string>& errors() const override;
    ScriptRuntimeCapabilities capabilities() const override {return {"lua",true,false};}
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
