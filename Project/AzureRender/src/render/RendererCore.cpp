#include "RendererCore.hpp"

#include <stdexcept>

namespace azurerender {

void RendererCoreBoundary::validateSceneView(const SceneView& view) {
    if (view.assetPath.empty()) {
        throw std::invalid_argument("SceneView.assetPath must not be empty");
    }
    if (view.cameraPreset.empty() || view.lightPreset.empty()) {
        throw std::invalid_argument(
            "SceneView cameraPreset and lightPreset are required");
    }
    validateRenderSettings(view.renderSettings);
}

void RendererCoreBoundary::validateCaptureRequest(
    const CaptureRequest& request) {
    if (request.outputDirectory.empty()) {
        throw std::invalid_argument(
            "CaptureRequest.outputDirectory must not be empty");
    }
    if (request.width < 64 || request.width > 7680
        || request.height < 64 || request.height > 4320) {
        throw std::invalid_argument(
            "CaptureRequest dimensions must be within 64..7680x64..4320");
    }
    if (request.frameCount == 0 || request.fps == 0 || request.fps > 240) {
        throw std::invalid_argument(
            "CaptureRequest frameCount/fps values are invalid");
    }
}

}  // namespace azurerender
