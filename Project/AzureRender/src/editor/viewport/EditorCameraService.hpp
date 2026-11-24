#pragma once
#include "SelectionBounds.hpp"
#include "EditorViewDescriptor.hpp"
#include <array>
namespace azurerender {
struct FrameSelectionResult {
    bool passed=false;
    std::array<float,3> position{}, target{};
    float distance=0;
    std::string diagnostic;
};
class EditorCameraService {
public:
    static void orient(EditorViewDescriptor&,const std::string& axis);
    static void projection(EditorViewDescriptor&,CameraProjectionKind);
    static FrameSelectionResult frameSelection(EditorViewDescriptor&,const SelectionBoundsResult&,float aspect);
    static FrameSelectionResult frameSelection(const SelectionBoundsResult& bounds,
        std::array<float,3> position, std::array<float,3> target, float verticalFovRadians,
        float aspect, float nearPlane=.01F, float farPlane=100000.F);
};
}
