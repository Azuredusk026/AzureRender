#include "editor/EditorCameraController.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

float distance(
    const std::array<float, 3>& left,
    const std::array<float, 3>& right) {
    const float x = left[0] - right[0];
    const float y = left[1] - right[1];
    const float z = left[2] - right[2];
    return std::sqrt(x * x + y * y + z * z);
}

bool near(const float left, const float right, const float epsilon = 0.0001F) {
    return std::abs(left - right) <= epsilon;
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    bool passed = true;

    std::array<float, 3> position{2.8F, 2.1F, 3.2F};
    std::array<float, 3> target{0.0F, 0.0F, 0.0F};
    const auto originalPosition = position;
    const auto originalTarget = target;
    passed &= expect(
        !azurerender::EditorCameraController::apply({}, position, target),
        "inactive input must not update the camera");
    passed &= expect(
        position == originalPosition && target == originalTarget,
        "inactive input must preserve camera values");

    const float originalDistance = distance(position, target);
    azurerender::EditorViewportInput orbit;
    orbit.orbitDeltaX = 24.0F;
    orbit.orbitDeltaY = -12.0F;
    passed &= expect(
        azurerender::EditorCameraController::apply(orbit, position, target),
        "orbit input must update the camera");
    passed &= expect(
        near(distance(position, target), originalDistance),
        "orbit must preserve camera distance");
    passed &= expect(target == originalTarget, "orbit must preserve target");

    azurerender::EditorViewportInput zoom;
    zoom.zoomSteps = 2.0F;
    const float beforeZoom = distance(position, target);
    azurerender::EditorCameraController::apply(zoom, position, target);
    passed &= expect(
        distance(position, target) < beforeZoom,
        "positive wheel input must zoom in");

    azurerender::EditorViewportInput pan;
    pan.panDeltaX = 30.0F;
    pan.panDeltaY = -15.0F;
    const std::array<float, 3> offsetBeforePan{
        position[0] - target[0],
        position[1] - target[1],
        position[2] - target[2],
    };
    azurerender::EditorCameraController::apply(pan, position, target);
    const std::array<float, 3> offsetAfterPan{
        position[0] - target[0],
        position[1] - target[1],
        position[2] - target[2],
    };
    passed &= expect(
        distance(offsetBeforePan, offsetAfterPan) < 0.0001F,
        "pan must preserve camera-to-target offset");
    passed &= expect(target != originalTarget, "pan must move the target");

    // A horizontal drag must turn the view towards the camera's local right.
    position = {0,0,5}; target = {0,0,0};
    azurerender::EditorViewportInput rightDrag; rightDrag.orbitDeltaX=10;
    azurerender::EditorCameraController::apply(rightDrag,position,target);
    passed &= expect(target[0]-position[0]>0, "right drag must look right");
    position={0,0,1000};target={0,0,0};
    azurerender::EditorViewportInput largePan;largePan.panDeltaX=1;
    azurerender::EditorCameraController::apply(largePan,position,target);
    passed &= expect(near(distance(position,target),1000,.01F), "large scene pan preserves distance");
    position={0,0,5};target={0,0,0};
    azurerender::EditorViewportInput frame;frame.frameRequested=true;frame.frameTarget={10,20,30};
    azurerender::EditorCameraController::apply(frame,position,target);
    passed &= expect(target==frame.frameTarget, "framing uses exact world centre");
    passed &= expect(near(position[0],10)&&near(position[1],20), "framing preserves view direction");
    position={0,0,5};target={0,0,0};
    azurerender::EditorViewportInput look;look.lookDeltaX=10;look.lookDeltaY=-10;
    azurerender::EditorCameraController::apply(look,position,target);
    passed &= expect(position==std::array<float,3>{0,0,5}, "RMB look preserves camera position");
    passed &= expect(target[0]>0 && target[1]>0, "RMB right/up follows local view directions");
    position={0,0,5};target={0,0,0};
    azurerender::EditorViewportInput fly;fly.flyForward=1;fly.deltaSeconds=.1F;
    azurerender::EditorCameraController::apply(fly,position,target);
    passed &= expect(near(position[2],4.5F)&&near(target[2],-.5F), "flight translates camera and target together");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
