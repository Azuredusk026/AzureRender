#pragma once
#include "runtime/InputActions.hpp"
#include "runtime/GameComponents.hpp"
#include "runtime/PhysicsWorld.hpp"
#include "runtime/ThirdPersonCamera.hpp"
#include "runtime/InteractionRuntime.hpp"
#include "runtime/SystemRegistry.hpp"
#include <functional>
namespace azurerender {
class GameRuntime {
public:
    explicit GameRuntime(RuntimeLifecycle& runtime);
    GameRuntime(RuntimeLifecycle& runtime, const SystemRegistry& registry, const nlohmann::json& config);
    ~GameRuntime();
    InputActions& input() noexcept { return input_; }
    PhysicsWorld& physics() noexcept { return physics_; }
    double simulationMilliseconds() const noexcept { return simulationMilliseconds_; }
    const std::vector<double>& lastStepSamples() const noexcept { return lastStepSamples_; }
    std::uint64_t steps() const noexcept { return steps_; }
    void setEventHandler(std::function<void(const PhysicsEvent&)> handler) { eventHandler_ = std::move(handler); }
    void setBeforeStep(std::function<void(double)> handler) { beforeStep_ = std::move(handler); }
    void setInteractionHandler(std::function<void(const InteractionTarget&)> handler){interactionHandler_=std::move(handler);}
    const std::optional<InteractionTarget>& interactionTarget() const{return interactions_.target();}
    double advance(double delta);
    void move(ecs::Entity entity, CharacterMotion motion) { motions_[entity] = motion; }
    void stageMotion(ecs::Entity entity, CharacterMotion motion) { if(!motions_.count(entity))motions_[entity]=motion; }
    const std::map<ecs::Entity, CharacterMotion>& motions() const { return motions_; }
    void physicsEvents(std::vector<PhysicsEvent> events) { events_=std::move(events); }
    void setInteractionPolicy(InteractionPolicy policy) { interactions_.setPolicy(std::move(policy)); }
    void selectInteraction(ecs::Entity actor, const std::string& action);
    void setCameraYaw(float yaw) { cameraYaw_ = yaw; }
    ThirdPersonCamera& camera() { return camera_; }
    float cameraYaw() const { return cameraYaw_; }
    void activateCamera(const game::ThirdPersonCamera& settings, std::array<float,3> target);
    void deactivateCamera() { hasCamera_=false; }
    void updateCamera(std::array<float,3> target, double dt, ecs::Entity ignored=ecs::kInvalidEntity);
    bool hasCamera() const { return hasCamera_; }
    void cameraInput(float x, float y, float scroll);
    SceneDocument renderScene();
    std::array<float,3> renderCameraPosition() const;
    std::array<float,3> renderCameraTarget() const;
private:
    std::map<ecs::Entity, CharacterMotion> motions_;
    RuntimeLifecycle& runtime_;
    InputActions input_;
    PhysicsWorld physics_;
    double simulationMilliseconds_ = 0;
    std::vector<double> lastStepSamples_;
    double accumulator_ = 0;
    std::uint64_t steps_ = 0;
    std::function<void(const PhysicsEvent&)> eventHandler_;
    std::function<void(double)> beforeStep_;
    ThirdPersonCamera camera_;
    float cameraYaw_ = 0;
    bool hasCamera_ = false;
    std::uint64_t sceneRevision_ = 0;
    bool sceneReady_ = false;
    game::ThirdPersonCamera cameraSettings_;
    std::vector<std::unique_ptr<IRuntimeSystem>> systems_;
    std::vector<PhysicsEvent> events_;
    std::map<std::string,ecs::TransformComponent> previousTransforms_;
    std::array<float,3> previousCameraPosition_{}, previousCameraTarget_{};
    bool interpolationReady_ = false;
    float renderAlpha() const;
    InteractionRuntime interactions_;
    std::function<void(const InteractionTarget&)> interactionHandler_;
    RuntimeSystemContext systemContext();
    void synchronizeSystemScene();
};
}
