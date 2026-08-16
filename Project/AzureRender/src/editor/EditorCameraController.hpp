#pragma once

#include <array>

namespace azurerender {

struct EditorViewportInput {
    float orbitDeltaX = 0.0F;
    float orbitDeltaY = 0.0F;
    float panDeltaX = 0.0F;
    float panDeltaY = 0.0F;
    float zoomSteps = 0.0F;
    float pickX = -1.0F;
    float pickY = -1.0F;
    bool pickRequested = false;

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
