#pragma once

#include "render/RenderMath.hpp"

#include <array>
#include <cmath>

namespace azurerender::scene {

// Six view-frustum planes in world space. Normals point inward; a point is
// inside when every signed distance is non-negative.
struct FrustumPlanes {
    std::array<std::array<float, 4>, 6> planes{};
};

// Gribb/Hartmann plane extraction from a column-major view-projection
// matrix. Vulkan clip volume: -w <= x,y <= w, 0 <= z <= w, so the near
// plane is the third matrix row and the far plane is row3 - row2.
[[nodiscard]] inline FrustumPlanes extractFrustumPlanes(
    const azurerender::internal::Matrix4& viewProjection) {
    const auto row = [&viewProjection](const std::size_t index) {
        return std::array<float, 4>{
            viewProjection[index + 0],
            viewProjection[index + 4],
            viewProjection[index + 8],
            viewProjection[index + 12],
        };
    };
    const std::array<float, 4> c0 = row(0);
    const std::array<float, 4> c1 = row(1);
    const std::array<float, 4> c2 = row(2);
    const std::array<float, 4> c3 = row(3);
    FrustumPlanes frustum;
    const auto make = [&frustum](
        const std::size_t index,
        const float a,
        const float b,
        const float c,
        const float d) {
        const float length = std::sqrt(a * a + b * b + c * c);
        const float inverse = length > 0.0F ? 1.0F / length : 1.0F;
        frustum.planes[index] = {
            a * inverse, b * inverse, c * inverse, d * inverse};
    };
    make(0, c3[0] + c0[0], c3[1] + c0[1], c3[2] + c0[2], c3[3] + c0[3]);
    make(1, c3[0] - c0[0], c3[1] - c0[1], c3[2] - c0[2], c3[3] - c0[3]);
    make(2, c3[0] + c1[0], c3[1] + c1[1], c3[2] + c1[2], c3[3] + c1[3]);
    make(3, c3[0] - c1[0], c3[1] - c1[1], c3[2] - c1[2], c3[3] - c1[3]);
    make(4, c2[0], c2[1], c2[2], c2[3]);
    make(5, c3[0] - c2[0], c3[1] - c2[1], c3[2] - c2[2], c3[3] - c2[3]);
    return frustum;
}

// AABB test against the frustum using the positive-vertex rule: a box is
// outside only when its most-inside corner is still outside a plane.
[[nodiscard]] inline bool boundsInsideFrustum(
    const FrustumPlanes& frustum,
    const azurerender::internal::Vector3& minimum,
    const azurerender::internal::Vector3& maximum) {
    for (const std::array<float, 4>& plane : frustum.planes) {
        const float positiveX =
            plane[0] >= 0.0F ? maximum[0] : minimum[0];
        const float positiveY =
            plane[1] >= 0.0F ? maximum[1] : minimum[1];
        const float positiveZ =
            plane[2] >= 0.0F ? maximum[2] : minimum[2];
        const float distance = plane[0] * positiveX + plane[1] * positiveY
            + plane[2] * positiveZ + plane[3];
        if (distance < 0.0F) {
            return false;
        }
    }
    return true;
}

}  // namespace azurerender::scene
