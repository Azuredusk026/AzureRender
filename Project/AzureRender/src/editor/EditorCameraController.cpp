#include "EditorCameraController.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace azurerender {

namespace {

using Vector3 = std::array<float, 3>;

float dot(const Vector3& left, const Vector3& right) {
    return left[0] * right[0] + left[1] * right[1]
        + left[2] * right[2];
}

Vector3 cross(const Vector3& left, const Vector3& right) {
    return {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
}

Vector3 normalize(const Vector3& value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.00001F) {
        return {0.0F, 0.0F, 0.0F};
    }
    return {value[0] / length, value[1] / length, value[2] / length};
}

}  // namespace

bool EditorViewportInput::active() const noexcept {
    return orbitDeltaX != 0.0F || orbitDeltaY != 0.0F
        || panDeltaX != 0.0F || panDeltaY != 0.0F
        || zoomSteps != 0.0F;
}

bool EditorCameraController::apply(
    const EditorViewportInput& input,
    std::array<float, 3>& position,
    std::array<float, 3>& target) {
    if (!input.active()) {
        return false;
    }

    Vector3 offset{
        position[0] - target[0],
        position[1] - target[1],
        position[2] - target[2],
    };
    float distance = std::sqrt(dot(offset, offset));
    if (distance <= 0.00001F) {
        offset = {0.0F, 0.0F, 1.0F};
        distance = 1.0F;
    }

    float yaw = std::atan2(offset[0], offset[2]);
    float pitch = std::asin(std::clamp(offset[1] / distance, -1.0F, 1.0F));
    yaw += input.orbitDeltaX * 0.008F;
    pitch = std::clamp(
        pitch - input.orbitDeltaY * 0.008F,
        -1.48F,
        1.48F);
    distance = std::clamp(
        distance * std::exp(-input.zoomSteps * 0.16F),
        0.25F,
        50.0F);

    const float pitchCosine = std::cos(pitch);
    offset = {
        std::sin(yaw) * pitchCosine * distance,
        std::sin(pitch) * distance,
        std::cos(yaw) * pitchCosine * distance,
    };

    const Vector3 forward = normalize({-offset[0], -offset[1], -offset[2]});
    const Vector3 right = normalize(cross(forward, {0.0F, 1.0F, 0.0F}));
    const Vector3 up = normalize(cross(right, forward));
    const float panScale = distance * 0.0015F;
    const Vector3 translation{
        (-right[0] * input.panDeltaX + up[0] * input.panDeltaY) * panScale,
        (-right[1] * input.panDeltaX + up[1] * input.panDeltaY) * panScale,
        (-right[2] * input.panDeltaX + up[2] * input.panDeltaY) * panScale,
    };
    for (std::size_t axis = 0; axis < 3; ++axis) {
        target[axis] += translation[axis];
        position[axis] = target[axis] + offset[axis];
    }
    return true;
}

}  // namespace azurerender
