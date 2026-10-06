#pragma once

#include "render/RenderSettings.hpp"
#include "runtime/SceneDocument.hpp"
#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace azurerender {
class EditorSession;
}

struct AzureRenderOptions {
    std::string projectFile;
    std::string editorActionsPath;
    std::string runtimeReportPath;
    std::string gameActionsPath;
    std::string settingsFile,defaultSettingsFile,projectSettingsFile;
    std::string aiPython="python",aiConfig,aiFixture;
    std::string shaderReloadConfig,previewViews;
    nlohmann::json settingOverrides=nlohmann::json::object();
    std::string validationScript,validationReport,validationTokenEnvironment,validationEndpoint;
    std::string validationAddress="127.0.0.1";
    std::uint16_t validationPort=0;
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
    // Force the fixed per-material descriptor path even when the device
    // supports descriptor indexing. Exists so the fallback path stays
    // testable on capable hardware.
    bool bindlessDisabled = false;
    // Disable frustum culling so every instance is submitted. The QA
    // toggle that proves both sides render identical frames.
    bool cullingDisabled = false;
    bool qaDisableFaceCulling = false;
    bool qaDisableDepthTest = false;
    bool computeSkinningDisabled = false;
    bool parallelRecordingDisabled = false;
    bool gpuCullingDisabled = false;
    bool visibilityPrototype = false;
    bool multiDrawIndirectDisabled = false;
    bool fixedFrameStep = false;
    // QA stress knob: clone the default asset entity N times onto a grid to
    // measure submission cost scaling with instance count.
    std::uint32_t instanceCount = 1;
    azurerender::RenderSettings renderSettings;
    bool hudEnabled = false;
    bool technicalSequence = false;
    std::string qaCamera;
    std::string qaLight;
    bool qaLightScan = false;
    bool qaAnimation = false;
    std::string qaEffect;
    std::string qaEffectState;
    std::string qaIsolation;
    std::string renderPathName;
    bool editorMode = false;
    std::string editorScenePath;
    std::shared_ptr<azurerender::EditorSession> editorSession;
    // Full scene document when the run was started from an .azscene file.
    // Single-asset runs leave this empty and the app builds a one-node scene
    // from assetPath.
    std::optional<azurerender::SceneDocument> sceneDocument;
};
