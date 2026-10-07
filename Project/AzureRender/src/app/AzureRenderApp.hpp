#pragma once
#include <deque>

#ifndef AZURE_WITH_EDITOR
#define AZURE_WITH_EDITOR 1
#endif
#include "AzureRenderOptions.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/ModuleAssembly.hpp"
#include "validation/ValidationTransport.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/EngineSettings.hpp"
#include "runtime/GameInputReplay.hpp"
#include "runtime/IScriptRuntime.hpp"
#include "runtime/PresentationRuntime.hpp"
#include "runtime/GameUi.hpp"
#include "render/GameUiRenderer.hpp"
#include "assets/GltfLoader.hpp"
#include "render/RenderSettings.hpp"
#include "render/TransientResourcePool.hpp"
#include "resources/ResourceLocator.hpp"
#include "rhi/GpuAllocator.hpp"
#include "rhi/UploadRingBuffer.hpp"
#include "rhi/VulkanRhi.hpp"
#include "rhi/WorkerCommandPools.hpp"
#include "render/RecordingWorkerPool.hpp"
#if AZURE_WITH_EDITOR
#include "devtools/ShaderHotReloader.hpp"
#include "render/RenderViewService.hpp"
#include "editor/preview/AssetThumbnailService.hpp"
#endif

#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace azurerender {
class EditorSession;
class EditorAutomation;
class GlfwFrontend;
class ImGuiEditorLayer;
class ISceneRenderer;
struct RenderContext;
struct SceneFrameData;
}

class AzureRenderApp final {
public:
    AzureRenderApp();
    AzureRenderApp(const AzureRenderApp&) = delete;
    AzureRenderApp& operator=(const AzureRenderApp&) = delete;
    ~AzureRenderApp();

    void run(const AzureRenderOptions& options = {});
    [[nodiscard]] const azurerender::RenderSettings& currentRenderSettings()
        const { return effectiveRenderSettings_; }

private:
#if AZURE_WITH_EDITOR
    std::unique_ptr<azurerender::ShaderHotReloader> shaderReloader_;
    std::unique_ptr<azurerender::RenderViewService> renderViews_;
    std::unique_ptr<azurerender::AssetThumbnailService> thumbnails_;
    std::vector<azurerender::RenderViewHandle> toolViews_;
    std::uint64_t cameraPreview_=0;
    std::string activeShaderDirectory_,previewDiagnostic_;
    std::uint64_t shaderReplacements_=0;
    std::uint64_t previewDocumentRevision_=0;
    nlohmann::json toolViewDescriptors_,devtoolsReport_=nlohmann::json::object();
    void initializeDevtools();
    void pollDevtools();
    void finishDevtools();
    void invalidatePreviews();
    nlohmann::json previewOperation(const std::string&,const nlohmann::json&);
    azurerender::RenderViewDescriptor previewDescriptor(const nlohmann::json&);
#endif
    std::uint64_t graphicsSubmission_=0,graphicsCompleted_=0;
    std::array<std::uint64_t,2> graphicsFrameSubmissions_{};
    azurerender::SettingRegistry playerSettings_;
    azurerender::SettingRegistry* engineSettings_=nullptr;
    azurerender::RenderSettings effectiveRenderSettings_;
    void initializeSettings();
    std::uint64_t settingsRevision_=0;
    std::unique_ptr<azurerender::ObservationRegistry> observations_;
    std::unique_ptr<azurerender::ValidationService> validation_;
    std::unique_ptr<azurerender::ValidationTransport> validationTransport_;
    std::uint64_t validationScreenshots_=0;
    void initializeValidation();
    void finishValidation();
    static constexpr std::uint32_t kShadowMapSize = 2048;
    static constexpr std::size_t kMaxFramesInFlight = 2;
    static constexpr std::uint32_t kTimestampQueryCount = 4;
    static constexpr std::size_t kMaxHudVertices = 24576;
    static constexpr VkFormat kHdrSceneColorFormat =
        VK_FORMAT_R16G16B16A16_SFLOAT;

#if defined(AZURERENDER_ENABLE_VALIDATION)
    static constexpr bool kEnableValidation = true;
#else
    static constexpr bool kEnableValidation = false;
#endif

    struct QueueFamilyIndices {
        std::optional<std::uint32_t> graphics;
        std::optional<std::uint32_t> present;

        [[nodiscard]] bool complete() const {
            return graphics.has_value() && present.has_value();
        }
    };

    struct SwapchainSupport {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct PostProcessPushConstants {
        float strength = 0.40F;
        float depthThreshold = 0.18F;
        float normalThreshold = 0.20F;
        float diagnosticView = 0.0F;
        float exposureEv = 0.0F;
        float toneMappingEnabled = 1.0F;
        float bloomStrength = 0.16F;
        float bloomIsolation = 0.0F;
        std::array<float, 4> outlineColor{0.008F, 0.013F, 0.022F, 1.0F};
        std::array<float, 4> gradeParameters{1.0F, 1.0F, 1.05F, 0.0F};
        std::array<float, 4> gradeTint{1.0F, 1.0F, 1.0F, 0.0F};
    };
    static_assert(sizeof(PostProcessPushConstants) == 80);

    struct GpuTimingAccumulator {
        std::uint64_t samples = 0;
        double shadowTotalMs = 0.0;
        double sceneTotalMs = 0.0;
        double postProcessTotalMs = 0.0;
        double frameTotalMs = 0.0;
        double frameMinMs = 0.0;
        double frameMaxMs = 0.0;
        // Retained per-frame totals so the report can publish percentiles
        // instead of averages alone. Percentiles are the meaningful signal for
        // frame-time regressions because a single stall moves max but not mean.
        std::deque<double> frameSamplesMs;
    };

    // Per-frame draw submission counters. These are the primary evidence that
    // a structural change actually reduced CPU-side submission cost, so they
    // are collected whenever GPU timing is enabled.
    struct SubmissionCounters {
        std::uint64_t indirectDrawCalls = 0;
        std::uint64_t instances = 0;
        std::uint64_t visibleInstances = 0;
        double recordingMilliseconds = 0.0;
        double graphPreparationMilliseconds = 0.0;
        double workerWaitMilliseconds = 0.0;
        double graphExecutionMilliseconds = 0.0;
        std::uint64_t frameAttempts = 0;
        std::uint64_t completedCpuFrames = 0;
        double frameSlotWaitMilliseconds = 0.0;
        double acquireMilliseconds = 0.0;
        double submitMilliseconds = 0.0;
        double presentMilliseconds = 0.0;
        double cpuFrameMilliseconds = 0.0;
        std::deque<double> workSamplesMs, totalSamplesMs, waitSamplesMs, physicsSamplesMs;
        std::deque<std::size_t> physicsStepCounts;
        std::uint64_t workerRecordedPasses = 0;
        std::uint64_t workerRecordedChunks = 0;
        std::uint64_t frames = 0;
        std::uint64_t drawCalls = 0;
        std::uint64_t descriptorSetBinds = 0;
        std::uint64_t pipelineBinds = 0;
        std::uint64_t pushConstantUpdates = 0;
    };

    struct HudVertex {
        std::array<float, 2> position{};
        std::array<std::uint8_t, 4> color{};
    };
    static_assert(sizeof(HudVertex) == 12);

    std::unique_ptr<azurerender::GlfwFrontend> frontend_;
#if AZURE_WITH_EDITOR
    std::unique_ptr<azurerender::ImGuiEditorLayer> editorLayer_;
#endif
    azurerender::RuntimeLifecycle runtime_;
    azurerender::ModuleAssembly runtimeModules_;
    std::unique_ptr<azurerender::LevelSession> levelSession_;
    std::unique_ptr<azurerender::GameRuntime> gameRuntime_;
    std::optional<azurerender::GameInputReplay> gameInputReplay_;
    nlohmann::json resourceFrameSamples_=nlohmann::json::array();
    std::uint64_t sampledLevelRevision_=0;
    nlohmann::json gameRouteFrames_=nlohmann::json::array();
    std::unique_ptr<azurerender::IScriptRuntime> scriptRuntime_;
    std::unique_ptr<azurerender::PresentationRuntime> presentationRuntime_;
    std::unique_ptr<azurerender::GameUiRenderer> gameUiRenderer_;
    std::unique_ptr<azurerender::GameUi> gameUi_;
#if AZURE_WITH_EDITOR
    std::unique_ptr<azurerender::EditorAutomation> editorAutomation_;
#endif
    std::string gameUiPath_,editorResourceSignature_;
    std::uint64_t gameplayFrame_=0,uiSerial_=0,uiDrawCalls_=0,animationFrames_=0,audioStarts_=0;
    std::uint64_t editorPreviewFrames_=0;
    std::vector<std::string> presentationErrors_;
    double pausedTimeOffset_ = 0.0;
    bool framebufferResized_ = false;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily_ = 0;
    double timestampPeriodNanoseconds_ = 0.0;
    std::uint32_t timestampValidBits_ = 0;
    // max(minUniformBufferOffsetAlignment, nonCoherentAtomSize) of the picked
    // device; every upload ring slice honors it.
    VkDeviceSize uploadRingAlignment_ = 1;
    bool hdrSceneColorFormatSupported_ = false;
    bool rgba16fStorageImageSupported_ = false;
    bool computeShaderSupported_ = false;
    bool indirectFirstInstanceSupported_ = false;
    bool multiDrawIndirectSupported_ = false;
    // Device supports the descriptor-indexing pair the character renderer's
    // bindless texture array needs (runtimeDescriptorArray plus
    // shaderSampledImageArrayNonUniformIndexing).
    bool bindlessTexturesSupported_ = false;
    std::vector<VkQueryPool> timestampQueryPools_;
    std::array<bool, kMaxFramesInFlight> timestampQuerySubmitted_{};
    GpuTimingAccumulator gpuTiming_;
    SubmissionCounters submissionCounters_;
    // Owns all GPU memory for the process. Initialized right after the logical
    // device and destroyed before it.
    azurerender::rhi::GpuAllocator gpuAllocator_;
    // All per-frame CPU-to-GPU uploads are slices of this ring.
    azurerender::rhi::UploadRingBuffer uploadRing_;
    // Resource creation backend handed to scene renderers.
    std::unique_ptr<azurerender::rhi::VulkanRhi> rhi_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    VkExtent2D renderExtent_{};
    VkExtent2D requestedEditorViewportExtent_{};
    bool editorViewportResizeRequested_ = false;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<azurerender::rhi::GpuImage> editorViewportImages_;
    std::vector<VkImageView> editorViewportImageViews_;
    VkSampler editorViewportSampler_ = VK_NULL_HANDLE;
    std::vector<azurerender::rhi::GpuImage> sceneColorImages_;
    std::vector<VkImageView> sceneColorImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    std::vector<VkFramebuffer> postProcessFramebuffers_;
    std::vector<VkFramebuffer> editorUiFramebuffers_;
    std::vector<azurerender::rhi::GpuImage> depthImages_;
    std::vector<VkImageView> depthImageViews_;
    VkFormat normalFormat_ = VK_FORMAT_R8G8B8A8_UNORM;
    std::vector<azurerender::rhi::GpuImage> normalImages_;
    std::vector<VkImageView> normalImageViews_;
    VkFormat shadowFormat_ = VK_FORMAT_UNDEFINED;
    azurerender::rhi::GpuImage shadowImage_;
    VkImageView shadowImageView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;
    VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkRenderPass postProcessRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass editorUiRenderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout postProcessDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool postProcessDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> postProcessDescriptorSets_;
    VkPipelineLayout postProcessPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline innerOutlinePipeline_ = VK_NULL_HANDLE;
    VkPipeline hudPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout hudPipelineLayout_ = VK_NULL_HANDLE;
    VkSampler screenAttachmentSampler_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::unique_ptr<azurerender::rhi::WorkerCommandPools> workerCommandPools_;
    azurerender::RecordingWorkerPool recordingWorkers_{4};
    AzureRenderOptions runOptions_;
    azurerender::ResourceLocator resourceLocator_;
    std::string resolvedAssetPath_;
    std::string selectedGpuName_;
    // Byte offsets of this frame's HUD vertex slice inside uploadRing_.
    std::array<VkDeviceSize, kMaxFramesInFlight> hudVertexOffsets_{};
    std::vector<HudVertex> hudScratch_;
    std::unique_ptr<azurerender::TransientResourcePool> capturePool_;
    std::uint64_t captureSerial_ = 0;
    VkDeviceSize readbackBufferSize_ = 0;
    std::array<std::uint32_t, kMaxFramesInFlight> hudVertexCounts_{};
    std::int32_t selectedPrimitiveIndex_ = -1;
    bool ecsRenderableLogged_ = false;
    float pendingPickX_ = 0.0F;
    float pendingPickY_ = 0.0F;
    bool pendingPickRequested_ = false;
    std::array<float, 3> cameraPosition_{2.8F, 2.1F, 3.2F};
    bool gameViewportFocus_ = true, gameCursorPrimed_ = false;
    double gameCursorX_ = 0, gameCursorY_ = 0;
    static void scrollCallback(GLFWwindow* window, double x, double y);
    std::array<float, 3> cameraTarget_{0.0F, 0.0F, 0.0F};
    float rotationAngle_ = 0.0F;
    float rotationSpeed_ = 0.65F;
    double lastRotationTime_ = 0.0;
    bool autoRotate_ = true;
    azurerender::RenderSettings renderSettings_;
    std::unique_ptr<azurerender::ISceneRenderer> sceneRenderer_;
    std::unique_ptr<azurerender::ISceneRenderer> preparedRenderer_;
    std::string rendererResourceKey_, preparedResourceKey_;
    azurerender::RenderSettings preparedRenderSettings_;
    bool preparedRendererReady_=false;
    bool hudEnabled_ = false;
    bool editorUiEnabled_ = false;
    bool qaHarnessEnabled_ = false;
    std::string qaCameraName_ = "none";
    std::string qaLightName_ = "current";
    std::string qaEffectName_ = "none";
    std::string qaEffectStateName_ = "enabled";
    std::string qaIsolationName_ = "beauty";
    std::uint32_t qaIsolationMode_ = 0;
    std::uint32_t qaEffectMode_ = 0;
    bool qaEffectEnabled_ = true;
    bool screenshotRequested_ = false;
    std::string pendingScreenshotLabel_;
    bool fixedSimulation_ = false;
    bool fixedSimulationStarted_ = false;
    float fixedDeltaSeconds_ = 0.0F;
    double fixedSimulationTime_ = 0.0;
    std::uint64_t capturedFrames_ = 0;
    std::uint32_t technicalSequenceChapter_ =
        std::numeric_limits<std::uint32_t>::max();
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    std::size_t currentFrame_ = 0;

    void initWindow();
    void initVulkan(const std::string& assetPath);
    void initEditorUi();
    void mainLoop(std::uint64_t smokeFrameLimit);
    void activatePortfolioOrbit();
    void configureQaHarness();
    void updateTechnicalSequenceState(std::uint64_t frameIndex);
    void prepareCaptureDirectory();
    void writeCaptureManifest(std::uint64_t renderedFrames) const;
    void createTimestampQueryPools();
    void collectGpuTiming(std::size_t frameIndex);
    void appendFrameTimingCsv(
        double shadowMs,
        double sceneMs,
        double postProcessMs,
        double frameMs) const;
    void printGpuTimingSummary() const;
    [[nodiscard]] std::string renderPathName() const;
    void cleanup();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createEditorViewportResources();
    void createSceneColorResources();
    void createDepthResources();
    void createNormalResources();
    void createShadowResources();
    void createRenderPass();
    void createPostProcessRenderPass();
    void createEditorUiRenderPass();
    void createPostProcessDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createPostProcessFramebuffers();
    void createEditorUiFramebuffers();
    void createSwapchainSemaphores();
    void createCommandPool();
    void createPostProcessDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();

    void createSceneRenderer();
    bool preloadLevelRenderer(const azurerender::Level& level);
    void prepareLevelRenderer(const azurerender::Level& level);
    void synchronizeEditorRuntime();
    void synchronizeGameUi();
    void updateGameUi(double delta);
    azurerender::PresentationRuntime* activePresentation();
    azurerender::RuntimeLifecycle* activeRuntime();
    azurerender::GameRuntime* activeGame();
    azurerender::IScriptRuntime* activeScripts();
    azurerender::AssetDatabase* activeAssets();
    void buildRenderContext(azurerender::RenderContext& context);
    azurerender::scene::SceneDescription resolveRenderDescription(
        const azurerender::SceneDocument& document) const;
    void buildSceneFrameData(azurerender::SceneFrameData& frame);

    void drawFrame();
    void pickPrimitive(float viewportX, float viewportY);
    void updateGizmoScreenData();
    void updateHudBuffer(std::size_t frameIndex);
    void recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        std::uint32_t imageIndex,
        VkBuffer screenshotBuffer,
        const azurerender::SceneFrameData& frame);
    void saveScreenshot(
        const void* pixelData,
        std::uint32_t width,
        std::uint32_t height,
        const std::string& outputPath = {}) const;
    void recreateSwapchain();
    void recreateEditorViewportResources();
    void cleanupEditorViewportResources(bool destroySampler);
    void cleanupSwapchain();

    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    [[nodiscard]] bool checkValidationLayerSupport() const;
    [[nodiscard]] std::vector<const char*> requiredInstanceExtensions() const;

    [[nodiscard]] static VkSurfaceFormatKHR chooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats);
    [[nodiscard]] static VkPresentModeKHR choosePresentMode(
        const std::vector<VkPresentModeKHR>& presentModes);
    [[nodiscard]] VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    [[nodiscard]] static std::vector<char> readBinaryFile(const std::string& path);
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;
    [[nodiscard]] VkFormat findDepthFormat() const;
    void copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size) const;
    void transitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        std::uint32_t mipLevels = 1) const;
    void copyBufferToImage(
        VkBuffer source,
        VkImage destination,
        std::uint32_t width,
        std::uint32_t height) const;
    void generateMipmaps(
        VkImage image,
        VkFormat format,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t mipLevels) const;
    [[nodiscard]] VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        std::uint32_t mipLevels = 1) const;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(
        GLFWwindow* window,
        int key,
        int scancode,
        int action,
        int modifiers);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData);
    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
};
