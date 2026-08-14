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

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
