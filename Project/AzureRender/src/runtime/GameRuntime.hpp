#pragma once
#include "runtime/InputActions.hpp"
#include "runtime/GameComponents.hpp"
#include "runtime/PhysicsWorld.hpp"
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
};
}
