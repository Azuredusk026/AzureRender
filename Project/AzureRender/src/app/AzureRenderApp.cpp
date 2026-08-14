#include "AzureRenderApp.hpp"
#include "editor/EditorContext.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "platform/GlfwFrontend.hpp"
#include "render/RendererCore.hpp"
#include "AzureRenderInternal.hpp"

#include <stb_easy_font.h>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <set>
#include <stdexcept>
#include <utility>

using namespace azurerender::internal;

namespace {

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

void appendShowcasePlatform(LoadedAsset& asset) {
    constexpr std::uint32_t kSegments = 96;
    const Vector3 boundsCenter = {
        (asset.boundsMin[0] + asset.boundsMax[0]) * 0.5F,
        (asset.boundsMin[1] + asset.boundsMax[1]) * 0.5F,
        (asset.boundsMin[2] + asset.boundsMax[2]) * 0.5F,
    };
    const float largestExtent = std::max({
        asset.boundsMax[0] - asset.boundsMin[0],
        asset.boundsMax[1] - asset.boundsMin[1],
        asset.boundsMax[2] - asset.boundsMin[2],
    });
    const float radius = largestExtent * 0.40F;
    const float topY = asset.boundsMin[1] - largestExtent * 0.004F;
    const float bottomY = topY - largestExtent * 0.050F;

    AssetMaterial platformMaterial;
    platformMaterial.name = "AzureRender_ShowcasePlatform";
    platformMaterial.materialClass = AssetMaterialClass::Showcase;
    platformMaterial.materialFeatures = 0;
    platformMaterial.materialProfileVersion = 1;
    platformMaterial.materialProfileExplicit = true;
    platformMaterial.baseColorWidth = 2;
    platformMaterial.baseColorHeight = 2;
    platformMaterial.baseColorPixels = {
        38, 50, 66, 255, 38, 50, 66, 255,
        38, 50, 66, 255, 38, 50, 66, 255,
    };
    platformMaterial.normalWidth = 2;
    platformMaterial.normalHeight = 2;
    platformMaterial.normalPixels = {
        128, 128, 255, 255, 128, 128, 255, 255,
        128, 128, 255, 255, 128, 128, 255, 255,
    };
    platformMaterial.metallicRoughnessWidth = 2;
    platformMaterial.metallicRoughnessHeight = 2;
    platformMaterial.metallicRoughnessPixels = {
        255, 210, 28, 255, 255, 210, 28, 255,
        255, 210, 28, 255, 255, 210, 28, 255,
    };
    platformMaterial.specularEmissiveWidth = 2;
    platformMaterial.specularEmissiveHeight = 2;
    platformMaterial.specularEmissivePixels = {
        0, 0, 0, 96, 0, 0, 0, 96,
        0, 0, 0, 96, 0, 0, 0, 96,
    };
    platformMaterial.styleMaskWidth = 2;
    platformMaterial.styleMaskHeight = 2;
    platformMaterial.styleMaskPixels.assign(16, 0);
    for (std::size_t alpha = 3; alpha < 16; alpha += 4) {
        platformMaterial.styleMaskPixels[alpha] = 255;
    }
    platformMaterial.matcapWidth = 2;
    platformMaterial.matcapHeight = 2;
    platformMaterial.matcapPixels = platformMaterial.styleMaskPixels;
    platformMaterial.hairDataWidth = 2;
    platformMaterial.hairDataHeight = 2;
    platformMaterial.hairDataPixels.assign(16, 128);
    platformMaterial.showcasePlatform = 1.0F;
    platformMaterial.doubleSided = true;

    const std::uint32_t materialIndex =
        static_cast<std::uint32_t>(asset.materials.size());
    asset.materials.push_back(std::move(platformMaterial));
    const std::uint32_t firstVertex =
        static_cast<std::uint32_t>(asset.vertices.size());
    const std::uint32_t firstIndex =
        static_cast<std::uint32_t>(asset.indices.size());

    asset.vertices.push_back({
        {boundsCenter[0], topY, boundsCenter[2]},
        {0.0F, 1.0F, 0.0F},
        {1.0F, 0.0F, 0.0F, 1.0F},
        {0.5F, 0.5F},
    });
    constexpr float kTwoPi = 6.28318530717958647692F;
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const float angle =
            kTwoPi * static_cast<float>(segment)
            / static_cast<float>(kSegments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        asset.vertices.push_back({
            {
                boundsCenter[0] + cosine * radius,
                topY,
                boundsCenter[2] + sine * radius,
            },
            {0.0F, 1.0F, 0.0F},
            {1.0F, 0.0F, 0.0F, 1.0F},
            {cosine * 0.5F + 0.5F, sine * 0.5F + 0.5F},
        });
    }
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const std::uint32_t next = (segment + 1) % kSegments;
        asset.indices.push_back(firstVertex);
        asset.indices.push_back(firstVertex + 1 + next);
        asset.indices.push_back(firstVertex + 1 + segment);
    }

    const std::uint32_t sideFirst =
        static_cast<std::uint32_t>(asset.vertices.size());
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const float angle =
            kTwoPi * static_cast<float>(segment)
            / static_cast<float>(kSegments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const float u =
            static_cast<float>(segment) / static_cast<float>(kSegments);
        for (const float y : {topY, bottomY}) {
            asset.vertices.push_back({
                {
                    boundsCenter[0] + cosine * radius,
                    y,
                    boundsCenter[2] + sine * radius,
                },
                {cosine, 0.0F, sine},
                {-sine, 0.0F, cosine, 1.0F},
                {u, y == topY ? 0.0F : 1.0F},
            });
        }
    }
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const std::uint32_t next = (segment + 1) % kSegments;
        const std::uint32_t top = sideFirst + segment * 2;
        const std::uint32_t bottom = top + 1;
        const std::uint32_t nextTop = sideFirst + next * 2;
        const std::uint32_t nextBottom = nextTop + 1;
        asset.indices.insert(
            asset.indices.end(),
            {top, nextTop, bottom, bottom, nextTop, nextBottom});
    }

    asset.primitives.push_back({
        firstIndex,
        static_cast<std::uint32_t>(asset.indices.size()) - firstIndex,
        materialIndex,
        {boundsCenter[0], (topY + bottomY) * 0.5F, boundsCenter[2]},
    });
}

VkResult createDebugUtilsMessenger(
    const VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger) {
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    return function != nullptr
        ? function(instance, createInfo, nullptr, messenger)
        : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void destroyDebugUtilsMessenger(
    const VkInstance instance,
    const VkDebugUtilsMessengerEXT messenger) {
    const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (function != nullptr) {
        function(instance, messenger, nullptr);
    }
}

}  // namespace

AzureRenderApp::AzureRenderApp() = default;

AzureRenderApp::~AzureRenderApp() {
    cleanup();
}

void AzureRenderApp::run(
    const AzureRenderOptions& options) {
    runOptions_ = options;
    azurerender::SceneView sceneView;
    sceneView.assetPath = options.assetPath.empty()
        ? std::string(AZURERENDER_ASSET_DIR) + "/test_model.gltf"
        : options.assetPath;
    sceneView.renderSettings = options.renderSettings;
    azurerender::RendererCoreBoundary::validateSceneView(sceneView);
    azurerender::validateRenderSettings(runOptions_.renderSettings);
    renderSettings_ = runOptions_.renderSettings;
#if defined(AZURERENDER_HAS_IMGUI)
    editorUiEnabled_ = runOptions_.editorContext != nullptr;
#endif
    fixedSimulation_ = runOptions_.captureFrameLimit > 0;
    fixedSimulationStarted_ = false;
    fixedDeltaSeconds_ = 1.0F / static_cast<float>(runOptions_.captureFps);
    capturedFrames_ = 0;
    if (fixedSimulation_) {
        prepareCaptureDirectory();
    }
    if (!runOptions_.gpuTimingOutput.empty()
        && std::filesystem::exists(runOptions_.gpuTimingOutput)) {
        throw std::runtime_error(
            "GPU timing output already exists; refusing to overwrite: "
            + runOptions_.gpuTimingOutput);
    }
    initWindow();
    initVulkan(runOptions_.assetPath);
    if (runOptions_.portfolioMode) {
        activatePortfolioOrbit();
    }
    renderSettings_ = runOptions_.renderSettings;
    if (runOptions_.editorContext != nullptr) {
        runOptions_.editorContext->attachRenderSettings(renderSettings_);
    }
    hudEnabled_ = runOptions_.hudEnabled;
#if !defined(AZURERENDER_HAS_IMGUI)
    hudEnabled_ = hudEnabled_ || runOptions_.editorMode;
#endif
    configureQaHarness();
    constexpr std::array<const char*, 5> kDiagnosticNames = {
        "Beauty",
        "World Normal",
        "Internal Outline",
        "Shadow Map",
        "Depth",
    };
    std::cout
        << "Diagnostic view: " << kDiagnosticNames[renderSettings_.diagnosticView]
        << ", stylized lighting: "
        << (renderSettings_.stylizedLightingEnabled ? "on" : "off")
        << ", internal outline: "
        << (renderSettings_.innerOutlineEnabled ? "on" : "off")
        << ", HUD: " << (hudEnabled_ ? "on" : "off")
        << '\n';
    mainLoop(runOptions_.smokeFrameLimit);
    if (runOptions_.editorContext != nullptr) {
        runOptions_.editorContext->detachRenderSettings();
    }
}

void AzureRenderApp::initWindow() {
    azurerender::GlfwFrontendConfig config;
    config.width = runOptions_.width;
    config.height = runOptions_.height;
    config.title = runOptions_.editorMode
        ? "AzureRender Editor Preview"
        : "AzureRender - Stylized Vulkan Renderer";
    config.resizable = !fixedSimulation_;
    config.userPointer = this;
    config.framebufferSizeCallback = framebufferResizeCallback;
    config.keyCallback = keyCallback;
    frontend_ = std::make_unique<azurerender::GlfwFrontend>(config);
    lastRotationTime_ = frontend_->timeSeconds();
    std::cout
        << "Controls: Space pause/resume, R auto rotate, "
        << "1/2/3/4 full-body angles, 5 face close-up, "
        << "6 portfolio orbit, "
        << "Left/Right fine rotate, "
        << "F1/F2/F3 showcase presets, "
        << "F4 animation pause/resume, F11 animation restart, "
        << "7/8 previous/next animation, 9 timeline status, "
        << "0 diagnostic view, "
        << "H HUD, "
        << "F10 inner outlines, "
        << "F9 style toggle, F7/F8 mask strength, "
        << "F5/F6 band threshold, "
        << "F12 screenshot\n";
}

void AzureRenderApp::initVulkan(const std::string& assetPath) {
    std::cout << "Validation layer: " << (kEnableValidation ? "enabled" : "disabled") << '\n';
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createDescriptorSetLayout();
    createPostProcessDescriptorSetLayout();
    resolvedAssetPath_ = assetPath.empty()
        ? std::string(AZURERENDER_ASSET_DIR) + "/test_model.gltf"
        : assetPath;
    asset_ = loadGltfAsset(resolvedAssetPath_);
    for (const AssetMaterial& material : asset_.materials) {
        if (!material.faceSdf.present) {
            continue;
        }
        if (faceSdfHeadNode_.has_value()
            && *faceSdfHeadNode_ != material.faceSdf.headNode) {
            throw std::runtime_error(
                "Face SDF materials must share one headNode");
        }
        faceSdfHeadNode_ = material.faceSdf.headNode;
        std::cout
            << "Face SDF: material=" << material.name
            << ", texture=" << material.faceSdf.width << 'x'
            << material.faceSdf.height
            << ", headNode=" << material.faceSdf.headNodeName << '\n';
    }
    appendShowcasePlatform(asset_);
    std::cout << "Asset path: " << resolvedAssetPath_ << '\n';
    std::cout << "Loaded asset: " << asset_.vertices.size() << " vertices, "
              << asset_.indices.size() << " indices, "
              << asset_.primitives.size() << " primitives, "
              << asset_.materials.size() << " materials\n";
    std::cout << "Material Class v1 inventory:\n";
    for (std::size_t index = 0; index < asset_.materials.size(); ++index) {
        const AssetMaterial& material = asset_.materials[index];
        std::cout
            << "  [" << index << "] " << material.name
            << " -> " << assetMaterialClassName(material.materialClass)
            << ", flags=0x" << std::hex << material.materialFeatures
            << std::dec
            << (material.materialProfileExplicit
                ? ", source=asset-extras"
                : ", source=fallback/inferred")
            << '\n';
    }
    std::cout << "Skinning: "
              << (asset_.hasSkin ? "enabled" : "static fallback")
              << ", " << asset_.jointMatrices.size() << " joint matrices\n";
    std::cout << "Animations: " << asset_.animations.size();
    if (!asset_.animations.empty()) {
        const auto& animation = asset_.animations.front();
        std::cout << " (playing \"" << animation.name << "\", "
                  << animation.endTime - animation.startTime
                  << " s loop)";
    }
    std::cout << '\n';
    animationIndex_ = 0;
    animationTime_ = 0.0F;
    animationPlaying_ = true;
    lastRotationTime_ = frontend_->timeSeconds();
    createVertexBuffer();
    createIndexBuffer();
    createTexture();
    createUniformBuffers();
    createJointBuffers();
    createHudBuffers();
    createShadowResources();
    createDescriptorPool();
    createDescriptorSets();
    createSwapchain();
    if (fixedSimulation_
        && (swapchainExtent_.width != runOptions_.width
            || swapchainExtent_.height != runOptions_.height)) {
        throw std::runtime_error(
            "Capture framebuffer extent does not match requested resolution: "
            + std::to_string(swapchainExtent_.width) + "x"
            + std::to_string(swapchainExtent_.height) + " instead of "
            + std::to_string(runOptions_.width) + "x"
            + std::to_string(runOptions_.height));
    }
    createImageViews();
    createEditorViewportResources();
    createSceneColorResources();
    createDepthResources();
    createNormalResources();
    createRenderPass();
    createPostProcessRenderPass();
    createEditorUiRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createPostProcessFramebuffers();
    createEditorUiFramebuffers();
    createPostProcessDescriptorSets();
    createSwapchainSemaphores();
    createCommandBuffers();
    createSyncObjects();
    createTimestampQueryPools();
    initEditorUi();
}

void AzureRenderApp::initEditorUi() {
    if (!editorUiEnabled_) {
        return;
    }
    if (editorLayer_ == nullptr) {
        editorLayer_ = std::make_unique<azurerender::ImGuiEditorLayer>(
            runOptions_.editorContext);
    }
    editorLayer_->initialize(
        frontend_->nativeHandle(),
        instance_,
        physicalDevice_,
        device_,
        graphicsQueueFamily_,
        graphicsQueue_,
        editorUiRenderPass_,
        static_cast<std::uint32_t>(swapchainImages_.size()));
    editorLayer_->setViewportImages(
        editorViewportSampler_,
        editorViewportImageViews_,
        renderExtent_.width,
        renderExtent_.height);
}

void AzureRenderApp::mainLoop(const std::uint64_t smokeFrameLimit) {
    std::uint64_t renderedFrames = 0;
    while (!frontend_->shouldClose()) {
        frontend_->pollEvents();
        drawFrame();
        ++renderedFrames;
        if (smokeFrameLimit > 0 && renderedFrames >= smokeFrameLimit) {
            frontend_->requestClose();
        }
        if (fixedSimulation_
            && capturedFrames_ >= runOptions_.captureFrameLimit) {
            frontend_->requestClose();
        }
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    if (runOptions_.gpuTimingEnabled) {
        for (std::size_t frame = 0;
             frame < kMaxFramesInFlight;
             ++frame) {
            collectGpuTiming(frame);
        }
        printGpuTimingSummary();
    }
    std::cout << "Rendered frames: " << renderedFrames << '\n';
    if (fixedSimulation_) {
        writeCaptureManifest(renderedFrames);
        std::cout
            << "Captured frames: " << capturedFrames_ << " at "
            << runOptions_.captureFps << " fps\n";
    }
}

void AzureRenderApp::activatePortfolioOrbit() {
    constexpr float kPi = 3.14159265358979323846F;
    rotationAngle_ = kPi * 0.25F;
    rotationSpeed_ = 0.16F;
    cameraPosition_ = {2.32F, 1.80F, 2.66F};
    cameraTarget_ = {0.0F, 0.05F, 0.0F};
    renderSettings_.showcasePreset = 1;
    renderSettings_.stylizedLightingEnabled = true;
    renderSettings_.innerOutlineEnabled = true;
    autoRotate_ = true;
    if (!asset_.animations.empty()) {
        animationTime_ = 0.0F;
        animationPlaying_ = true;
    }
    std::cout
        << "View preset: 6 (portfolio orbit, Endfield Industrial)\n";
}

void AzureRenderApp::configureQaHarness() {
    qaHarnessEnabled_ = !runOptions_.qaCamera.empty()
        || !runOptions_.qaLight.empty()
        || !runOptions_.qaEffect.empty()
        || !runOptions_.qaEffectState.empty()
        || !runOptions_.qaIsolation.empty();
    if (!qaHarnessEnabled_) {
        return;
    }

    constexpr float kPi = 3.14159265358979323846F;
    qaCameraName_ = runOptions_.qaCamera.empty()
        ? "full-body-front"
        : runOptions_.qaCamera;
    if (qaCameraName_ == "full-body-front") {
        rotationAngle_ = kPi * 0.25F;
        cameraPosition_ = {2.25F, 1.75F, 2.55F};
        cameraTarget_ = {0.0F, 0.10F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "face-front") {
        rotationAngle_ = kPi * 0.25F;
        cameraPosition_ = {0.915F, 1.507F, 1.046F};
        cameraTarget_ = {0.0F, 0.82F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "face-three-quarter") {
        rotationAngle_ = kPi * 0.40F;
        cameraPosition_ = {0.915F, 1.507F, 1.046F};
        cameraTarget_ = {0.0F, 0.82F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "back-detail") {
        rotationAngle_ = kPi * 1.25F;
        cameraPosition_ = {1.45F, 1.58F, 1.72F};
        cameraTarget_ = {0.0F, 0.38F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "lighting-sweep") {
        rotationAngle_ = kPi * 0.25F;
        cameraPosition_ = {2.25F, 1.75F, 2.55F};
        cameraTarget_ = {0.0F, 0.10F, 0.0F};
        rotationSpeed_ = 0.60F;
        autoRotate_ = true;
    } else {
        throw std::invalid_argument(
            "Unknown --qa-camera: " + qaCameraName_);
    }

    qaLightName_ = runOptions_.qaLight.empty()
        ? "stylized-key"
        : runOptions_.qaLight;
    if (qaLightName_ == "neutral-material") {
        renderSettings_.showcasePreset = 2;
    } else if (qaLightName_ == "stylized-key") {
        renderSettings_.showcasePreset = 1;
    } else if (qaLightName_ == "specular-rim") {
        renderSettings_.showcasePreset = 3;
    } else if (qaLightName_ == "rear-emissive") {
        renderSettings_.showcasePreset = 4;
    } else {
        throw std::invalid_argument(
            "Unknown --qa-light: " + qaLightName_);
    }

    qaEffectName_ = runOptions_.qaEffect.empty()
        ? "none"
        : runOptions_.qaEffect;
    constexpr std::array<const char*, 11> kEffectNames = {
        "none", "toon", "shadow", "hair-kk", "rim", "specular",
        "emissive", "outline", "face-sdf", "overlay", "bloom",
    };
    const auto effect = std::find(
        kEffectNames.begin(), kEffectNames.end(), qaEffectName_);
    if (effect == kEffectNames.end()) {
        throw std::invalid_argument(
            "Unknown --qa-effect: " + qaEffectName_);
    }
    qaEffectMode_ = static_cast<std::uint32_t>(
        std::distance(kEffectNames.begin(), effect));

    qaEffectStateName_ = runOptions_.qaEffectState.empty()
        ? "enabled"
        : runOptions_.qaEffectState;
    if (qaEffectStateName_ != "enabled"
        && qaEffectStateName_ != "disabled"
        && qaEffectStateName_ != "isolation") {
        throw std::invalid_argument(
            "Unknown --qa-effect-state: " + qaEffectStateName_);
    }
    qaEffectEnabled_ = qaEffectStateName_ != "disabled";

    qaIsolationName_ = runOptions_.qaIsolation.empty()
        ? "beauty"
        : runOptions_.qaIsolation;
    constexpr std::array<const char*, 20> kIsolationNames = {
        "beauty", "albedo", "world-normal", "depth", "diffuse-band",
        "shadow-visibility", "hair-kk", "rim", "specular", "emissive",
        "outline", "shadow-map", "material-id", "style-mask", "ambient",
        "direct-diffuse", "shadow-tint", "face-sdf", "overlay", "bloom",
    };
    const auto isolation = std::find(
        kIsolationNames.begin(), kIsolationNames.end(), qaIsolationName_);
    if (isolation == kIsolationNames.end()) {
        throw std::invalid_argument(
            "Unknown --qa-isolation: " + qaIsolationName_);
    }
    const std::uint32_t isolationIndex = static_cast<std::uint32_t>(
        std::distance(kIsolationNames.begin(), isolation));
    constexpr std::array<std::uint32_t, 20> kPostProcessViews = {
        0, 0, 1, 4, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    constexpr std::array<std::uint32_t, 20> kShaderIsolationModes = {
        0, 1, 0, 0, 2, 3, 4, 5, 6, 7, 0, 0, 8, 9, 10, 11, 12, 13, 14, 15,
    };
    renderSettings_.diagnosticView = kPostProcessViews[isolationIndex];
    qaIsolationMode_ = kShaderIsolationModes[isolationIndex];

    if (qaEffectStateName_ == "isolation") {
        constexpr std::array<std::uint32_t, 11> kEffectIsolationModes = {
            0, 2, 3, 4, 5, 6, 7, 0, 13, 14, 15,
        };
        qaIsolationMode_ = kEffectIsolationModes[qaEffectMode_];
        qaIsolationName_ = qaEffectName_;
        if (qaEffectName_ == "outline") {
            renderSettings_.diagnosticView = 2;
            qaIsolationName_ = "outline";
        } else {
            renderSettings_.diagnosticView = 0;
        }
    }
    if (qaIsolationMode_ > 0) {
        // Component isolation must not be contaminated by either outline path.
        renderSettings_.innerOutlineEnabled = false;
        renderSettings_.silhouetteOutlineEnabled = false;
    }
    if (qaEffectName_ == "outline") {
        renderSettings_.innerOutlineEnabled = qaEffectEnabled_;
        renderSettings_.silhouetteOutlineEnabled = qaEffectEnabled_;
    }
    if (!asset_.animations.empty() && qaCameraName_ != "lighting-sweep") {
        animationTime_ = 0.0F;
        animationPlaying_ = false;
    }
    std::cout
        << "CQ-0 QA: camera=" << qaCameraName_
        << ", light=" << qaLightName_
        << ", effect=" << qaEffectName_
        << ", state=" << qaEffectStateName_
        << ", isolation=" << qaIsolationName_
        << '\n';
}

void AzureRenderApp::updateTechnicalSequenceState(
    const std::uint64_t frameIndex) {
    if (!runOptions_.technicalSequence) {
        return;
    }
    const std::uint64_t chapterFrames =
        runOptions_.captureFrameLimit / 5;
    const std::uint32_t chapter = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(
            frameIndex / chapterFrames,
            4));
    constexpr std::array<std::uint32_t, 5> kDiagnosticViews = {
        0,
        1,
        2,
        3,
        0,
    };
    constexpr std::array<bool, 5> kHudStates = {
        false,
        false,
        false,
        false,
        true,
    };
    constexpr std::array<const char*, 5> kChapterNames = {
        "Beauty",
        "World Normal",
        "Internal Outline",
        "Shadow Map",
        "Beauty + HUD",
    };
    renderSettings_.diagnosticView = kDiagnosticViews[chapter];
    hudEnabled_ = kHudStates[chapter];
    renderSettings_.stylizedLightingEnabled = true;
    renderSettings_.innerOutlineEnabled = true;
    if (technicalSequenceChapter_ != chapter) {
        technicalSequenceChapter_ = chapter;
        const double chapterTime =
            static_cast<double>(frameIndex)
            / static_cast<double>(runOptions_.captureFps);
        std::cout
            << "Technical chapter " << chapter + 1 << "/5: "
            << kChapterNames[chapter] << " at frame "
            << frameIndex << " (" << chapterTime << " s)\n";
    }
}

void AzureRenderApp::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (editorLayer_ != nullptr) {
            editorLayer_->shutdownVulkan();
        }
        cleanupSwapchain();

        for (const auto semaphore : imageAvailableSemaphores_) {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
        for (const auto fence : inFlightFences_) {
            vkDestroyFence(device_, fence, nullptr);
        }
        for (const VkQueryPool pool : timestampQueryPools_) {
            vkDestroyQueryPool(device_, pool, nullptr);
        }
        timestampQueryPools_.clear();

        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        }
        if (screenAttachmentSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, screenAttachmentSampler_, nullptr);
        }
        if (postProcessDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(
                device_,
                postProcessDescriptorSetLayout_,
                nullptr);
        }
        for (std::size_t index = 0; index < uniformBuffers_.size(); ++index) {
            if (uniformBufferMapped_[index] != nullptr) {
                vkUnmapMemory(device_, uniformBufferMemories_[index]);
            }
            vkDestroyBuffer(device_, uniformBuffers_[index], nullptr);
            vkFreeMemory(device_, uniformBufferMemories_[index], nullptr);
        }
        for (std::size_t index = 0; index < jointBuffers_.size(); ++index) {
            if (jointBufferMapped_[index] != nullptr) {
                vkUnmapMemory(device_, jointBufferMemories_[index]);
            }
            vkDestroyBuffer(device_, jointBuffers_[index], nullptr);
            vkFreeMemory(device_, jointBufferMemories_[index], nullptr);
        }
        for (std::size_t index = 0; index < hudVertexBuffers_.size(); ++index) {
            if (hudVertexBufferMapped_[index] != nullptr) {
                vkUnmapMemory(
                    device_,
                    hudVertexBufferMemories_[index]);
            }
            vkDestroyBuffer(device_, hudVertexBuffers_[index], nullptr);
            vkFreeMemory(
                device_,
                hudVertexBufferMemories_[index],
                nullptr);
        }
        if (indexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, indexBuffer_, nullptr);
            vkFreeMemory(device_, indexBufferMemory_, nullptr);
        }
        if (vertexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, vertexBuffer_, nullptr);
            vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        }
        for (const auto& material : gpuMaterials_) {
            for (const GpuTexture* texture : {
                     &material.baseColor,
                     &material.normal,
                     &material.metallicRoughness,
                     &material.specularEmissive,
                     &material.styleMask,
                     &material.matcap,
                     &material.hairData,
                     &material.faceSdf}) {
                vkDestroySampler(device_, texture->sampler, nullptr);
                vkDestroyImageView(device_, texture->view, nullptr);
                vkDestroyImage(device_, texture->image, nullptr);
                vkFreeMemory(device_, texture->memory, nullptr);
            }
        }
        vkDestroySampler(device_, environmentTexture_.sampler, nullptr);
        vkDestroyImageView(device_, environmentTexture_.view, nullptr);
        vkDestroyImage(device_, environmentTexture_.image, nullptr);
        vkFreeMemory(device_, environmentTexture_.memory, nullptr);
        vkDestroySampler(device_, toonRampTexture_.sampler, nullptr);
        vkDestroyImageView(device_, toonRampTexture_.view, nullptr);
        vkDestroyImage(device_, toonRampTexture_.image, nullptr);
        vkFreeMemory(device_, toonRampTexture_.memory, nullptr);
        if (shadowFramebuffer_ != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, shadowFramebuffer_, nullptr);
        }
        if (shadowRenderPass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, shadowRenderPass_, nullptr);
        }
        if (shadowSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, shadowSampler_, nullptr);
        }
        if (shadowImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, shadowImageView_, nullptr);
        }
        if (shadowImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, shadowImage_, nullptr);
        }
        if (shadowImageMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, shadowImageMemory_, nullptr);
        }
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        }
        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (debugMessenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        destroyDebugUtilsMessenger(instance_, debugMessenger_);
        debugMessenger_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    frontend_.reset();
    editorLayer_.reset();
}

void AzureRenderApp::createInstance() {
    if (kEnableValidation && !checkValidationLayerSupport()) {
        throw std::runtime_error("VK_LAYER_KHRONOS_validation is unavailable");
    }

    VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "AzureRender";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.pEngineName = "AzureRender";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    const auto extensions = requiredInstanceExtensions();
    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (kEnableValidation) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    vkCheck(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
}

void AzureRenderApp::setupDebugMessenger() {
    if (!kEnableValidation) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    vkCheck(
        createDebugUtilsMessenger(instance_, &createInfo, &debugMessenger_),
        "vkCreateDebugUtilsMessengerEXT");
}

void AzureRenderApp::createSurface() {
    surface_ = frontend_->createSurface(instance_);
}

void AzureRenderApp::pickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkCheck(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU was found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkCheck(
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()),
        "vkEnumeratePhysicalDevices");

    for (const auto device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        const auto iterator = std::find_if(
            devices.begin(),
            devices.end(),
            [this](const VkPhysicalDevice device) { return isDeviceSuitable(device); });
        if (iterator != devices.end()) {
            physicalDevice_ = *iterator;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("No GPU supports the required graphics/present queues and swapchain");
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    selectedGpuName_ = properties.deviceName;
    timestampPeriodNanoseconds_ = properties.limits.timestampPeriod;
    const QueueFamilyIndices queueIndices =
        findQueueFamilies(physicalDevice_);
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice_,
        &queueFamilyCount,
        nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(
        queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice_,
        &queueFamilyCount,
        queueProperties.data());
    timestampValidBits_ =
        queueProperties.at(*queueIndices.graphics).timestampValidBits;
    VkFormatProperties hdrSceneColorProperties{};
    vkGetPhysicalDeviceFormatProperties(
        physicalDevice_,
        kHdrSceneColorFormat,
        &hdrSceneColorProperties);
    constexpr VkFormatFeatureFlags kRequiredHdrSceneColorFeatures =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
        | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    hdrSceneColorFormatSupported_ =
        (hdrSceneColorProperties.optimalTilingFeatures
         & kRequiredHdrSceneColorFeatures)
        == kRequiredHdrSceneColorFeatures;
    if (runOptions_.gpuTimingEnabled && timestampValidBits_ == 0) {
        throw std::runtime_error(
            "Selected graphics queue does not support timestamp queries");
    }
    std::cout << "Selected GPU: " << selectedGpuName_ << '\n';
    std::cout
        << "HDR scene color candidate: VK_FORMAT_R16G16B16A16_SFLOAT ("
        << (hdrSceneColorFormatSupported_ ? "supported" : "unsupported")
        << ")\n";
    if (!hdrSceneColorFormatSupported_) {
        throw std::runtime_error(
            "Selected GPU does not support the required RGBA16F sampled, "
            "color-attachment, and color-blend features");
    }
    if (runOptions_.gpuTimingEnabled) {
        std::cout
            << "GPU timing: enabled, timestamp period "
            << timestampPeriodNanoseconds_ << " ns, "
            << timestampValidBits_ << " valid bits\n";
    }
}

void AzureRenderApp::createLogicalDevice() {
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    graphicsQueueFamily_ = *indices.graphics;
    const std::set uniqueFamilies = {*indices.graphics, *indices.present};
    const float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueFamilies.size());

    for (const std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    vkCheck(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, *indices.graphics, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, *indices.present, 0, &presentQueue_);
}

void AzureRenderApp::createSwapchain() {
    const SwapchainSupport support = querySwapchainSupport(physicalDevice_);
    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    const VkExtent2D extent = chooseExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    if ((support.capabilities.supportedUsageFlags
         & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
        throw std::runtime_error(
            "Surface swapchain images do not support screenshot transfer source");
    }
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    const std::array queueFamilies = {*indices.graphics, *indices.present};
    if (indices.graphics != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilies.size());
        createInfo.pQueueFamilyIndices = queueFamilies.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    vkCheck(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "vkCreateSwapchainKHR");
    vkCheck(
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr),
        "vkGetSwapchainImagesKHR");
    swapchainImages_.resize(imageCount);
    vkCheck(
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data()),
        "vkGetSwapchainImagesKHR");

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
    if (editorUiEnabled_
        && requestedEditorViewportExtent_.width > 0
        && requestedEditorViewportExtent_.height > 0) {
        renderExtent_.width = std::clamp(
            requestedEditorViewportExtent_.width,
            std::min(64U, extent.width),
            extent.width);
        renderExtent_.height = std::clamp(
            requestedEditorViewportExtent_.height,
            std::min(64U, extent.height),
            extent.height);
    } else {
        renderExtent_ = extent;
    }
    if (editorUiEnabled_) {
        std::cout << "Editor viewport render extent: "
                  << renderExtent_.width << 'x' << renderExtent_.height
                  << '\n';
    }
}

void AzureRenderApp::createSwapchainSemaphores() {
    renderFinishedSemaphores_.resize(swapchainImages_.size());
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (auto& semaphore : renderFinishedSemaphores_) {
        vkCheck(
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore),
            "vkCreateSemaphore(render finished)");
    }
}

void AzureRenderApp::createCommandPool() {
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    VkCommandPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = *indices.graphics;
    vkCheck(vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_), "vkCreateCommandPool");
}

void AzureRenderApp::createCommandBuffers() {
    commandBuffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers_.data()),
        "vkAllocateCommandBuffers");
}

void AzureRenderApp::createSyncObjects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        vkCheck(
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[index]),
            "vkCreateSemaphore");
        vkCheck(
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[index]),
            "vkCreateFence");
    }
}

void AzureRenderApp::createTimestampQueryPools() {
    if (!runOptions_.gpuTimingEnabled) {
        return;
    }
    timestampQueryPools_.resize(kMaxFramesInFlight);
    VkQueryPoolCreateInfo createInfo{
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = kTimestampQueryCount;
    for (VkQueryPool& pool : timestampQueryPools_) {
        vkCheck(
            vkCreateQueryPool(device_, &createInfo, nullptr, &pool),
            "vkCreateQueryPool(timestamp)");
    }
}

void AzureRenderApp::keyCallback(
    GLFWwindow* window,
    const int key,
    const int scancode,
    const int action,
    const int modifiers) {
    (void)scancode;
    (void)modifiers;
    auto* application = static_cast<AzureRenderApp*>(
        glfwGetWindowUserPointer(window));
    if (application == nullptr) {
        return;
    }
    if (application->editorLayer_ != nullptr) {
        if (!application->editorLayer_->acceptsViewportShortcuts()) {
            return;
        }
    }

    constexpr float kPi = 3.14159265358979323846F;
    constexpr float kFineStep = kPi / 36.0F;
    const auto printAnimationStatus = [application]() {
        if (application->asset_.animations.empty()) {
            std::cout << "Animation: unavailable for this asset\n";
            return;
        }
        const AssetAnimation& animation =
            application->asset_.animations[application->animationIndex_];
        const float duration =
            std::max(animation.endTime - animation.startTime, 0.0F);
        const float playhead = duration > 1.0e-8F
            ? std::fmod(
                  std::max(application->animationTime_, 0.0F),
                  duration)
            : 0.0F;
        const std::string displayName = animation.name.empty()
            ? "<unnamed>"
            : animation.name;
        std::cout
            << "Animation [" << application->animationIndex_ + 1
            << '/' << application->asset_.animations.size() << "]: \""
            << displayName << "\", " << playhead << " / " << duration
            << " s, "
            << (application->animationPlaying_ ? "playing" : "paused")
            << '\n';
    };
    const auto selectAnimation =
        [application, &printAnimationStatus](const int direction) {
        const std::size_t count = application->asset_.animations.size();
        if (count == 0) {
            printAnimationStatus();
            return;
        }
        if (direction < 0) {
            application->animationIndex_ =
                (application->animationIndex_ + count - 1) % count;
        } else {
            application->animationIndex_ =
                (application->animationIndex_ + 1) % count;
        }
        application->animationTime_ = 0.0F;
        application->animationPlaying_ = true;
        printAnimationStatus();
    };
    if (action == GLFW_PRESS) {
        if (application->runOptions_.editorContext != nullptr
            && key == GLFW_KEY_TAB
            && application->runOptions_.editorContext->selectedNode() != nullptr) {
            application->runOptions_.editorContext->selectNextNode();
            std::cout << "Editor selected node: "
                      << application->runOptions_.editorContext
                             ->selectedNode()->name
                      << '\n';
        } else if (key == GLFW_KEY_SPACE) {
            application->autoRotate_ = !application->autoRotate_;
            std::cout << "Auto rotate: "
                      << (application->autoRotate_ ? "on" : "paused")
                      << '\n';
        } else if (key == GLFW_KEY_R) {
            application->autoRotate_ = true;
            application->rotationSpeed_ = 0.65F;
            std::cout << "Auto rotate: on (standard speed)\n";
        } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
            application->rotationAngle_ =
                kPi * 0.25F
                + static_cast<float>(key - GLFW_KEY_1) * kPi * 0.5F;
            application->cameraPosition_ = {2.8F, 2.1F, 3.2F};
            application->cameraTarget_ = {0.0F, 0.0F, 0.0F};
            application->autoRotate_ = false;
            application->rotationSpeed_ = 0.65F;
            std::cout << "Angle preset: " << key - GLFW_KEY_0 << '\n';
        } else if (key == GLFW_KEY_5) {
            application->rotationAngle_ = kPi * 0.25F;
            application->cameraPosition_ = {0.915F, 1.507F, 1.046F};
            application->cameraTarget_ = {0.0F, 0.82F, 0.0F};
            application->autoRotate_ = false;
            application->rotationSpeed_ = 0.65F;
            std::cout << "View preset: 5 (face close-up)\n";
        } else if (key == GLFW_KEY_6) {
            application->activatePortfolioOrbit();
            printAnimationStatus();
        } else if (key == GLFW_KEY_7) {
            selectAnimation(-1);
        } else if (key == GLFW_KEY_8) {
            selectAnimation(1);
        } else if (key == GLFW_KEY_9) {
            printAnimationStatus();
        } else if (key == GLFW_KEY_0) {
            application->renderSettings_.diagnosticView =
                (application->renderSettings_.diagnosticView + 1) % 5;
            constexpr std::array<const char*, 5> kDiagnosticNames = {
                "Beauty",
                "World Normal",
                "Internal Outline",
                "Shadow Map",
                "Depth",
            };
            std::cout
                << "Diagnostic view: "
                << kDiagnosticNames[application->renderSettings_.diagnosticView]
                << '\n';
        } else if (key == GLFW_KEY_H) {
            application->hudEnabled_ = !application->hudEnabled_;
            std::cout
                << "HUD: "
                << (application->hudEnabled_ ? "on" : "off")
                << '\n';
            if (application->hudEnabled_
                && !application->runOptions_.gpuTimingEnabled) {
                std::cout
                    << "HUD GPU pass timing is disabled; restart with "
                    << "--hud or --gpu-timing to enable it\n";
            }
        } else if (key == GLFW_KEY_F12) {
            application->screenshotRequested_ = true;
            std::cout << "Screenshot requested\n";
        } else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F3) {
            application->renderSettings_.showcasePreset =
                static_cast<std::uint32_t>(key - GLFW_KEY_F1);
            constexpr std::array<const char*, 3> kPresetNames = {
                "Azure Gallery",
                "Endfield Industrial",
                "Neutral Material Check",
            };
            std::cout
                << "Showcase preset: "
                << key - GLFW_KEY_F1 + 1
                << " ("
                << kPresetNames[application->renderSettings_.showcasePreset]
                << ")\n";
        } else if (key == GLFW_KEY_F4) {
            if (application->asset_.animations.empty()) {
                std::cout << "Animation: unavailable for this asset\n";
            } else {
                application->animationPlaying_ =
                    !application->animationPlaying_;
                std::cout
                    << "Animation: "
                    << (application->animationPlaying_ ? "playing" : "paused")
                    << '\n';
            }
        } else if (key == GLFW_KEY_F11) {
            if (application->asset_.animations.empty()) {
                std::cout << "Animation: unavailable for this asset\n";
            } else {
                application->animationTime_ = 0.0F;
                application->animationPlaying_ = true;
                std::cout << "Animation: restarted\n";
            }
        } else if (key == GLFW_KEY_F10) {
            application->renderSettings_.innerOutlineEnabled =
                !application->renderSettings_.innerOutlineEnabled;
            std::cout
                << "Inner outlines: "
                << (application->renderSettings_.innerOutlineEnabled ? "on" : "off")
                << '\n';
        } else if (key == GLFW_KEY_F9) {
            application->renderSettings_.stylizedLightingEnabled =
                !application->renderSettings_.stylizedLightingEnabled;
            std::cout
                << "Stylized lighting: "
                << (application->renderSettings_.stylizedLightingEnabled ? "on" : "off")
                << '\n';
        }
    }
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_LEFT) {
            application->rotationAngle_ -= kFineStep;
            application->autoRotate_ = false;
        } else if (key == GLFW_KEY_RIGHT) {
            application->rotationAngle_ += kFineStep;
            application->autoRotate_ = false;
        } else if (key == GLFW_KEY_F7) {
            application->renderSettings_.styleMaskStrength = std::max(
                application->renderSettings_.styleMaskStrength - 0.10F,
                0.0F);
            std::cout
                << "Style mask strength: "
                << application->renderSettings_.styleMaskStrength
                << '\n';
        } else if (key == GLFW_KEY_F8) {
            application->renderSettings_.styleMaskStrength = std::min(
                application->renderSettings_.styleMaskStrength + 0.10F,
                2.0F);
            std::cout
                << "Style mask strength: "
                << application->renderSettings_.styleMaskStrength
                << '\n';
        } else if (key == GLFW_KEY_F5) {
            application->renderSettings_.diffuseBandThreshold = std::max(
                application->renderSettings_.diffuseBandThreshold - 0.05F,
                0.05F);
            std::cout
                << "Diffuse band threshold: "
                << application->renderSettings_.diffuseBandThreshold
                << '\n';
        } else if (key == GLFW_KEY_F6) {
            application->renderSettings_.diffuseBandThreshold = std::min(
                application->renderSettings_.diffuseBandThreshold + 0.05F,
                0.95F);
            std::cout
                << "Diffuse band threshold: "
                << application->renderSettings_.diffuseBandThreshold
                << '\n';
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_LEFT_BRACKET) {
            application->renderSettings_.outline.strength = std::max(
                application->renderSettings_.outline.strength - 0.05F,
                0.0F);
            std::cout << "Editor outline strength: "
                      << application->renderSettings_.outline.strength << '\n';
            application->runOptions_.editorContext->markDirty();
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_RIGHT_BRACKET) {
            application->renderSettings_.outline.strength = std::min(
                application->renderSettings_.outline.strength + 0.05F,
                2.0F);
            std::cout << "Editor outline strength: "
                      << application->renderSettings_.outline.strength << '\n';
            application->runOptions_.editorContext->markDirty();
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_MINUS) {
            application->renderSettings_.grade.exposureEv = std::max(
                application->renderSettings_.grade.exposureEv - 0.25F,
                -8.0F);
            std::cout << "Editor exposure EV: "
                      << application->renderSettings_.grade.exposureEv << '\n';
            application->runOptions_.editorContext->markDirty();
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_EQUAL) {
            application->renderSettings_.grade.exposureEv = std::min(
                application->renderSettings_.grade.exposureEv + 0.25F,
                8.0F);
            std::cout << "Editor exposure EV: "
                      << application->renderSettings_.grade.exposureEv << '\n';
            application->runOptions_.editorContext->markDirty();
        }
    }
}
