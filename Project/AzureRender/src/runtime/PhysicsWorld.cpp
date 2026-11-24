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
#include <Jolt/Physics/Constraints/HingeConstraint.h>
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
#include "runtime/physics/PhysicsShapeFactory.hpp"
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
    struct Character { JPH::Ref<JPH::CharacterVirtual> value; std::string node; game::Character settings; float groundAge = 1, jumpBuffer = 0; bool jumping = false; std::array<float,3> actualVelocity{}; };
    std::map<ecs::Entity, Body> bodies;
    std::map<ecs::Entity, Character> characters;
    struct HingeState {JPH::Ref<JPH::TwoBodyConstraint> value;ecs::Entity a,b;game::Hinge settings;};
    std::map<ecs::Entity,HingeState> hinges;
    std::set<std::pair<ecs::Entity, ecs::Entity>> contacts;
    Impl() {
        broad.MapObjectToBroadPhaseLayer(0, JPH::BroadPhaseLayer(0)); broad.MapObjectToBroadPhaseLayer(1, JPH::BroadPhaseLayer(1));
        pairs.EnableCollision(0, 1); pairs.EnableCollision(1, 1);
        filter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(broad, 2, pairs, 2);
        system.Init(4096, 0, 4096, 4096, broad, *filter, pairs);
    }
    void removeBody(ecs::Entity entity) {
        for(auto it=hinges.begin();it!=hinges.end();)if(it->second.a==entity||it->second.b==entity){system.RemoveConstraint(it->second.value);it=hinges.erase(it);}else ++it;
        const auto id = bodies.at(entity).id; system.GetBodyInterface().RemoveBody(id); system.GetBodyInterface().DestroyBody(id); bodies.erase(entity);
    }
    ~Impl() { characters.clear(); while (!bodies.empty()) removeBody(bodies.begin()->first); }
};
PhysicsWorld::PhysicsWorld() { initializeJolt(); impl_ = std::make_unique<Impl>(); }
PhysicsWorld::~PhysicsWorld() = default;
std::size_t PhysicsWorld::constraintCount() const{return impl_->hinges.size();}
std::optional<float> PhysicsWorld::hingeAngle(ecs::Entity entity) const{
    const auto found=impl_->hinges.find(entity);if(found==impl_->hinges.end())return {};
    return JPH::RadiansToDegrees(static_cast<const JPH::HingeConstraint*>(found->second.value.GetPtr())->GetCurrentAngle());
}
void PhysicsWorld::validateScene(RuntimeLifecycle& runtime) const{
    auto& world=runtime.world();
    world.each<ecs::TransformComponent,game::RigidBody>([&](auto entity,const auto& transform,const auto& settings){
        if(world.has<game::Character>(entity))throw std::invalid_argument("Character and rigid body are mutually exclusive");
        static_cast<void>(createPhysicsShape(settings,transform.scale,meshResolver_));
    });
    world.each<game::Hinge>([&](auto,const auto& settings){
        validateHinge(settings);const auto a=runtime.entity(settings.bodyA),b=runtime.entity(settings.bodyB);
        const auto* first=world.tryGet<game::RigidBody>(a);const auto* second=world.tryGet<game::RigidBody>(b);
        if(!first||!second||(!first->dynamic&&!second->dynamic)||first->trigger||second->trigger)throw std::invalid_argument("Hinge requires two rigid bodies, one dynamic, without sensors");
    });
}
std::vector<PhysicsEvent> PhysicsWorld::step(RuntimeLifecycle& runtime, float dt, const std::map<ecs::Entity, CharacterMotion>& motions) {
    if (!std::isfinite(dt) || dt <= 0) throw std::invalid_argument("Physics dt must be finite and positive");
    if (sceneRevision_ != runtime.sceneRevision()) {
        validateScene(runtime);
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
        validateRigidBody(settings,transform.scale);
        auto existing = p.bodies.find(entity);
        if (existing != p.bodies.end() && (existing->second.settings.halfExtent != settings.halfExtent
            || existing->second.settings.dynamic != settings.dynamic || existing->second.settings.kinematic != settings.kinematic || existing->second.settings.trigger != settings.trigger || existing->second.scale != transform.scale
            || existing->second.settings.shape!=settings.shape || existing->second.settings.radius!=settings.radius || existing->second.settings.halfHeight!=settings.halfHeight || existing->second.settings.mesh!=settings.mesh)) {
            p.removeBody(entity); existing = p.bodies.end();
        }
        if (existing == p.bodies.end()) {
            const auto shape=createPhysicsShape(settings,transform.scale,meshResolver_);
            JPH::BodyCreationSettings create(shape, JPH::RVec3(vec(transform.translation)), rotation(transform.rotation),
                settings.dynamic ? JPH::EMotionType::Dynamic : settings.kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static, settings.dynamic || settings.kinematic || settings.trigger ? 1 : 0);
            create.mIsSensor = settings.trigger;
            const auto id = bi.CreateAndAddBody(create, JPH::EActivation::Activate);
            if (id.IsInvalid()) throw std::runtime_error("Jolt body capacity exceeded");
            bi.SetUserData(id, entity); p.bodies.emplace(entity, Impl::Body{id, runtime.nodeId(entity), settings, transform.scale});
        } else if (settings.kinematic) bi.MoveKinematic(existing->second.id,JPH::RVec3(vec(transform.translation)),rotation(transform.rotation),dt);
        else if (!settings.dynamic) bi.SetPositionAndRotation(existing->second.id, JPH::RVec3(vec(transform.translation)), rotation(transform.rotation), JPH::EActivation::DontActivate);
    });
    for(auto it=p.hinges.begin();it!=p.hinges.end();)if(!world.has<game::Hinge>(it->first)||!p.bodies.count(it->second.a)||!p.bodies.count(it->second.b)){
        p.system.RemoveConstraint(it->second.value);it=p.hinges.erase(it);
    }else ++it;
    world.each<game::Hinge>([&](auto entity,const auto& settings){
        validateHinge(settings);const auto a=runtime.entity(settings.bodyA),b=runtime.entity(settings.bodyB);
        if(!p.bodies.count(a)||!p.bodies.count(b))return; // Live endpoint deletion releases the constraint.
        const auto* first=world.tryGet<game::RigidBody>(a);const auto* second=world.tryGet<game::RigidBody>(b);
        if((!first->dynamic&&!second->dynamic)||first->trigger||second->trigger)throw std::invalid_argument("Hinge requires a dynamic body and nonsensor endpoints");
        auto found=p.hinges.find(entity);
        if(found!=p.hinges.end()){
            const auto& old=found->second.settings;
            if(found->second.a==a&&found->second.b==b&&old.axis==settings.axis&&old.normal==settings.normal&&old.anchorA==settings.anchorA&&old.anchorB==settings.anchorB&&old.minimumAngle==settings.minimumAngle&&old.maximumAngle==settings.maximumAngle)return;
            p.system.RemoveConstraint(found->second.value);p.hinges.erase(found);
        }
        JPH::HingeConstraintSettings create;
        const auto point=[&](ecs::Entity endpoint,const auto& anchor){const auto& body=p.bodies.at(endpoint);return bi.GetPosition(body.id)+bi.GetRotation(body.id)*(vec(anchor)*vec(body.scale));};
        create.mPoint1=point(a,settings.anchorA);create.mPoint2=point(b,settings.anchorB);
        create.mHingeAxis1=create.mHingeAxis2=vec(settings.axis).Normalized();create.mNormalAxis1=create.mNormalAxis2=vec(settings.normal).Normalized();
        create.mLimitsMin=JPH::DegreesToRadians(settings.minimumAngle);create.mLimitsMax=JPH::DegreesToRadians(settings.maximumAngle);
        JPH::Ref<JPH::TwoBodyConstraint> constraint=bi.CreateConstraint(&create,p.bodies.at(a).id,p.bodies.at(b).id);
        if(!constraint)throw std::runtime_error("Cannot create hinge constraint");
        p.system.AddConstraint(constraint);p.hinges.emplace(entity,Impl::HingeState{constraint,a,b,settings});
        bi.ActivateBody(p.bodies.at(b).id);
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
        character.UpdateGroundVelocity();
        const auto support=onGround?character.GetGroundVelocity():JPH::Vec3::sZero();
        auto& state = found->second;
        state.groundAge = onGround ? 0 : state.groundAge + dt;
        if (onGround && vertical <= support.GetY()+.01F) state.jumping = false;
        state.jumpBuffer = move.jump ? .1F : std::max(0.0F,state.jumpBuffer-dt);
        if (onGround && !state.jumping) vertical = support.GetY();
        if (!state.jumping && state.groundAge <= .1F && state.jumpBuffer > 0) {
            vertical = settings.jumpSpeed+support.GetY(); state.jumpBuffer = 0; state.groundAge = 1; state.jumping = true;
        }
        vertical -= 9.81F * dt;
        const float speed = move.velocity ? 1 : settings.speed / denominator;
        character.SetLinearVelocity({move.x * speed+support.GetX(), vertical, move.z * speed+support.GetZ()});
        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        update.mWalkStairsStepUp = {0,settings.stepHeight,0};
        if (vertical > 0) update.mStickToFloorStepDown = JPH::Vec3::sZero();
        const auto startPosition = character.GetPosition();
        character.ExtendedUpdate(dt, p.system.GetGravity(), update, p.system.GetDefaultBroadPhaseLayerFilter(1), p.system.GetDefaultLayerFilter(1), {}, {}, p.allocator);
        state.actualVelocity = array(JPH::Vec3(character.GetPosition() - startPosition) / dt);
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
std::array<float,3> PhysicsWorld::actualVelocity(ecs::Entity entity) const {
    const auto found=impl_->characters.find(entity);
    return found==impl_->characters.end()?std::array<float,3>{}:found->second.actualVelocity;
}
std::array<float,3> PhysicsWorld::groundVelocity(ecs::Entity entity) const {
    const auto found=impl_->characters.find(entity);
    return found==impl_->characters.end()||!grounded(entity)?std::array<float,3>{}:array(found->second.value->GetGroundVelocity());
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
