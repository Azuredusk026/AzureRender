#pragma once
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/InputActions.hpp"
#include "runtime/PhysicsWorld.hpp"
namespace azurerender {
class GameRuntime;
struct RuntimeSystemContext {
    RuntimeLifecycle& runtime;
    InputActions& input;
    PhysicsWorld& physics;
    GameRuntime& game;
    double fixedStep;
};
class IRuntimeSystem {
public:
    virtual ~IRuntimeSystem() = default;
    virtual void initialize(RuntimeSystemContext&) {}
    virtual void fixedStep(RuntimeSystemContext&) = 0;
    virtual void sceneChanged(RuntimeSystemContext&) {}
    virtual void shutdown(RuntimeSystemContext&) noexcept {}
};
}
