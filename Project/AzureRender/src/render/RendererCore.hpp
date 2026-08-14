#pragma once

#include "RenderSettings.hpp"

#include <cstdint>
#include <string>

namespace azurerender {

struct SceneView {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::string assetPath;
    std::string cameraPreset = "full-body-front";
    std::string lightPreset = "stylized-key";
    RenderSettings renderSettings;
};

struct CaptureRequest {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::string outputDirectory;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameCount = 1;
    std::uint32_t fps = 60;
};

class RendererCoreBoundary final {
public:
    static constexpr std::uint32_t kApiVersion = 1;

    static void validateSceneView(const SceneView& view);
    static void validateCaptureRequest(const CaptureRequest& request);
};

}  // namespace azurerender
