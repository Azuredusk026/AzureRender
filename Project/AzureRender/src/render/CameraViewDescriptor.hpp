#pragma once
#include <array>
namespace azurerender {
enum class CameraProjectionKind { Perspective, Orthographic };
// Pure value contract shared by renderers and editor tools.
struct CameraViewDescriptor {
    CameraProjectionKind projection=CameraProjectionKind::Perspective;
    std::array<float,3> position{0,1.18F,3.55F},target{0,1.18F,0},up{0,1,0};
    float fov=.692604F,orthoHeight=5,nearPlane=.1F,farPlane=100.F;
};
}
