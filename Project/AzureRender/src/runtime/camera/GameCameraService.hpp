#pragma once
#include "render/CameraViewDescriptor.hpp"
#include "ecs/CameraComponents.hpp"
#include "render/ReferenceCameraCalibration.hpp"
#include <optional>
#include <string>
namespace azurerender {
class RuntimeLifecycle;
struct SceneDocument;
namespace scene {struct SceneDescription;}
struct GameCameraSelection {
    std::optional<CameraViewDescriptor> view;
    std::string node;
    std::string diagnostic;
    std::optional<ReferenceCameraView> referenceView;
    std::string referenceName;
    bool referenceRejected=false;
};
CameraViewDescriptor gameCameraView(const game::GameCamera& camera,const std::array<float,16>& world);
// Resolves authored cameras from the interpolated scene and live components.
// Highest priority wins; equal priorities use the stable node identifier.
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const SceneDocument& scene);
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const scene::SceneDescription& scene);
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const scene::SceneDescription& scene,const ReferenceCameraViews& views);
}
