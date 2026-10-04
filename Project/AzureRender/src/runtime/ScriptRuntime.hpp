#pragma once
#include "runtime/AssetDatabase.hpp"
#include "runtime/GameRuntime.hpp"
#include <functional>
#include <memory>
namespace azurerender {
class ScriptRuntime {
public:
    ScriptRuntime(RuntimeLifecycle& runtime, GameRuntime& game, AssetDatabase& assets);
    ~ScriptRuntime();
    void update(double delta);
    void dispatch(const PhysicsEvent& event);
    void dispatchInteraction(const InteractionTarget& event);
    void reloadChanged();
    void setLevelHandler(std::function<void(std::string)> handler);
    void setAudioHandler(std::function<void(ecs::Entity)> handler);
    void setUiHandler(std::function<void(std::string,std::string)> handler);
    std::size_t activeCount() const;
    const std::vector<std::string>& errors() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
