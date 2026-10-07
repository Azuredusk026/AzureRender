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
        || zoomSteps != 0.0F || frameRequested || lookDeltaX != 0 || lookDeltaY != 0
        || flyForward != 0 || flyRight != 0 || flyUp != 0;
}

bool EditorCameraController::apply(
    const EditorViewportInput& input,
    std::array<float, 3>& position,
    std::array<float, 3>& target) {
    if (!input.active()) {
        return false;
    }

    const auto startingPosition=position;
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

    if(input.frameRequested) {
        const auto direction=normalize(offset);
        target=input.frameTarget;
        for(std::size_t axis=0;axis<3;++axis)position[axis]=target[axis]+direction[axis]*std::max(input.frameDistance,.001F);
        return true;
    }
    if(input.lookDeltaX!=0 || input.lookDeltaY!=0) {
        float yaw=std::atan2(-offset[0],-offset[2]);
        float pitch=std::asin(std::clamp(-offset[1]/distance,-1.F,1.F));
        yaw-=input.lookDeltaX*.008F;
        pitch=std::clamp(pitch-input.lookDeltaY*.008F,-1.48F,1.48F);
        const Vector3 forward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
        for(std::size_t axis=0;axis<3;++axis){target[axis]=position[axis]+forward[axis]*distance;offset[axis]=-forward[axis]*distance;}
    }
    float yaw = std::atan2(offset[0], offset[2]);
    float pitch = std::asin(std::clamp(offset[1] / distance, -1.0F, 1.0F));
    yaw -= input.orbitDeltaX * 0.008F;
    pitch = std::clamp(
        pitch - input.orbitDeltaY * 0.008F,
        -1.48F,
        1.48F);
    distance = std::clamp(
        distance * std::exp(-input.zoomSteps * 0.16F),
        0.001F,
        1000000.0F);

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
    Vector3 translation{
        (-right[0] * input.panDeltaX + up[0] * input.panDeltaY) * panScale,
        (-right[1] * input.panDeltaX + up[1] * input.panDeltaY) * panScale,
        (-right[2] * input.panDeltaX + up[2] * input.panDeltaY) * panScale,
    };
    const auto motion=normalize({forward[0]*input.flyForward+right[0]*input.flyRight,
        forward[1]*input.flyForward+right[1]*input.flyRight+input.flyUp,
        forward[2]*input.flyForward+right[2]*input.flyRight});
    for (std::size_t axis = 0; axis < 3; ++axis) {
        translation[axis]+=motion[axis]*input.flySpeed*std::clamp(input.deltaSeconds,0.F,.1F);
        target[axis] += translation[axis];
        position[axis] = target[axis] + offset[axis];
        if((input.lookDeltaX!=0 || input.lookDeltaY!=0) && input.orbitDeltaX==0 && input.orbitDeltaY==0 && input.zoomSteps==0)
            position[axis]=startingPosition[axis]+translation[axis];
    }
    return true;
}

}  // namespace azurerender
