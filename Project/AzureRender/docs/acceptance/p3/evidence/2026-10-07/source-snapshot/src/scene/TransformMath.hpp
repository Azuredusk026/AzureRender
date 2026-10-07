#pragma once

#include "render/RenderMath.hpp"

#include <algorithm>
#include <array>

namespace azurerender::scene {

[[nodiscard]] inline azurerender::internal::Matrix4 identityMatrix() {
    return {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

// TRS composition shared by the scene graph, the editor gizmo and the
// turntable: T * Rx * Ry * Rz * S with rotation angles in degrees.
[[nodiscard]] inline azurerender::internal::Matrix4 composeTrs(
    const azurerender::internal::Vector3& translationValue,
    const azurerender::internal::Vector3& rotationDegrees,
    const azurerender::internal::Vector3& scaleValue) {
    constexpr float kPi = 3.14159265358979323846F;
    return azurerender::internal::multiply(
        azurerender::internal::translation(
            translationValue[0], translationValue[1], translationValue[2]),
        azurerender::internal::multiply(
            azurerender::internal::multiply(
                azurerender::internal::rotationX(
                    rotationDegrees[0] * kPi / 180.0F),
                azurerender::internal::rotationY(
                    rotationDegrees[1] * kPi / 180.0F)),
            azurerender::internal::multiply(
                azurerender::internal::rotationZ(
                    rotationDegrees[2] * kPi / 180.0F),
                azurerender::internal::scale(
                    scaleValue[0], scaleValue[1], scaleValue[2]))));
}

struct AxisAlignedBounds {
    azurerender::internal::Vector3 minimum{0.0F, 0.0F, 0.0F};
    azurerender::internal::Vector3 maximum{0.0F, 0.0F, 0.0F};
};

// Exact AABB of a transformed AABB: transform all eight corners and
// re-enclose. Handles rotation and negative scale correctly.
[[nodiscard]] inline AxisAlignedBounds transformBounds(
    const AxisAlignedBounds& bounds,
    const azurerender::internal::Matrix4& transform) {
    AxisAlignedBounds result{
        {1.0e30F, 1.0e30F, 1.0e30F},
        {-1.0e30F, -1.0e30F, -1.0e30F},
    };
    for (std::uint32_t corner = 0; corner < 8; ++corner) {
        const azurerender::internal::Vector3 point{
            (corner & 1U) != 0U ? bounds.maximum[0] : bounds.minimum[0],
            (corner & 2U) != 0U ? bounds.maximum[1] : bounds.minimum[1],
            (corner & 4U) != 0U ? bounds.maximum[2] : bounds.minimum[2],
        };
        const azurerender::internal::Vector3 transformed =
            azurerender::internal::transformPosition(transform, point);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            result.minimum[axis] =
                std::min(result.minimum[axis], transformed[axis]);
            result.maximum[axis] =
                std::max(result.maximum[axis], transformed[axis]);
        }
    }
    return result;
}

}  // namespace azurerender::scene
