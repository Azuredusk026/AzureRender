#include "ecs/World.hpp"
#include "scene/Frustum.hpp"
#include "scene/TransformSystem.hpp"

#include <cassert>
#include <cmath>

#ifdef _WIN32
#include <crtdbg.h>
#endif

namespace {

using azurerender::ecs::World;
using azurerender::internal::Matrix4;
using azurerender::internal::Vector3;
using azurerender::scene::AxisAlignedBounds;

constexpr float kEpsilon = 1.0e-4F;

bool near(const float left, const float right) {
    return std::abs(left - right) < kEpsilon;
}

bool near3(const Vector3& left, const Vector3& right) {
    return near(left[0], right[0]) && near(left[1], right[1])
        && near(left[2], right[2]);
}

}  // namespace

int main() {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    // composeTrs: translation applied last in world axes, scale per column.
    const Matrix4 transform = azurerender::scene::composeTrs(
        {1.0F, 2.0F, 3.0F}, {0.0F, 90.0F, 0.0F}, {2.0F, 2.0F, 2.0F});
    const Vector3 rotated = azurerender::internal::transformPosition(
        transform, {1.0F, 0.0F, 0.0F});
    // 90 degrees about Y maps +X to -Z in this convention, then scaled
    // and translated.
    assert(near3(rotated, {1.0F, 2.0F, 1.0F}));

    // transformBounds: unit cube rotated 90 degrees about Y keeps an
    // axis-aligned enclosure by corner sweep; negative scale flips axes.
    const AxisAlignedBounds unit{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    const AxisAlignedBounds rotatedBox =
        azurerender::scene::transformBounds(unit, transform);
    assert(near3(rotatedBox.minimum, {-1.0F, 0.0F, 1.0F}));
    assert(near3(rotatedBox.maximum, {3.0F, 4.0F, 5.0F}));

    // extractFrustumPlanes with identity view-projection: the clip volume is
    // [-1, 1]^3, so origin boxes are inside and far boxes outside.
    const azurerender::scene::FrustumPlanes identityFrustum =
        azurerender::scene::extractFrustumPlanes(
            azurerender::scene::identityMatrix());
    assert(azurerender::scene::boundsInsideFrustum(
        identityFrustum, {-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}));
    assert(!azurerender::scene::boundsInsideFrustum(
        identityFrustum, {10.0F, 0.0F, 0.0F}, {11.0F, 1.0F, 1.0F}));
    // A box that crosses one plane stays inside.
    assert(azurerender::scene::boundsInsideFrustum(
        identityFrustum, {0.9F, 0.0F, 0.0F}, {2.0F, 0.1F, 0.1F}));

    // updateTransforms: children inherit the parent world matrix and only
    // dirty subtrees recompute.
    World world;
    const azurerender::ecs::Entity parent = world.createEntity();
    const azurerender::ecs::Entity child = world.createEntity();
    const azurerender::ecs::Entity grandchild = world.createEntity();
    world.addComponent(parent, azurerender::ecs::TransformComponent{
        {10.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}});
    world.addComponent(child, azurerender::ecs::TransformComponent{
        {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 2.0F}});
    world.addComponent(grandchild, azurerender::ecs::TransformComponent{
        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}});
    world.addComponent(child, azurerender::scene::HierarchyComponent{parent});
    world.addComponent(
        grandchild, azurerender::scene::HierarchyComponent{child});
    world.addComponent(parent, azurerender::scene::WorldTransformComponent{});
    world.addComponent(child, azurerender::scene::WorldTransformComponent{});
    world.addComponent(
        grandchild, azurerender::scene::WorldTransformComponent{});

    assert(azurerender::scene::updateTransforms(world) == 3);
    const auto* parentWorld =
        world.tryGet<azurerender::scene::WorldTransformComponent>(parent);
    const auto* childWorld =
        world.tryGet<azurerender::scene::WorldTransformComponent>(child);
    const auto* grandchildWorld =
        world.tryGet<azurerender::scene::WorldTransformComponent>(grandchild);
    assert(near3(
        azurerender::internal::transformPosition(
            childWorld->matrix, {0.0F, 0.0F, 0.0F}),
        {11.0F, 0.0F, 0.0F}));
    assert(near3(
        azurerender::internal::transformPosition(
            grandchildWorld->matrix, {0.0F, 0.0F, 0.0F}),
        {11.0F, 2.0F, 0.0F}));
    assert(parentWorld != nullptr && !parentWorld->dirty);

    // No edits: nothing recomputes.
    assert(azurerender::scene::updateTransforms(world) == 0);

    // Edit the parent: the whole subtree follows.
    world.tryGet<azurerender::ecs::TransformComponent>(parent)
        ->translation[1] = 5.0F;
    world.tryGet<azurerender::scene::WorldTransformComponent>(parent)
        ->dirty = true;
    assert(azurerender::scene::updateTransforms(world) == 3);
    assert(near3(
        azurerender::internal::transformPosition(
            grandchildWorld->matrix, {0.0F, 0.0F, 0.0F}),
        {11.0F, 7.0F, 0.0F}));

    // A hierarchy cycle is tolerated: cyclic members stay untouched.
    World cyclic;
    const azurerender::ecs::Entity a = cyclic.createEntity();
    const azurerender::ecs::Entity b = cyclic.createEntity();
    cyclic.addComponent(a, azurerender::ecs::TransformComponent{});
    cyclic.addComponent(b, azurerender::ecs::TransformComponent{});
    cyclic.addComponent(a, azurerender::scene::HierarchyComponent{b});
    cyclic.addComponent(b, azurerender::scene::HierarchyComponent{a});
    cyclic.addComponent(a, azurerender::scene::WorldTransformComponent{});
    cyclic.addComponent(b, azurerender::scene::WorldTransformComponent{});
    assert(azurerender::scene::updateTransforms(cyclic) == 0);
    return 0;
}
