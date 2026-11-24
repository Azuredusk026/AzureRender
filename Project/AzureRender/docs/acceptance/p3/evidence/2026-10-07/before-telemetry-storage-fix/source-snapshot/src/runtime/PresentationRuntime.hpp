#pragma once
#include "runtime/AnimationStateMachine.hpp"
#include "runtime/AudioRuntime.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/GameComponents.hpp"
#include "render/RenderContext.hpp"
#include <set>
namespace azurerender {
class PresentationRuntime {
public:
    PresentationRuntime(RuntimeLifecycle& runtime,AssetDatabase& assets,bool device=true);
    ~PresentationRuntime();
    void update(double delta);
    void play(ecs::Entity entity);
    const std::vector<NodeAnimationFrame>& animations() const { return frames_; }
    const std::vector<std::string>& errors() const { return errors_; }
    std::string uiDocument();
    std::uint64_t audioStarts() const { return starts_; }
    std::size_t soundCount() const { return sounds_.size(); }
    double mixedEnergy() const { return energy_; }
    bool hasAudioDevice() const { return audio_.hasDevice(); }
private:
    void synchronize();
    RuntimeLifecycle& runtime_;
    AssetDatabase& assets_;
    AudioRuntime audio_;
    struct Animation { std::string asset,state,node;AnimationStateMachine machine; };
    struct Sound { std::string key,node;AudioRuntime::Handle handle; };
    std::map<ecs::Entity,Animation> animations_;
    std::map<ecs::Entity,Sound> sounds_;
    std::set<std::string> failed_;
    std::vector<NodeAnimationFrame> frames_;
    std::vector<std::string> errors_;
    std::uint64_t revision_=0,starts_=0;
    double energy_=0;
};
}
