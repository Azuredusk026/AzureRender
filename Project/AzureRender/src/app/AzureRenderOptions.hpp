#pragma once

#include "render/RenderSettings.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace azurerender {
class EditorSession;
}

struct AzureRenderOptions {
    std::string assetPath;
    std::string resourceRoot;
    std::string environmentPath;
    std::uint64_t smokeFrameLimit = 0;
    std::string captureDirectory;
    std::uint64_t captureFrameLimit = 0;
    std::uint32_t captureFps = 60;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool portfolioMode = false;
    bool gpuTimingEnabled = false;
    std::string gpuTimingOutput;
    azurerender::RenderSettings renderSettings;
    bool hudEnabled = false;
    bool technicalSequence = false;
    std::string qaCamera;
    std::string qaLight;
    std::string qaEffect;
    std::string qaEffectState;
    std::string qaIsolation;
    bool editorMode = false;
    std::string editorScenePath;
    std::shared_ptr<azurerender::EditorSession> editorSession;
};
