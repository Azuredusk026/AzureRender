#pragma once

#include "scene/Frustum.hpp"
#include "scene/TransformMath.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace azurerender::scene {

// One drawable mesh occurrence for a frame. `sourceIndex` is the entity's
// stable ordering key so batch building stays deterministic frame to frame.
struct SceneInstance {
    azurerender::internal::Matrix4 model = identityMatrix();
    AxisAlignedBounds worldBounds{};
    std::uint32_t sourceIndex = 0;
    std::uint32_t meshKey = 0;
    std::string nodeId;
};

// Marks instances outside the frustum invisible. Returns the number of
// visible instances. Instance order is preserved; culling only filters.
[[nodiscard]] inline std::size_t appendVisibleInstances(
    const std::vector<SceneInstance>& instances,
    const FrustumPlanes& frustum,
    std::vector<const SceneInstance*>& visible) {
    visible.clear();
    for (const SceneInstance& instance : instances) {
        if (boundsInsideFrustum(
                frustum,
                instance.worldBounds.minimum,
                instance.worldBounds.maximum)) {
            visible.push_back(&instance);
        }
    }
    return visible.size();
}

}  // namespace azurerender::scene
