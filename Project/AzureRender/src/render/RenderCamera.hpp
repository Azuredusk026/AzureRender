#pragma once
#include "render/CameraProjection.hpp"
#include <optional>
namespace azurerender {
enum class RenderCameraSource { SceneView, GameCamera, Fallback };
struct RenderCamera {
    CameraViewDescriptor view;
    RenderCameraSource source=RenderCameraSource::Fallback;
};
inline RenderCamera selectRenderCamera(const std::optional<CameraViewDescriptor>& sceneView,
    const std::optional<CameraViewDescriptor>& gameView,const CameraViewDescriptor& fallback){
    RenderCamera result{fallback,RenderCameraSource::Fallback};
    if(sceneView)result={*sceneView,RenderCameraSource::SceneView};
    else if(gameView)result={*gameView,RenderCameraSource::GameCamera};
    validateCameraView(result.view,1.F);return result;
}
}
