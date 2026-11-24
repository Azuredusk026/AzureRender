#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include "PhysicsDescriptors.hpp"
#include <functional>
namespace azurerender {
JPH::RefConst<JPH::Shape> createPhysicsShape(const game::RigidBody&,const std::array<float,3>& scale,const CollisionMeshResolver&);
class AssetDatabase;
CollisionMeshResolver collisionMeshAssets(const AssetDatabase& assets);
}
