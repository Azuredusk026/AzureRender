#include "EditorCameraController.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include "render/CameraProjection.hpp"

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

bool EditorCameraController::apply(const EditorViewportInput& input,CameraViewDescriptor& view){
    if(!input.active())return false;validateCameraView(view,1);
    auto next=view;
    if(input.frameRequested){const auto forward=internal::normalize(internal::subtract(next.target,next.position));next.target=input.frameTarget;next.position=internal::subtract(next.target,internal::scaleVector(forward,std::max(input.frameDistance,.001F)));view=next;return true;}
    auto forward=internal::normalize(internal::subtract(next.target,next.position));auto right=internal::normalize(internal::cross(forward,next.up));
    const auto rotate=[](const Vector3& v,const Vector3& axis,float angle){const auto c=std::cos(angle),s=std::sin(angle);return internal::addVectors(internal::addVectors(internal::scaleVector(v,c),internal::scaleVector(internal::cross(axis,v),s)),internal::scaleVector(axis,internal::dot(axis,v)*(1-c)));};
    const auto distance=internal::vectorLength(internal::subtract(next.target,next.position));
    if(input.lookDeltaX!=0||input.lookDeltaY!=0){
        forward=rotate(forward,internal::normalize(next.up),-input.lookDeltaX*.008F);right=internal::normalize(internal::cross(forward,next.up));
        const auto pitch=rotate(forward,right,-input.lookDeltaY*.008F);if(std::abs(internal::dot(pitch,internal::normalize(next.up)))<.996F)forward=pitch;
        next.target=internal::addVectors(next.position,internal::scaleVector(forward,distance));
    }
    if(input.orbitDeltaX!=0||input.orbitDeltaY!=0){
        auto offset=internal::subtract(next.position,next.target);offset=rotate(offset,internal::normalize(next.up),-input.orbitDeltaX*.008F);forward=internal::normalize(internal::scaleVector(offset,-1));right=internal::normalize(internal::cross(forward,next.up));
        const auto tilted=rotate(offset,right,input.orbitDeltaY*.008F);if(std::abs(internal::dot(internal::normalize(tilted),internal::normalize(next.up)))<.996F)offset=tilted;
        next.position=internal::addVectors(next.target,offset);forward=internal::normalize(internal::scaleVector(offset,-1));
    }
    right=internal::normalize(internal::cross(forward,next.up));const auto up=internal::cross(right,forward);
    if(input.zoomSteps!=0){const auto factor=std::exp(-input.zoomSteps*.16F);if(next.projection==CameraProjectionKind::Orthographic)next.orthoHeight=std::clamp(next.orthoHeight*factor,.0001F,1000000.F);
        else next.position=internal::subtract(next.target,internal::scaleVector(forward,std::clamp(distance*factor,.001F,1000000.F)));}
    const float panScale=(next.projection==CameraProjectionKind::Orthographic?next.orthoHeight:distance)*.0015F;
    auto translation=internal::addVectors(internal::scaleVector(right,-input.panDeltaX*panScale),internal::scaleVector(up,input.panDeltaY*panScale));
    auto motion=internal::addVectors(internal::addVectors(internal::scaleVector(forward,input.flyForward),internal::scaleVector(right,input.flyRight)),Vector3{0,input.flyUp,0});
    const auto length=internal::vectorLength(motion);if(length>1e-6F)translation=internal::addVectors(translation,internal::scaleVector(motion,input.flySpeed*std::clamp(input.deltaSeconds,0.F,.1F)/length));
    next.position=internal::addVectors(next.position,translation);next.target=internal::addVectors(next.target,translation);validateCameraView(next,1);view=next;return true;
}

}  // namespace azurerender
