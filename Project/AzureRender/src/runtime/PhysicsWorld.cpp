#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include "runtime/PhysicsWorld.hpp"
#include "runtime/GameComponents.hpp"
#include <cmath>
#include <mutex>
#include <set>
namespace azurerender {
namespace {
JPH::Vec3 vec(const std::array<float, 3>& value) { return {value[0], value[1], value[2]}; }
std::array<float, 3> array(JPH::Vec3Arg value) { return {value.GetX(), value.GetY(), value.GetZ()}; }
JPH::Quat rotation(const std::array<float, 3>& value) {
    constexpr float radians = 0.017453292519943295F;
    return JPH::Quat::sEulerAngles(vec(value) * radians);
}
struct JoltRegistration {
    JoltRegistration() { JPH::RegisterDefaultAllocator(); JPH::Factory::sInstance = new JPH::Factory(); JPH::RegisterTypes(); }
    ~JoltRegistration() { JPH::UnregisterTypes(); delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr; }
};
void initializeJolt() { static JoltRegistration registration; static_cast<void>(registration); }
}
struct PhysicsWorld::Impl {
    JPH::BroadPhaseLayerInterfaceTable broad{2, 2};
    JPH::ObjectLayerPairFilterTable pairs{2};
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> filter;
    JPH::PhysicsSystem system;
    JPH::TempAllocatorImpl allocator{16 * 1024 * 1024};
    JPH::JobSystemSingleThreaded jobs{1024};
    struct Body { JPH::BodyID id; std::string node; game::RigidBody settings; std::array<float, 3> scale; };
    struct Character { JPH::Ref<JPH::CharacterVirtual> value; std::string node; game::Character settings; float groundAge = 1, jumpBuffer = 0; bool jumping = false; };
    std::map<ecs::Entity, Body> bodies;
    std::map<ecs::Entity, Character> characters;
    std::set<std::pair<ecs::Entity, ecs::Entity>> contacts;
    Impl() {
        broad.MapObjectToBroadPhaseLayer(0, JPH::BroadPhaseLayer(0)); broad.MapObjectToBroadPhaseLayer(1, JPH::BroadPhaseLayer(1));
        pairs.EnableCollision(0, 1); pairs.EnableCollision(1, 1);
        filter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(broad, 2, pairs, 2);
        system.Init(4096, 0, 4096, 4096, broad, *filter, pairs);
    }
    void removeBody(ecs::Entity entity) {
        const auto id = bodies.at(entity).id; system.GetBodyInterface().RemoveBody(id); system.GetBodyInterface().DestroyBody(id); bodies.erase(entity);
    }
    ~Impl() { characters.clear(); while (!bodies.empty()) removeBody(bodies.begin()->first); }
};
PhysicsWorld::PhysicsWorld() { initializeJolt(); impl_ = std::make_unique<Impl>(); }
PhysicsWorld::~PhysicsWorld() = default;
std::vector<PhysicsEvent> PhysicsWorld::step(RuntimeLifecycle& runtime, float dt, const std::map<ecs::Entity, CharacterMotion>& motions) {
    if (!std::isfinite(dt) || dt <= 0) throw std::invalid_argument("Physics dt must be finite and positive");
    if (sceneRevision_ != runtime.sceneRevision()) {
        impl_ = std::make_unique<Impl>(); sceneRevision_ = runtime.sceneRevision();
    }
    auto& p = *impl_; auto& world = runtime.world(); auto& bi = p.system.GetBodyInterface();
    for (auto it = p.characters.begin(); it != p.characters.end();) {
        if (!world.has<game::Character>(it->first) || runtime.nodeId(it->first) != it->second.node) it = p.characters.erase(it); else ++it;
    }
    std::vector<ecs::Entity> deleted;
    for (const auto& body : p.bodies) if (!world.has<game::RigidBody>(body.first) || runtime.nodeId(body.first) != body.second.node) deleted.push_back(body.first);
    for (auto entity : deleted) p.removeBody(entity);
    world.each<ecs::TransformComponent, game::RigidBody>([&](auto entity, auto& transform, const auto& settings) {
        if (world.has<game::Character>(entity)) throw std::invalid_argument("Character and rigid body are mutually exclusive");
        for (unsigned i = 0; i < 3; ++i) if (!std::isfinite(settings.halfExtent[i]) || settings.halfExtent[i] <= 0 || transform.scale[i] <= 0)
            throw std::invalid_argument("Invalid rigid body extent");
        auto existing = p.bodies.find(entity);
        if (existing != p.bodies.end() && (existing->second.settings.halfExtent != settings.halfExtent
            || existing->second.settings.dynamic != settings.dynamic || existing->second.settings.trigger != settings.trigger || existing->second.scale != transform.scale)) {
            p.removeBody(entity); existing = p.bodies.end();
        }
        if (existing == p.bodies.end()) {
            const JPH::Vec3 extent = vec(settings.halfExtent) * vec(transform.scale);
            JPH::BodyCreationSettings create(new JPH::BoxShape(extent, std::min(0.05F, extent.ReduceMin() * 0.5F)), JPH::RVec3(vec(transform.translation)), rotation(transform.rotation),
                settings.dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static, settings.dynamic || settings.trigger ? 1 : 0);
            create.mIsSensor = settings.trigger;
            const auto id = bi.CreateAndAddBody(create, JPH::EActivation::Activate);
            if (id.IsInvalid()) throw std::runtime_error("Jolt body capacity exceeded");
            bi.SetUserData(id, entity); p.bodies.emplace(entity, Impl::Body{id, runtime.nodeId(entity), settings, transform.scale});
        } else if (!settings.dynamic) bi.SetPositionAndRotation(existing->second.id, JPH::RVec3(vec(transform.translation)), rotation(transform.rotation), JPH::EActivation::DontActivate);
    });
    world.each<ecs::TransformComponent, game::Character>([&](auto entity, auto& transform, const auto& settings) {
        if (!(settings.radius > 0 && settings.halfHeight > 0)) throw std::invalid_argument("Invalid character shape");
        auto found = p.characters.find(entity);
        if (found != p.characters.end() && (found->second.settings.radius != settings.radius || found->second.settings.halfHeight != settings.halfHeight
            || found->second.settings.centerOffset != settings.centerOffset || found->second.settings.maximumSlope != settings.maximumSlope)) {
            p.characters.erase(found); found = p.characters.end();
        }
        if (found == p.characters.end()) {
            JPH::CharacterVirtualSettings create;
            create.mShape = new JPH::CapsuleShape(settings.halfHeight, settings.radius);
            create.mInnerBodyShape = create.mShape; create.mInnerBodyLayer = 1;
            create.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -settings.radius);
            create.mMaxSlopeAngle = JPH::DegreesToRadians(settings.maximumSlope);
            auto center = transform.translation; center[1] += settings.centerOffset;
            JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(&create, JPH::RVec3(vec(center)), JPH::Quat::sIdentity(), entity, &p.system);
            found = p.characters.emplace(entity, Impl::Character{character, runtime.nodeId(entity), settings}).first;
        }
        auto& character = *found->second.value;
        auto center = transform.translation; center[1] += settings.centerOffset;
        const auto difference = JPH::Vec3(character.GetPosition()) - vec(center);
        if (difference.LengthSq() > 0.000001F) character.SetPosition(JPH::RVec3(vec(center)));
        const auto requested = motions.find(entity); const CharacterMotion move = requested == motions.end() ? CharacterMotion{} : requested->second;
        const float length = std::sqrt(move.x * move.x + move.z * move.z), denominator = std::max(1.0F, length);
        float vertical = character.GetLinearVelocity().GetY();
        const bool onGround = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
        auto& state = found->second;
        state.groundAge = onGround ? 0 : state.groundAge + dt;
        if (onGround && vertical <= 0) state.jumping = false;
        state.jumpBuffer = move.jump ? .1F : std::max(0.0F,state.jumpBuffer-dt);
        if (onGround && vertical < 0) vertical = 0;
        if (!state.jumping && state.groundAge <= .1F && state.jumpBuffer > 0) {
            vertical = settings.jumpSpeed; state.jumpBuffer = 0; state.groundAge = 1; state.jumping = true;
        }
        vertical -= 9.81F * dt;
        const float speed = move.velocity ? 1 : settings.speed / denominator;
        character.SetLinearVelocity({move.x * speed, vertical, move.z * speed});
        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        update.mWalkStairsStepUp = {0,settings.stepHeight,0};
        if (vertical > 0) update.mStickToFloorStepDown = JPH::Vec3::sZero();
        character.ExtendedUpdate(dt, p.system.GetGravity(), update, p.system.GetDefaultBroadPhaseLayerFilter(1), p.system.GetDefaultLayerFilter(1), {}, {}, p.allocator);
        transform.translation = array(JPH::Vec3(character.GetPosition()));
        transform.translation[1] -= settings.centerOffset;
    });
    const auto result = p.system.Update(dt, 1, &p.allocator, &p.jobs);
    if (result != JPH::EPhysicsUpdateError::None) throw std::runtime_error("Jolt update capacity exceeded");
    for (const auto& body : p.bodies) if (body.second.settings.dynamic) {
        auto* transform = world.tryGet<ecs::TransformComponent>(body.first);
        transform->translation = array(JPH::Vec3(bi.GetPosition(body.second.id)));
        transform->rotation = array(bi.GetRotation(body.second.id).GetEulerAngles() * 57.295779513F);
    }
    // Sensors use Jolt narrow-phase overlap queries, including virtual-character inner bodies.
    std::set<std::pair<ecs::Entity, ecs::Entity>> contacts;
    for (const auto& sensor : p.bodies) if (sensor.second.settings.trigger) {
        const auto shape = bi.GetTransformedShape(sensor.second.id);
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
        p.system.GetNarrowPhaseQuery().CollideShape(shape.mShape, shape.GetShapeScale(), shape.GetCenterOfMassTransform(), {}, JPH::RVec3::sZero(), hits);
        for (const auto& hit : hits.mHits) {
            const auto other = static_cast<ecs::Entity>(bi.GetUserData(hit.mBodyID2));
            if (other != sensor.first && (p.characters.count(other) || (p.bodies.count(other) && p.bodies.at(other).settings.dynamic)))
                contacts.insert({sensor.first, other});
        }
    }
    std::vector<PhysicsEvent> events;
    for (const auto& contact : contacts) if (!p.contacts.count(contact)) events.push_back({contact.first, contact.second, true});
    for (const auto& contact : p.contacts) if (!contacts.count(contact)) events.push_back({contact.first, contact.second, false});
    p.contacts.swap(contacts); return events;
}
bool PhysicsWorld::contains(ecs::Entity entity) const { return impl_->bodies.count(entity) || impl_->characters.count(entity); }
bool PhysicsWorld::grounded(ecs::Entity entity) const {
    const auto found = impl_->characters.find(entity);
    return found != impl_->characters.end() && found->second.value->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}
std::optional<RayHit> PhysicsWorld::raycast(const std::array<float, 3>& origin, const std::array<float, 3>& displacement) const {
    JPH::RayCastResult result;
    if (!impl_->system.GetNarrowPhaseQuery().CastRay(JPH::RRayCast(JPH::RVec3(vec(origin)), vec(displacement)), result)) return {};
    return RayHit{static_cast<ecs::Entity>(impl_->system.GetBodyInterface().GetUserData(result.mBodyID)), result.mFraction};
}
std::array<float,3> PhysicsWorld::velocity(ecs::Entity entity) const {
    const auto found=impl_->characters.find(entity);
    return found==impl_->characters.end()?std::array<float,3>{}:array(found->second.value->GetLinearVelocity());
}
std::optional<RayHit> PhysicsWorld::sphereSweep(const std::array<float,3>& origin,
    const std::array<float,3>& displacement, float radius, ecs::Entity ignore) const {
    if (!std::isfinite(radius)||radius<=0) throw std::invalid_argument("Sphere radius must be positive");
    struct Filter final : JPH::BodyFilter {
        ecs::Entity ignored;
        explicit Filter(ecs::Entity value):ignored(value){}
        bool ShouldCollideLocked(const JPH::Body& body) const override {
            return !body.IsSensor() && body.GetUserData()!=ignored;
        }
    } filter(ignore);
    JPH::Ref<JPH::SphereShape> sphere = new JPH::SphereShape(radius);
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> hits;
    impl_->system.GetNarrowPhaseQuery().CastShape(JPH::RShapeCast(sphere,JPH::Vec3::sReplicate(1),
        JPH::RMat44::sTranslation(JPH::RVec3(vec(origin))),vec(displacement)), {}, JPH::RVec3::sZero(), hits, {}, {}, filter);
    if(!hits.HadHit()) return {};
    return RayHit{static_cast<ecs::Entity>(impl_->system.GetBodyInterface().GetUserData(hits.mHit.mBodyID2)), hits.mHit.mFraction};
}
} // namespace azurerender
