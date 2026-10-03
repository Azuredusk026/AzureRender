#pragma once
#include "runtime/RuntimeLifecycle.hpp"
#include <array>
#include <map>
#include <memory>
#include <optional>
namespace azurerender {
struct PhysicsEvent { ecs::Entity trigger, other; bool entered; };
struct RayHit { ecs::Entity entity; float fraction; };
struct CharacterMotion { float x = 0, z = 0; bool jump = false; };
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    std::vector<PhysicsEvent> step(RuntimeLifecycle& runtime, float dt, const std::map<ecs::Entity, CharacterMotion>& motion);
    bool contains(ecs::Entity entity) const;
    bool grounded(ecs::Entity entity) const;
    std::optional<RayHit> raycast(const std::array<float, 3>& origin, const std::array<float, 3>& displacement) const;
private:
    struct Impl;
    std::uint64_t sceneRevision_ = 0;
    std::unique_ptr<Impl> impl_;
};
}
