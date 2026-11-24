#pragma once

#include "render/RenderMath.hpp"
#include "ecs/Entity.hpp"
#include "scene/TransformMath.hpp"

#include <array>
#include <string>

namespace azurerender::scene {

// Parent link for transform hierarchy. kInvalidEntity means a root node.
struct HierarchyComponent {
    azurerender::ecs::Entity parent = azurerender::ecs::kInvalidEntity;
};

// Cached world transform. Recomputed by updateTransforms whenever the
// entity's local transform or any ancestor changes.
struct WorldTransformComponent {
    azurerender::internal::Matrix4 matrix = identityMatrix();
    bool dirty = true;
};

// Asset the entity renders. The render resource cache keys GPU resources by
// this path so entities share one copy of each mesh.
struct MeshReferenceComponent {
    std::string assetPath;
};

// Mesh-local bounds used for visibility. Populated from the asset's
// computed bounds at load; the world bounds are derived per frame from the
// world transform.
struct RenderBoundsComponent {
    AxisAlignedBounds localBounds{};
    AxisAlignedBounds worldBounds{};
    bool visible = true;
};

}  // namespace azurerender::scene
