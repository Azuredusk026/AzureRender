#pragma once
#include "render/CameraViewDescriptor.hpp"
namespace azurerender {
// Per-document editing camera. Never stored as a game component.
struct SceneViewCamera : CameraViewDescriptor {};
using EditorViewDescriptor=SceneViewCamera;
}
