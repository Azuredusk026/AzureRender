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
    void stop(ecs::Entity entity);
    void pause(ecs::Entity entity, bool paused);
    bool playing(ecs::Entity entity) const;
    double audioPosition(ecs::Entity entity) const;
    const std::vector<NodeAnimationFrame>& animations() const { return frames_; }
    const std::vector<std::string>& errors() const { return errors_; }
    std::string uiDocument();
    std::uint64_t audioStarts() const { return starts_; }
    std::size_t soundCount() const { return sounds_.size(); }
    double mixedEnergy() const { return energy_; }
    bool hasAudioDevice() const { return audio_.hasDevice(); }
private:
    void synchronize();
    AudioRuntime::Handle requireAudioSource(ecs::Entity entity);
    RuntimeLifecycle& runtime_;
    AssetDatabase& assets_;
    AudioRuntime audio_;
    struct Animation { std::string asset,state,node;AnimationStateMachine machine;LocomotionBlendState locomotion; };
    struct Sound { std::string asset,node;AudioRuntime::Handle handle;float volume;bool loop; };
    const Sound* activeAudioSource(ecs::Entity entity) const;
    std::map<ecs::Entity,Animation> animations_;
    std::map<ecs::Entity,Sound> sounds_;
    std::set<std::string> failed_;
    std::vector<NodeAnimationFrame> frames_;
    std::vector<std::string> errors_;
    std::uint64_t revision_=0,starts_=0;
    double energy_=0;
};
}
