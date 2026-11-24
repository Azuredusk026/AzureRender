#pragma once
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/physics/PhysicsDescriptors.hpp"
#include <array>
#include <map>
#include <memory>
#include <optional>
namespace azurerender {
struct PhysicsEvent { ecs::Entity trigger, other; bool entered; };
struct RayHit { ecs::Entity entity; float fraction; };
struct CharacterMotion { float x = 0, z = 0; bool jump = false; bool velocity = false; };
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
    std::optional<RayHit> sphereSweep(const std::array<float,3>& origin, const std::array<float,3>& displacement,
        float radius, ecs::Entity ignore = ecs::kInvalidEntity) const;
    std::array<float,3> velocity(ecs::Entity entity) const;
    // World-space displacement / dt from the last completed physics step.
    // External transform corrections are excluded; velocity() retains the
    // controller's integration velocity. No simulation step leaves this sample intact.
    std::array<float,3> actualVelocity(ecs::Entity entity) const;
    std::array<float,3> groundVelocity(ecs::Entity entity) const;
    void setMeshResolver(CollisionMeshResolver resolver){meshResolver_=std::move(resolver);}
    void validateScene(RuntimeLifecycle&) const;
    std::size_t constraintCount() const;
    std::optional<float> hingeAngle(ecs::Entity) const;
private:
    CollisionMeshResolver meshResolver_;
    struct Impl;
    std::uint64_t sceneRevision_ = 0;
    std::unique_ptr<Impl> impl_;
};
}
