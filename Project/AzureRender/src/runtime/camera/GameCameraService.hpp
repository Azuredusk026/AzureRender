#pragma once
#include "render/CameraViewDescriptor.hpp"
#include "ecs/CameraComponents.hpp"
#include <optional>
#include <string>
namespace azurerender {
class RuntimeLifecycle;
struct SceneDocument;
struct GameCameraSelection {
    std::optional<CameraViewDescriptor> view;
    std::string node;
    std::string diagnostic;
};
CameraViewDescriptor gameCameraView(const game::GameCamera& camera,const std::array<float,16>& world);
// Resolves authored cameras from the interpolated scene and live components.
// Highest priority wins; equal priorities use the stable node identifier.
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const SceneDocument& scene);
}
