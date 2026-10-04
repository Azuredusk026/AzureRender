#pragma once
#include "runtime/InputActions.hpp"
#include "runtime/GameComponents.hpp"
#include "runtime/PhysicsWorld.hpp"
#include "runtime/ThirdPersonCamera.hpp"
#include <functional>
namespace azurerender {
class GameRuntime {
public:
    explicit GameRuntime(RuntimeLifecycle& runtime) : runtime_(runtime) {}
    InputActions& input() noexcept { return input_; }
    PhysicsWorld& physics() noexcept { return physics_; }
    double simulationMilliseconds() const noexcept { return simulationMilliseconds_; }
    std::uint64_t steps() const noexcept { return steps_; }
    void setEventHandler(std::function<void(const PhysicsEvent&)> handler) { eventHandler_ = std::move(handler); }
    void setBeforeStep(std::function<void(double)> handler) { beforeStep_ = std::move(handler); }
    double advance(double delta);
    void move(ecs::Entity entity, CharacterMotion motion) { motions_[entity] = motion; }
    void setCameraYaw(float yaw) { cameraYaw_ = yaw; }
    ThirdPersonCamera& camera() { return camera_; }
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
    double accumulator_ = 0;
    std::uint64_t steps_ = 0;
    std::function<void(const PhysicsEvent&)> eventHandler_;
    std::function<void(double)> beforeStep_;
    ThirdPersonCamera camera_;
    float cameraYaw_ = 0;
    bool hasCamera_ = false;
    std::uint64_t sceneRevision_ = 0;
    game::ThirdPersonCamera cameraSettings_;
    std::map<ecs::Entity,std::array<float,3>> velocities_;
    std::map<std::string,ecs::TransformComponent> previousTransforms_;
    std::array<float,3> previousCameraPosition_{}, previousCameraTarget_{};
    bool interpolationReady_ = false;
    float renderAlpha() const;
    void updateCamera(float dt);
};
}
