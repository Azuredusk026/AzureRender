#pragma once

#include <array>

namespace azurerender {

struct EditorViewportInput {
    float orbitDeltaX = 0.0F;
    float orbitDeltaY = 0.0F;
    float lookDeltaX = 0.0F;
    float lookDeltaY = 0.0F;
    float flyForward = 0.0F;
    float flyRight = 0.0F;
    float flyUp = 0.0F;
    float flySpeed = 5.0F;
    float deltaSeconds = 0.0F;
    float frameDistance = 5.0F;
    bool pickAdditive = false;
    float panDeltaX = 0.0F;
    float panDeltaY = 0.0F;
    float zoomSteps = 0.0F;
    float pickX = -1.0F;
    float pickY = -1.0F;
    bool pickRequested = false;
    bool frameRequested = false;
    std::array<float,3> frameTarget{};

    [[nodiscard]] bool active() const noexcept;
};

class EditorCameraController final {
public:
    static bool apply(
        const EditorViewportInput& input,
        std::array<float, 3>& position,
        std::array<float, 3>& target);
};

}  // namespace azurerender
