#include "AzureRenderApp.hpp"
#include "runtime/GameplayKeys.hpp"
#if AZURE_WITH_EDITOR
#include "editor/EditorSession.hpp"
#include "editor/EditorAutomation.hpp"
#endif
#if AZURE_WITH_EDITOR
#include "editor/ImGuiEditorLayer.hpp"
#endif
#include "diagnostics/GpuCapabilityReport.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "extensions/ExtensionRegistry.hpp"
#include "extensions/ISceneRenderer.hpp"
#include "platform/GlfwFrontend.hpp"
#include "render/RendererCore.hpp"
#include "render/RenderContext.hpp"
#include "scene/TransformMath.hpp"
#include "scenes/BuiltinRendererCatalog.hpp"
#include "AzureRenderInternal.hpp"


#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <set>
#include <stdexcept>
#include <utility>
#include <unordered_map>

using namespace azurerender::internal;

namespace {

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};


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
    if (options.sceneDocument.has_value() && !options.editorMode) runtime_.loadScene(*options.sceneDocument);
    if (!options.projectFile.empty() && !options.editorMode)
        levelSession_ = std::make_unique<azurerender::LevelSession>(azurerender::Project::load(options.projectFile), runtime_);
    if (levelSession_) gameRuntime_ = std::make_unique<azurerender::GameRuntime>(runtime_);
    if (gameRuntime_) {
        scriptRuntime_ = std::make_unique<azurerender::ScriptRuntime>(runtime_, *gameRuntime_, levelSession_->assets());
        scriptRuntime_->setLevelHandler([this](std::string reference) { levelSession_->request(std::move(reference)); });
        gameRuntime_->setBeforeStep([this](double delta) { scriptRuntime_->update(delta); });
        gameRuntime_->setEventHandler([this](const auto& event) { scriptRuntime_->dispatch(event); });
        gameRuntime_->setInteractionHandler([this](const auto& event){scriptRuntime_->dispatchInteraction(event);});
    }
    if(levelSession_){presentationRuntime_=std::make_unique<azurerender::PresentationRuntime>(runtime_,levelSession_->assets());scriptRuntime_->setAudioHandler([this](auto entity){presentationRuntime_->play(entity);});}
#if AZURE_WITH_EDITOR
    if(!options.editorActionsPath.empty())editorAutomation_=std::make_unique<azurerender::EditorAutomation>(options.editorActionsPath);
#endif
    runtime_.start();
    resourceLocator_ = azurerender::ResourceLocator(options.resourceRoot);
    azurerender::loadShowcasePresetCatalog(resourceLocator_.showcaseLooks());
    azurerender::SceneView sceneView;
    sceneView.assetPath = resourceLocator_.resolveAsset(options.assetPath).string();
    sceneView.renderSettings = options.renderSettings;
    azurerender::RendererCoreBoundary::validateSceneView(sceneView);
    azurerender::validateRenderSettings(runOptions_.renderSettings);
    renderSettings_ = runOptions_.renderSettings;
#if defined(AZURERENDER_HAS_IMGUI)
    editorUiEnabled_ = runOptions_.editorSession != nullptr;
#endif
    fixedSimulation_ = runOptions_.captureFrameLimit > 0;
    fixedSimulationStarted_ = false;
    fixedSimulationTime_ = 0.0;
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
#if !AZURE_WITH_EDITOR
    glfwSetCharCallback(frontend_->nativeHandle(),[](GLFWwindow* window,unsigned int character){auto* app=static_cast<AzureRenderApp*>(glfwGetWindowUserPointer(window));if(app && app->gameUi_)app->gameUi_->character(character);});
#endif
    renderSettings_ = runOptions_.renderSettings;
    if(levelSession_){
        rendererResourceKey_=levelSession_->current().resourceKey;
        levelSession_->setPrepareHandler([this](const auto& level){prepareLevelRenderer(level);});
        levelSession_->setPreloadHandler([this](const auto& level){return preloadLevelRenderer(level);});
        levelSession_->setReadinessHandler([this](const auto& level){return (sceneRenderer_ && sceneRenderer_->name()=="character" && level.scene.renderSettings.sceneType==azurerender::SceneType::Character && rendererResourceKey_==level.resourceKey) || (preparedRendererReady_ && preparedRenderSettings_.sceneType==level.scene.renderSettings.sceneType && preparedResourceKey_==level.resourceKey);});
    }
    if(!runOptions_.projectFile.empty())autoRotate_=false;
    if (runOptions_.portfolioMode) {
        activatePortfolioOrbit();
    }
#if AZURE_WITH_EDITOR
    if (runOptions_.editorSession != nullptr) {
        runOptions_.editorSession->context().attachRenderSettings(renderSettings_);
    }
#endif
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
    azurerender::RuntimeDiagnostics::instance().print(
        "render",
        std::string("Diagnostic view: ")
            + kDiagnosticNames[renderSettings_.diagnosticView]
            + ", stylized lighting: "
            + (renderSettings_.stylizedLightingEnabled ? "on" : "off")
            + ", internal outline: "
            + (renderSettings_.innerOutlineEnabled ? "on" : "off")
            + ", HUD: " + (hudEnabled_ ? "on" : "off"));
    mainLoop(runOptions_.smokeFrameLimit);
#if AZURE_WITH_EDITOR
    if (runOptions_.editorSession != nullptr) {
        runOptions_.editorSession->context().detachRenderSettings();
    }
#endif
}

void AzureRenderApp::initWindow() {
    azurerender::GlfwFrontendConfig config;
    config.width = runOptions_.width;
    config.height = runOptions_.height;
    config.title = runOptions_.editorMode
        ? "AzureRender Editor Preview"
        : "AzureRender - Stylized Vulkan Renderer";
    config.resizable = !fixedSimulation_;
    // Deterministic capture uses a hidden surface so framebuffer dimensions
    // are not clamped by the Windows work area (required for native 2K/4K).
    config.visible = !fixedSimulation_;
    config.decorated = !fixedSimulation_;
    config.userPointer = this;
    config.framebufferSizeCallback = framebufferResizeCallback;
    config.keyCallback = keyCallback;
    config.scrollCallback = scrollCallback;
    frontend_ = std::make_unique<azurerender::GlfwFrontend>(config);
    lastRotationTime_ = frontend_->timeSeconds();
    azurerender::RuntimeDiagnostics::instance().print(
        "input",
        "Controls: Space pause/resume, R auto rotate, "
        "1/2/3/4 full-body angles, 5 face close-up, "
        "6 portfolio orbit, "
        "Left/Right fine rotate, "
        "F1/F2/F3 showcase presets, "
        "F4 animation pause/resume, F11 animation restart, "
        "7/8 previous/next animation, 9 timeline status, "
        "0 diagnostic view, "
        "H HUD, "
        "F10 inner outlines, "
        "F9 style toggle, F7/F8 mask strength, "
        "F5/F6 band threshold, "
        "F12 screenshot");
}

void AzureRenderApp::initVulkan(const std::string& assetPath) {
    azurerender::RuntimeDiagnostics::instance().print(
        "gpu", "Validation layer: "
            + std::string(kEnableValidation ? "enabled" : "disabled"));
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    // The allocator must outlive every GPU resource, so it is created directly
    // after the device and torn down just before it.
    gpuAllocator_.initialize(
        instance_, physicalDevice_, device_, VK_API_VERSION_1_3);
    // 1 MiB per in-flight frame comfortably covers HUD vertices (288 KiB at
    // the vertex cap) and leaves headroom for upcoming uniform ring slices.
    uploadRing_.initialize(
        gpuAllocator_,
        1024 * 1024,
        kMaxFramesInFlight,
        uploadRingAlignment_);
    createCommandPool();
    rhi_ = std::make_unique<azurerender::rhi::VulkanRhi>(
        device_, physicalDevice_, graphicsQueue_, commandPool_,
        gpuAllocator_);
    createPostProcessDescriptorSetLayout();
    resolvedAssetPath_ = resourceLocator_.resolveAsset(assetPath).string();
    createShadowResources();
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
    createSceneRenderer();
    initEditorUi();
}

void AzureRenderApp::initEditorUi() {
#if AZURE_WITH_EDITOR

    if (!editorUiEnabled_) {
        return;
    }
    if (editorLayer_ == nullptr) {
        editorLayer_ = std::make_unique<azurerender::ImGuiEditorLayer>(
            runOptions_.editorSession);
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

#endif
}

void AzureRenderApp::mainLoop(const std::uint64_t smokeFrameLimit) {
    std::uint64_t renderedFrames = 0;
    while (!frontend_->shouldClose()) {
        frontend_->pollEvents();
        drawFrame();
        ++renderedFrames;
        if (smokeFrameLimit > 0 && renderedFrames >= smokeFrameLimit) {
#if AZURE_WITH_EDITOR
            if(editorAutomation_ && !editorAutomation_->complete())continue;
#endif
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
    if (!runOptions_.runtimeReportPath.empty()) {
        nlohmann::json nodes = nlohmann::json::array();
        auto scene = runtime_.snapshotScene();
#if AZURE_WITH_EDITOR
        if(runOptions_.editorSession)scene=runOptions_.editorSession->viewScene();
#endif
        for (const auto& node : scene.nodes) nodes.push_back({{"id", node.id}, {"translation", node.translation}, {"visible", node.visible}});
        std::ofstream report(runOptions_.runtimeReportPath);
        report << nlohmann::json{{"scene", scene.sceneId}, {"nodes", nodes}, {"levelRevision", levelSession_ ? levelSession_->revision() : 0}, {"fixedSteps", gameRuntime_ ? gameRuntime_->steps() : 0}, {"simulationMilliseconds", gameRuntime_ ? gameRuntime_->simulationMilliseconds() : 0.0}, {"activeScripts", scriptRuntime_ ? scriptRuntime_->activeCount() : 0}, {"scriptErrors", scriptRuntime_ ? scriptRuntime_->errors() : std::vector<std::string>{}}}.dump(2);
        report.close();
        std::ifstream saved(runOptions_.runtimeReportPath);nlohmann::json data;saved>>data;saved.close();
        data["uiDrawCalls"]=uiDrawCalls_;data["animationFrames"]=animationFrames_;data["audioStarts"]=audioStarts_;data["presentationErrors"]=presentationErrors_;
        data["routeFrames"]=gameRouteFrames_;
        data["replayedActions"]=gameInputReplay_?gameInputReplay_->consumed():0;
        data["animations"]=nlohmann::json::array();
        if(auto* presentation=activePresentation())for(const auto& animation:presentation->animations())
            data["animations"].push_back({{"node",animation.node},{"clip",animation.clip},{"time",animation.time},
                {"previousClip",animation.previousClip},{"blend",animation.blend},{"morph",animation.morph}});
#if AZURE_WITH_EDITOR
        data["editorPlaying"]=runOptions_.editorSession && runOptions_.editorSession->playing();if(editorAutomation_)data.update(editorAutomation_->report());
        data["editorPreviewFrames"]=editorPreviewFrames_;
        data["editorDebugLines"]=editorLayer_?editorLayer_->debugLineCount():0;
        if(runOptions_.editorSession) { data["gameBuildPassed"]=runOptions_.editorSession->buildResult().passed; data["gameBuildMilliseconds"]=runOptions_.editorSession->buildResult().milliseconds; }
#endif
        std::ofstream extended(runOptions_.runtimeReportPath);extended<<data.dump(2);
        if (!extended) throw std::runtime_error("Cannot write runtime report");
    }
    scriptRuntime_.reset();
    gameRuntime_.reset();
    presentationRuntime_.reset();
    runtime_.stop();
    azurerender::RuntimeDiagnostics::instance().print(
        "render", "Rendered frames: " + std::to_string(renderedFrames));
    if (fixedSimulation_) {
        writeCaptureManifest(renderedFrames);
        azurerender::RuntimeDiagnostics::instance().print(
            "capture",
            "Captured frames: " + std::to_string(capturedFrames_) + " at "
                + std::to_string(runOptions_.captureFps) + " fps");
    }
}

void AzureRenderApp::createSceneRenderer() {
    azurerender::RenderContext context;
    buildRenderContext(context);
    std::string rendererId =
        azurerender::sceneTypeName(renderSettings_.sceneType);
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession && context.scene.resources.empty())rendererId="sample";
#endif
    azurerender::SceneRendererRegistry registry =
        azurerender::BuiltinRendererCatalog::createRegistry();
    sceneRenderer_ = registry.create(rendererId);
    azurerender::validateSceneRendererCapabilities(
        sceneRenderer_->capabilities());
    sceneRenderer_->onLoad(context);
    azurerender::RuntimeDiagnostics::instance().print(
        "render",
        "Scene renderer: " + std::string(sceneRenderer_->name())
            + " (sceneType=" + azurerender::sceneTypeName(
                                   renderSettings_.sceneType) + ")");
}

azurerender::scene::SceneDescription AzureRenderApp::resolveRenderDescription(
    const azurerender::SceneDocument& document) const {
    auto description = document.renderDescription();
    for (auto& resource : description.resources) {
        resource.path = resourceLocator_.resolveAsset(resource.path).string();
    }
    return description;
}

void AzureRenderApp::buildRenderContext(
    azurerender::RenderContext& context) {
    context.device = device_;
    context.physicalDevice = physicalDevice_;
    context.graphicsQueue = graphicsQueue_;
    context.graphicsQueueFamily = graphicsQueueFamily_;
    context.commandPool = commandPool_;
    context.allocator = &gpuAllocator_;
    context.rhi = rhi_.get();
    context.bindlessTextures =
        bindlessTexturesSupported_ && !runOptions_.bindlessDisabled;
    context.rgba16fStorageImage = rgba16fStorageImageSupported_;
    context.computeSkinning =
        computeShaderSupported_ && !runOptions_.computeSkinningDisabled;
    context.cullingEnabled = !runOptions_.cullingDisabled;
    context.faceCullingEnabled = !runOptions_.qaDisableFaceCulling;
    context.depthTestEnabled = !runOptions_.qaDisableDepthTest;
    context.gpuCulling = computeShaderSupported_ && indirectFirstInstanceSupported_
        && !runOptions_.gpuCullingDisabled;
    context.multiDrawIndirect = multiDrawIndirectSupported_ && !runOptions_.multiDrawIndirectDisabled;
    VkPhysicalDeviceProperties indirectProperties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &indirectProperties);
    context.maxDrawIndirectCount = indirectProperties.limits.maxDrawIndirectCount;
    context.qaInstanceCount = std::max(runOptions_.instanceCount, 1U);
    context.maxFramesInFlight = kMaxFramesInFlight;
    context.renderExtent = renderExtent_;
    context.swapchainExtent = swapchainExtent_;
    context.sceneColorFormat = kHdrSceneColorFormat;
    context.depthFormat = depthFormat_;
    context.normalFormat = normalFormat_;
    context.sceneRenderPass = renderPass_;
    context.postProcessRenderPass = postProcessRenderPass_;
    context.postProcessPipelineLayout = postProcessPipelineLayout_;
    context.postProcessDescriptorSetLayout = postProcessDescriptorSetLayout_;
    context.screenAttachmentSampler = screenAttachmentSampler_;
    context.shadowFormat = shadowFormat_;
    context.shadowRenderPass = shadowRenderPass_;
    context.shadowFramebuffer = shadowFramebuffer_;
    context.shadowImageView = shadowImageView_;
    context.shadowSampler = shadowSampler_;
    context.shadowMapSize = kShadowMapSize;
    {
        const azurerender::SceneDocument* document = nullptr;
#if AZURE_WITH_EDITOR
        azurerender::SceneDocument editorDocument;
#endif
#if AZURE_WITH_EDITOR
        if (runOptions_.editorSession != nullptr) {
            editorDocument=runOptions_.editorSession->viewScene();document=&editorDocument;
        } else
#endif
        if (runOptions_.sceneDocument.has_value()) {
            document = &*runOptions_.sceneDocument;
        }
        if (document != nullptr) {
            context.scene = resolveRenderDescription(*document);
            context.sceneWorldCoordinates = true;
        } else {
            context.scene.resources.push_back({"asset-0", resolvedAssetPath_});
            azurerender::scene::SceneNodeDesc root{};
            root.resourceId = "asset-0";
            context.scene.nodes.push_back(std::move(root));
        }
    }
    context.shaderDirectory = resourceLocator_.shaderDirectory().string();
    context.environment.path = runOptions_.environmentPath;
    if (context.environment.path.empty() && runOptions_.projectFile.empty()) {
        const std::filesystem::path privateRoot("D:/Assigment/temp");
        if (renderSettings_.sceneType == azurerender::SceneType::Blackhole) {
            const auto candidate = privateRoot / "Space_Skybox";
            if (std::filesystem::is_directory(candidate)) {
                context.environment.path = candidate.string();
                context.environment.projection =
                    azurerender::EnvironmentProjection::CubeFaces;
            }
        } else if (renderSettings_.sceneType == azurerender::SceneType::Character) {
            const auto candidate = privateRoot / "EveningSkyHDRI007B_2K"
                / "EveningSkyHDRI007B_2K_TONEMAPPED.jpg";
            if (std::filesystem::is_regular_file(candidate)) {
                context.environment.path = candidate.string();
                context.environment.projection =
                    azurerender::EnvironmentProjection::Equirectangular;
            }
        }
    }
    context.rampAtlasPath = resourceLocator_.rampAtlas().string();
    context.renderSettings = &renderSettings_;
}

void AzureRenderApp::buildSceneFrameData(
    azurerender::SceneFrameData& frame) {
    const double currentTime = frontend_->timeSeconds();
    const float rawDeltaSeconds = (fixedSimulation_ || runOptions_.fixedFrameStep)
        ? (fixedSimulationStarted_ ? fixedDeltaSeconds_ : 0.0F)
        : static_cast<float>(
              std::max(currentTime - lastRotationTime_, 0.0));
    auto* game=activeGame();
    if(game&&!runOptions_.gameActionsPath.empty()) {
        if(!gameInputReplay_){std::ifstream input(runOptions_.gameActionsPath);nlohmann::json document;input>>document;
            gameInputReplay_=azurerender::GameInputReplay::parse(document);}
        auto* levels=levelSession_.get();
#if AZURE_WITH_EDITOR
        if(runOptions_.editorSession)levels=runOptions_.editorSession->levels();
#endif
        gameInputReplay_->apply(gameplayFrame_,*game,levels);
    }
    if (game&&!gameInputReplay_) {
        bool focused = glfwGetWindowAttrib(frontend_->nativeHandle(), GLFW_FOCUSED) != 0;
#if AZURE_WITH_EDITOR
        if (editorLayer_) focused = focused && editorLayer_->acceptsViewportShortcuts();
#endif
        double x=0,y=0;glfwGetCursorPos(frontend_->nativeHandle(),&x,&y);
        bool uiPointer=false;
#if AZURE_WITH_EDITOR
        if(!editorLayer_)
#endif
        if(gameUi_&&glfwGetInputMode(frontend_->nativeHandle(),GLFW_CURSOR)!=GLFW_CURSOR_DISABLED){
            int width=1,height=1;glfwGetWindowSize(frontend_->nativeHandle(),&width,&height);
            uiPointer=gameUi_->wantsPointer(static_cast<int>(x*renderExtent_.width/std::max(width,1)),static_cast<int>(y*renderExtent_.height/std::max(height,1)));
        }
        if(focused&&glfwGetMouseButton(frontend_->nativeHandle(),GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS&&!uiPointer) gameViewportFocus_=true;
        focused=focused&&gameViewportFocus_&&!(gameUi_&&gameUi_->wantsKeyboard())&&!uiPointer;
        game->input().setFocused(focused);
        if(focused&&game->hasCamera()&&activeRuntime()->state()==azurerender::RuntimeLifecycle::State::Running){
            if(gameCursorPrimed_)game->cameraInput(static_cast<float>(x-gameCursorX_),static_cast<float>(y-gameCursorY_),0);
            gameCursorPrimed_=true;
            glfwSetInputMode(frontend_->nativeHandle(),GLFW_CURSOR,GLFW_CURSOR_DISABLED);
        }else{gameCursorPrimed_=false;glfwSetInputMode(frontend_->nativeHandle(),GLFW_CURSOR,GLFW_CURSOR_NORMAL);}
        gameCursorX_=x;gameCursorY_=y;
        for (int key : azurerender::kGameplayKeys)
            game->input().key(key, glfwGetKey(frontend_->nativeHandle(), key) == GLFW_PRESS);
    }
    double elapsed=0;
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)elapsed=runOptions_.editorSession->advance(rawDeltaSeconds);else
#endif
    {elapsed=game?game->advance(rawDeltaSeconds):runtime_.beginFrame(rawDeltaSeconds);if(presentationRuntime_)presentationRuntime_->update(elapsed);}
    const float deltaSeconds=static_cast<float>(elapsed);
    if(game&&game->hasCamera()&&!qaHarnessEnabled_){
        cameraPosition_=game->renderCameraPosition();cameraTarget_=game->renderCameraTarget();autoRotate_=false;rotationAngle_=0;
    }
    if(auto* presentation=activePresentation())frame.animations=presentation->animations();
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession && !runOptions_.editorSession->playing()){
        const auto& preview=runOptions_.editorSession->context().animationPreview();
        if(preview){frame.animations.push_back(*preview);++editorPreviewFrames_;}
    }
#endif
    if(gameInputReplay_&&gameRouteFrames_.size()<8192){nlohmann::json state;
        state["frame"]=gameplayFrame_;state["camera"]=cameraPosition_;state["target"]=cameraTarget_;state["focused"]=game->input().focused();
        state["characters"]=nlohmann::json::array();
        activeRuntime()->world().each<azurerender::game::Character>([&](auto entity,const auto&){
            const auto* transform=activeRuntime()->world().tryGet<azurerender::ecs::TransformComponent>(entity);
            const auto* animator=activeRuntime()->world().tryGet<azurerender::game::Animator>(entity);
            if(transform)state["characters"].push_back({{"node",activeRuntime()->nodeId(entity)},{"position",transform->translation},
                {"rotation",transform->rotation},{"grounded",game->physics().grounded(entity)},
                {"state",animator?animator->state:""},{"playbackRate",animator?animator->playbackRate:0}});
        });
        auto* runtime=activeRuntime();state["tasks"]=nlohmann::json::array();
        runtime->world().each<azurerender::game::TaskState>([&](auto entity,const auto& task){
            state["tasks"].push_back({{"node",runtime->nodeId(entity)},{"started",task.started},{"collected",task.collected},
                {"doorOpened",task.doorOpened},{"completed",task.completed}});});
        unsigned collectibles=0;bool doorOpen=false,checkpointActivated=false;
        runtime->world().each<azurerender::game::Collectible>([&](auto,const auto& value){if(!value.collected)++collectibles;});
        runtime->world().each<azurerender::game::Door>([&](auto,const auto& value){doorOpen|=value.open;});
        runtime->world().each<azurerender::game::Checkpoint>([&](auto,const auto& value){checkpointActivated|=value.activated;});
        state["collectibles"]=collectibles;state["doorOpen"]=doorOpen;state["checkpointActivated"]=checkpointActivated;
        state["levelRevision"]=runtime->sceneRevision();
        state["interactionTarget"]=game->interactionTarget()?game->interactionTarget()->node:std::string();
        state["cameraObstructed"]=game->hasCamera()&&game->camera().actualDistance()<game->camera().distance()-.1F;
        gameRouteFrames_.push_back(std::move(state));}
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)frame.sceneSnapshot=std::make_shared<const azurerender::scene::SceneDescription>(resolveRenderDescription(game?game->renderScene():runOptions_.editorSession->viewScene()));
#endif
    pausedTimeOffset_ += static_cast<double>(rawDeltaSeconds - deltaSeconds);
    if (runOptions_.sceneDocument.has_value() && !runOptions_.editorMode)
        runOptions_.sceneDocument = game?game->renderScene():runtime_.snapshotScene();
    if (runOptions_.sceneDocument.has_value() && !runOptions_.editorMode)
        frame.sceneSnapshot = std::make_shared<const azurerender::scene::SceneDescription>(
            resolveRenderDescription(*runOptions_.sceneDocument));
    fixedSimulationStarted_ = true;
    lastRotationTime_ = currentTime;
    if (autoRotate_) {
        rotationAngle_ += deltaSeconds * rotationSpeed_;
    }
    frame.deltaSeconds = deltaSeconds;
    if (fixedSimulation_ || runOptions_.fixedFrameStep) {
        fixedSimulationTime_ += deltaSeconds;
        frame.timeSeconds = fixedSimulationTime_;
    } else {
        frame.timeSeconds = currentTime - pausedTimeOffset_;
    }
    frame.renderSettings = &renderSettings_;
    frame.cameraPosition[0] = cameraPosition_[0];
    frame.cameraPosition[1] = cameraPosition_[1];
    frame.cameraPosition[2] = cameraPosition_[2];
    frame.cameraTarget[0] = cameraTarget_[0];
    frame.cameraTarget[1] = cameraTarget_[1];
    frame.cameraTarget[2] = cameraTarget_[2];
    frame.rotationAngle = rotationAngle_;
    frame.qaIsolationMode = qaIsolationMode_;
    frame.qaEffectMode = qaEffectMode_;
    frame.qaEffectEnabled = qaEffectEnabled_;
    frame.qaHarnessEnabled = qaHarnessEnabled_;
    frame.qaLightScan = runOptions_.qaLightScan;
    frame.capturedFrames = capturedFrames_;
    frame.captureFps = runOptions_.captureFps;
    frame.captureActive = fixedSimulation_;
    frame.technicalSequence = runOptions_.technicalSequence;
    frame.technicalSequenceChapter = technicalSequenceChapter_;
    frame.currentFrame = static_cast<std::uint32_t>(currentFrame_);
    frame.selectedPrimitiveIndex = selectedPrimitiveIndex_;
    frame.gizmoActive =
        selectedPrimitiveIndex_ >= 0
        && runOptions_.editorSession != nullptr;
#if AZURE_WITH_EDITOR
    if (runOptions_.editorSession != nullptr) {
        if(runOptions_.editorSession->playing())frame.gizmoActive=false;
        const azurerender::EditorContext& editorContext =
            runOptions_.editorSession->context();
        const std::array<float, 3> translation =
            editorContext.gizmoTranslation();
        const std::array<float, 3> rotation =
            editorContext.gizmoRotation();
        const std::array<float, 3> scale = editorContext.gizmoScale();
        frame.gizmoTranslation[0] = translation[0];
        frame.gizmoTranslation[1] = translation[1];
        frame.gizmoTranslation[2] = translation[2];
        frame.gizmoRotation[0] = rotation[0];
        frame.gizmoRotation[1] = rotation[1];
        frame.gizmoRotation[2] = rotation[2];
        frame.gizmoScale[0] = scale[0];
        frame.gizmoScale[1] = scale[1];
        frame.gizmoScale[2] = scale[2];
    }
#endif
    frame.swapchainWidth = swapchainExtent_.width;
    frame.swapchainHeight = swapchainExtent_.height;
}

void AzureRenderApp::activatePortfolioOrbit() {
    constexpr float kTwoPi = 6.28318530717958647692F;
    constexpr float kCharacterTurntablePeriodSeconds = 16.0F;
    rotationAngle_ = renderSettings_.sceneType == azurerender::SceneType::Character
        ? kTwoPi * 0.75F
        : 0.0F;
    // Every character showcase segment completes one deterministic turn in
    // sixteen seconds. Keep the approved black-hole motion unchanged.
    rotationSpeed_ = renderSettings_.sceneType == azurerender::SceneType::Character
        ? kTwoPi / kCharacterTurntablePeriodSeconds
        : 0.20F;
    cameraPosition_ = {0.0F, 1.18F, 3.55F};
    cameraTarget_ = {0.0F, 0.12F, 0.0F};
    azurerender::applyShowcasePresetLook(renderSettings_, 1);
    autoRotate_ = true;
    if (sceneRenderer_ != nullptr) {
        sceneRenderer_->restartPlayback();
    }
    azurerender::RuntimeDiagnostics::instance().print(
        "input", "View preset: 6 (portfolio orbit, Endfield Industrial)");
}

void AzureRenderApp::configureQaHarness() {
    qaHarnessEnabled_ = !runOptions_.qaCamera.empty()
        || runOptions_.qaLightScan
        || runOptions_.qaAnimation
        || !runOptions_.qaLight.empty()
        || !runOptions_.qaEffect.empty()
        || !runOptions_.qaEffectState.empty()
        || !runOptions_.qaIsolation.empty();
    // FYP render-path selection: map the CLI name to the settings enum.
    // Runs before the QA early-return so --render-path works standalone.
    if (!runOptions_.renderPathName.empty()) {
        if (runOptions_.renderPathName == "subpasses") {
            renderSettings_.renderPath =
                azurerender::RenderSettings::RenderPath::Subpasses;
        } else if (runOptions_.renderPathName == "dynamic") {
            renderSettings_.renderPath =
                azurerender::RenderSettings::RenderPath::DynamicRendering;
        } else {
            renderSettings_.renderPath =
                azurerender::RenderSettings::RenderPath::Traditional;
        }
        azurerender::RuntimeDiagnostics::instance().print(
            "render",
            "Render path: " + runOptions_.renderPathName);
    }
    if (!qaHarnessEnabled_) {
        return;
    }

    constexpr float kPi = 3.14159265358979323846F;
    qaCameraName_ = runOptions_.qaCamera.empty()
        ? "full-body-front"
        : runOptions_.qaCamera;
    if (qaCameraName_ == "full-body-front") {
        if (!runOptions_.portfolioMode) {
            rotationAngle_ = 0.0F;
        }
        cameraPosition_ = {0.0F, 1.18F, 3.55F};
        cameraTarget_ = {0.0F, 0.12F, 0.0F};
        if (runOptions_.sceneDocument && sceneRenderer_ && sceneRenderer_->sceneState()
            && sceneRenderer_->sceneState()->asset) {
            const auto& asset = *sceneRenderer_->sceneState()->asset;
            const auto scene = resolveRenderDescription(*runOptions_.sceneDocument);
            const auto transforms = azurerender::scene::resolveNodeWorldTransforms(scene);
            for (std::size_t index = 0; index < scene.nodes.size(); ++index) {
                if (scene.resources.empty() || scene.nodes[index].resourceId != scene.resources.front().id
                    || !scene.nodes[index].visible) continue;
                const auto& matrix = transforms[index];
                const std::array<float, 3> center{
                    (asset.boundsMin[0] + asset.boundsMax[0]) * 0.5F,
                    (asset.boundsMin[1] + asset.boundsMax[1]) * 0.5F,
                    (asset.boundsMin[2] + asset.boundsMax[2]) * 0.5F};
                for (unsigned axis = 0; axis < 3; ++axis)
                    cameraTarget_[axis] = static_cast<float>(matrix[axis] * center[0]
                        + matrix[4 + axis] * center[1] + matrix[8 + axis] * center[2] + matrix[12 + axis]);
                cameraPosition_ = {cameraTarget_[0], cameraTarget_[1] + 1.06F, cameraTarget_[2] + 3.55F};
                break;
            }
        }
        // An isolation pass implicitly selects this QA camera. Preserve the
        // portfolio turntable in that combined mode so diagnostic segments
        // follow the same orbit as Beauty instead of silently becoming still.
        autoRotate_ = runOptions_.portfolioMode;
    } else if (qaCameraName_ == "face-front") {
        rotationAngle_ = kPi * 0.25F;
        cameraPosition_ = {0.915F, 1.507F, 1.046F};
        cameraTarget_ = {0.0F, 0.82F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "face-three-quarter" || qaCameraName_ == "face-three-quarter-left") {
        rotationAngle_ = kPi * 0.40F;
        cameraPosition_ = {0.915F, 1.507F, 1.046F};
        cameraTarget_ = {0.0F, 0.82F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "face-three-quarter-right"
               || qaCameraName_ == "face-side-left" || qaCameraName_ == "face-side-right") {
        rotationAngle_ = kPi * (qaCameraName_ == "face-three-quarter-right" ? 0.10F
            : qaCameraName_ == "face-side-left" ? 0.75F : -0.25F);
        cameraPosition_ = {0.915F, 1.507F, 1.046F};
        cameraTarget_ = {0.0F, 0.82F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "back-detail") {
        rotationAngle_ = kPi * 1.25F;
        cameraPosition_ = {1.45F, 1.58F, 1.72F};
        cameraTarget_ = {0.0F, 0.38F, 0.0F};
        autoRotate_ = false;
    } else if (qaCameraName_ == "lighting-sweep") {
        rotationAngle_ = 0.0F;
        cameraPosition_ = {0.0F, 1.18F, 3.55F};
        cameraTarget_ = {0.0F, 0.12F, 0.0F};
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
        azurerender::applyShowcasePresetLook(renderSettings_, 2);
    } else if (qaLightName_ == "stylized-key") {
        azurerender::applyShowcasePresetLook(renderSettings_, 1);
    } else if (qaLightName_ == "specular-rim") {
        azurerender::applyShowcasePresetLook(renderSettings_, 3);
    } else if (qaLightName_ == "rear-emissive") {
        azurerender::applyShowcasePresetLook(renderSettings_, 4);
    } else {
        throw std::invalid_argument(
            "Unknown --qa-light: " + qaLightName_);
    }
    // Explicit CLI feature disables remain authoritative when a QA camera or
    // light selects a complete look preset.
    if (!runOptions_.renderSettings.stylizedLightingEnabled) {
        renderSettings_.stylizedLightingEnabled = false;
    }
    if (!runOptions_.renderSettings.innerOutlineEnabled) {
        renderSettings_.innerOutlineEnabled = false;
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

    constexpr std::array<const char*, 21> kIsolationNames = {
        "beauty", "albedo", "world-normal", "depth", "diffuse-band",
        "shadow-visibility", "hair-kk", "rim", "specular", "emissive",
        "outline", "shadow-map", "material-id", "style-mask", "ambient",
        "direct-diffuse", "shadow-tint", "face-sdf", "overlay", "bloom",
        "brow-mask",
    };
    const auto isolation = std::find(
        kIsolationNames.begin(), kIsolationNames.end(), qaIsolationName_);
    if (isolation == kIsolationNames.end()) {
        throw std::invalid_argument(
            "Unknown --qa-isolation: " + qaIsolationName_);
    }
    const std::uint32_t isolationIndex = static_cast<std::uint32_t>(
        std::distance(kIsolationNames.begin(), isolation));
    constexpr std::array<std::uint32_t, 21> kPostProcessViews = {
        0, 0, 1, 4, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    constexpr std::array<std::uint32_t, 21> kShaderIsolationModes = {
        0, 1, 0, 0, 2, 3, 4, 5, 6, 7, 0, 0, 8, 9, 10, 11, 12, 13, 14, 15, 16,
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
    if (sceneRenderer_ != nullptr && qaCameraName_ != "lighting-sweep" && !runOptions_.qaAnimation) {
        sceneRenderer_->setPlaybackPlaying(false);
    }
    azurerender::RuntimeDiagnostics::instance().print(
        "qa",
        std::string("CQ-0 QA: camera=") + qaCameraName_
            + ", light=" + qaLightName_
            + ", effect=" + qaEffectName_
            + ", state=" + qaEffectStateName_
            + ", isolation=" + qaIsolationName_);
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
        azurerender::RuntimeDiagnostics::instance().print(
            "capture",
            "Technical chapter " + std::to_string(chapter + 1) + "/5: "
                + kChapterNames[chapter] + " at frame "
                + std::to_string(frameIndex) + " ("
                + std::to_string(chapterTime) + " s)");
    }
}

void AzureRenderApp::cleanup() {
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession && runOptions_.editorSession->playing())static_cast<void>(runOptions_.editorSession->execute(azurerender::EditorCommand::Stop));
#endif
    scriptRuntime_.reset();
    gameRuntime_.reset();
    presentationRuntime_.reset();
    runtime_.stop();
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        gameUi_.reset();gameUiRenderer_.reset();
        workerCommandPools_.reset();
#if AZURE_WITH_EDITOR
        if (editorLayer_ != nullptr) {
            editorLayer_->shutdownVulkan();
        }
#endif
        if(preparedRenderer_){azurerender::RenderContext context;buildRenderContext(context);preparedRenderer_->onUnload(context);preparedRenderer_.reset();}
        cleanupSwapchain();
        if (sceneRenderer_ != nullptr) {
            azurerender::RenderContext unloadContext;
            buildRenderContext(unloadContext);
            sceneRenderer_->onUnload(unloadContext);
            sceneRenderer_.reset();
        }

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

        if (screenAttachmentSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, screenAttachmentSampler_, nullptr);
        }
        if (postProcessDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(
                device_,
                postProcessDescriptorSetLayout_,
                nullptr);
        }
        capturePool_.reset();
        uploadRing_.shutdown();
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
        gpuAllocator_.destroyImage(shadowImage_);
        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }
        rhi_.reset();
        const auto& unloaded = gpuAllocator_.statistics();
        azurerender::RuntimeDiagnostics::instance().print(
            "render", "Allocator after unload: buffers=" + std::to_string(unloaded.liveBuffers)
                + " images=" + std::to_string(unloaded.liveImages));
        gpuAllocator_.shutdown();
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
#if AZURE_WITH_EDITOR
    editorLayer_.reset();
#endif
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
    uploadRingAlignment_ = std::max<VkDeviceSize>(
        properties.limits.minUniformBufferOffsetAlignment,
        properties.limits.nonCoherentAtomSize);
    VkPhysicalDeviceVulkan12Features vulkan12Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 features2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext = &vulkan12Features;
    vkGetPhysicalDeviceFeatures2(physicalDevice_, &features2);
    indirectFirstInstanceSupported_ = features2.features.drawIndirectFirstInstance == VK_TRUE;
    multiDrawIndirectSupported_ = features2.features.multiDrawIndirect == VK_TRUE;
    computeShaderSupported_ = false;
    bindlessTexturesSupported_ =
        vulkan12Features.runtimeDescriptorArray == VK_TRUE
        && vulkan12Features.shaderSampledImageArrayNonUniformIndexing
            == VK_TRUE;
    const std::filesystem::path capabilityDirectory =
        runOptions_.captureDirectory.empty()
        ? std::filesystem::path("captures")
        : std::filesystem::path(runOptions_.captureDirectory);
    static_cast<void>(azurerender::writeGpuCapabilityReport(
        physicalDevice_, capabilityDirectory / "gpu_capabilities.json"));
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
    computeShaderSupported_ =
        (queueProperties.at(*queueIndices.graphics).queueFlags
         & VK_QUEUE_COMPUTE_BIT) != 0;
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
    rgba16fStorageImageSupported_ =
        (hdrSceneColorProperties.optimalTilingFeatures
         & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
        == VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    if (runOptions_.gpuTimingEnabled && timestampValidBits_ == 0) {
        throw std::runtime_error(
            "Selected graphics queue does not support timestamp queries");
    }
    azurerender::RuntimeDiagnostics::instance().print(
        "gpu", "Selected GPU: " + selectedGpuName_);
    azurerender::RuntimeDiagnostics::instance().print(
        "gpu",
        "HDR scene color candidate: VK_FORMAT_R16G16B16A16_SFLOAT ("
            + std::string(hdrSceneColorFormatSupported_ ? "supported" : "unsupported")
            + ")");
    if (!hdrSceneColorFormatSupported_) {
        throw std::runtime_error(
            "Selected GPU does not support the required RGBA16F sampled, "
            "color-attachment, and color-blend features");
    }
    if (runOptions_.gpuTimingEnabled) {
        azurerender::RuntimeDiagnostics::instance().print(
            "gpu",
            "GPU timing: enabled, timestamp period "
                + std::to_string(timestampPeriodNanoseconds_) + " ns, "
                + std::to_string(timestampValidBits_) + " valid bits");
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
    // Required so scene renderers (e.g. the blackhole tracer) can use a
    // zero-write World Normal attachment next to a written Scene Color.
    deviceFeatures.independentBlend = VK_TRUE;
    deviceFeatures.drawIndirectFirstInstance = indirectFirstInstanceSupported_ ? VK_TRUE : VK_FALSE;
    deviceFeatures.multiDrawIndirect = multiDrawIndirectSupported_ ? VK_TRUE : VK_FALSE;
    VkPhysicalDeviceVulkan12Features vulkan12Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    if (bindlessTexturesSupported_ && !runOptions_.bindlessDisabled) {
        vulkan12Features.runtimeDescriptorArray = VK_TRUE;
        vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    }
    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.pNext = &vulkan12Features;
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
        azurerender::RuntimeDiagnostics::instance().print(
            "editor",
            "Editor viewport render extent: "
                + std::to_string(renderExtent_.width) + 'x'
                + std::to_string(renderExtent_.height));
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
    workerCommandPools_ = std::make_unique<azurerender::rhi::WorkerCommandPools>(
        device_, *indices.graphics, kMaxFramesInFlight, recordingWorkers_.recordingThreadCount());
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

void AzureRenderApp::scrollCallback(GLFWwindow* window, double x, double y) {
    (void)x;auto* app=static_cast<AzureRenderApp*>(glfwGetWindowUserPointer(window));
    if(app)if(auto* game=app->activeGame())game->cameraInput(0,0,static_cast<float>(y));
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
#if AZURE_WITH_EDITOR
    if (application->editorLayer_ != nullptr) {
        if (!application->editorLayer_->acceptsViewportShortcuts()) {
            return;
        }
    }
#endif

    if(application->gameUi_){application->gameUi_->key(key,action!=GLFW_RELEASE);if(application->gameUi_->wantsKeyboard())return;}
    auto* game=application->activeGame();auto* runtime=application->activeRuntime();
    if(game&&key==GLFW_KEY_ESCAPE&&action==GLFW_PRESS){application->gameViewportFocus_=false;application->gameCursorPrimed_=false;game->input().setFocused(false);glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_NORMAL);return;}
    if(game && azurerender::isGameplayKey(key)){game->input().key(key,action!=GLFW_RELEASE);return;}
    if(runtime && game){
        if(action==GLFW_PRESS&&key==GLFW_KEY_P){if(runtime->state()==azurerender::RuntimeLifecycle::State::Running)runtime->pause();else if(runtime->state()==azurerender::RuntimeLifecycle::State::Paused)runtime->resume();return;}
        if(action==GLFW_PRESS&&key==GLFW_KEY_O){if(runtime->state()==azurerender::RuntimeLifecycle::State::Paused)runtime->step();return;}
#if AZURE_WITH_EDITOR
        if(application->runOptions_.editorSession && application->runOptions_.editorSession->playing())return;
#endif
    }
    constexpr float kPi = 3.14159265358979323846F;
    constexpr float kFineStep = kPi / 36.0F;
    if (action == GLFW_PRESS) {
#if AZURE_WITH_EDITOR
        if (application->runOptions_.editorSession != nullptr
            && key == GLFW_KEY_TAB
            && application->runOptions_.editorSession->context().selectedNode() != nullptr) {
            application->runOptions_.editorSession->context().selectNextNode();
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Editor selected node: "
                    + application->runOptions_.editorSession->context()
                          .selectedNode()->name);
        } else
#endif
        if (key == GLFW_KEY_SPACE) {
            application->autoRotate_ = !application->autoRotate_;
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Auto rotate: "
                    + std::string(application->autoRotate_ ? "on" : "paused"));
        } else if (key == GLFW_KEY_R) {
            application->autoRotate_ = true;
            application->rotationSpeed_ = 0.65F;
            azurerender::RuntimeDiagnostics::instance().print(
                "input", "Auto rotate: on (standard speed)");
        } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
            application->rotationAngle_ =
                kPi * 0.25F
                + static_cast<float>(key - GLFW_KEY_1) * kPi * 0.5F;
            application->cameraPosition_ = {2.8F, 2.1F, 3.2F};
            application->cameraTarget_ = {0.0F, 0.0F, 0.0F};
            application->autoRotate_ = false;
            application->rotationSpeed_ = 0.65F;
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Angle preset: " + std::to_string(key - GLFW_KEY_0));
        } else if (key == GLFW_KEY_5) {
            application->rotationAngle_ = kPi * 0.25F;
            application->cameraPosition_ = {0.915F, 1.507F, 1.046F};
            application->cameraTarget_ = {0.0F, 0.82F, 0.0F};
            application->autoRotate_ = false;
            application->rotationSpeed_ = 0.65F;
            azurerender::RuntimeDiagnostics::instance().print(
                "input", "View preset: 5 (face close-up)");
        } else if (key == GLFW_KEY_6) {
            application->activatePortfolioOrbit();
        } else if (key == GLFW_KEY_7 || key == GLFW_KEY_8
                   || key == GLFW_KEY_9) {
            if (application->sceneRenderer_ != nullptr) {
                application->sceneRenderer_->onAnimationKey(key, action);
            }
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
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                std::string("Diagnostic view: ")
                    + kDiagnosticNames[application->renderSettings_.diagnosticView]);
        } else if (key == GLFW_KEY_H) {
            application->hudEnabled_ = !application->hudEnabled_;
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "HUD: " + std::string(application->hudEnabled_ ? "on" : "off"));
            if (application->hudEnabled_
                && !application->runOptions_.gpuTimingEnabled) {
                azurerender::RuntimeDiagnostics::instance().print(
                    "input",
                    "HUD GPU pass timing is disabled; restart with "
                    "--hud or --gpu-timing to enable it");
            }
        } else if (key == GLFW_KEY_F12) {
            application->screenshotRequested_ = true;
            azurerender::RuntimeDiagnostics::instance().print(
                "input", "Screenshot requested");
        } else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F3) {
            azurerender::applyShowcasePresetLook(
                application->renderSettings_,
                static_cast<std::uint32_t>(key - GLFW_KEY_F1));
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Showcase preset: "
                    + std::to_string(key - GLFW_KEY_F1 + 1) + " ("
                    + std::string(azurerender::showcasePresetName(
                        application->renderSettings_.showcasePreset))
                    + ")");
        } else if (key == GLFW_KEY_F4 || key == GLFW_KEY_F11) {
            if (application->sceneRenderer_ != nullptr) {
                application->sceneRenderer_->onAnimationKey(key, action);
            }
        } else if (key == GLFW_KEY_F10) {
            application->renderSettings_.innerOutlineEnabled =
                !application->renderSettings_.innerOutlineEnabled;
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Inner outlines: "
                    + std::string(
                        application->renderSettings_.innerOutlineEnabled ? "on" : "off"));
        } else if (key == GLFW_KEY_F9) {
            application->renderSettings_.stylizedLightingEnabled =
                !application->renderSettings_.stylizedLightingEnabled;
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Stylized lighting: "
                    + std::string(
                        application->renderSettings_.stylizedLightingEnabled ? "on" : "off"));
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
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Style mask strength: "
                    + std::to_string(
                        application->renderSettings_.styleMaskStrength));
        } else if (key == GLFW_KEY_F8) {
            application->renderSettings_.styleMaskStrength = std::min(
                application->renderSettings_.styleMaskStrength + 0.10F,
                2.0F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Style mask strength: "
                    + std::to_string(
                        application->renderSettings_.styleMaskStrength));
        } else if (key == GLFW_KEY_F5) {
            application->renderSettings_.diffuseBandThreshold = std::max(
                application->renderSettings_.diffuseBandThreshold - 0.05F,
                0.05F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Diffuse band threshold: "
                    + std::to_string(
                        application->renderSettings_.diffuseBandThreshold));
        } else if (key == GLFW_KEY_F6) {
            application->renderSettings_.diffuseBandThreshold = std::min(
                application->renderSettings_.diffuseBandThreshold + 0.05F,
                0.95F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Diffuse band threshold: "
                    + std::to_string(
                        application->renderSettings_.diffuseBandThreshold));
        }
#if AZURE_WITH_EDITOR
        else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_LEFT_BRACKET) {
            application->runOptions_.editorSession->context().beginEdit();
            application->renderSettings_.outline.strength = std::max(
                application->renderSettings_.outline.strength - 0.05F,
                0.0F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Editor outline strength: "
                    + std::to_string(
                        application->renderSettings_.outline.strength));
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_RIGHT_BRACKET) {
            application->runOptions_.editorSession->context().beginEdit();
            application->renderSettings_.outline.strength = std::min(
                application->renderSettings_.outline.strength + 0.05F,
                2.0F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Editor outline strength: "
                    + std::to_string(
                        application->renderSettings_.outline.strength));
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_MINUS) {
            application->runOptions_.editorSession->context().beginEdit();
            application->renderSettings_.grade.exposureEv = std::max(
                application->renderSettings_.grade.exposureEv - 0.25F,
                -8.0F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Editor exposure EV: "
                    + std::to_string(
                        application->renderSettings_.grade.exposureEv));
        } else if (application->runOptions_.editorMode
                   && key == GLFW_KEY_EQUAL) {
            application->runOptions_.editorSession->context().beginEdit();
            application->renderSettings_.grade.exposureEv = std::min(
                application->renderSettings_.grade.exposureEv + 0.25F,
                8.0F);
            azurerender::RuntimeDiagnostics::instance().print(
                "input",
                "Editor exposure EV: "
                    + std::to_string(
                        application->renderSettings_.grade.exposureEv));
        }
#endif
    }
}
