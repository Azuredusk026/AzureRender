#pragma once

#include "assets/GltfLoader.hpp"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

struct AzureRenderOptions {
    std::string assetPath;
    std::uint64_t smokeFrameLimit = 0;
    std::string captureDirectory;
    std::uint64_t captureFrameLimit = 0;
    std::uint32_t captureFps = 60;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool portfolioMode = false;
    bool gpuTimingEnabled = false;
    std::string gpuTimingOutput;
    std::uint32_t diagnosticView = 0;
    bool stylizedLightingEnabled = true;
    bool innerOutlineEnabled = true;
    bool hudEnabled = false;
    bool technicalSequence = false;
    std::string qaCamera;
    std::string qaLight;
    std::string qaEffect;
    std::string qaEffectState;
    std::string qaIsolation;
};

class AzureRenderApp final {
public:
    AzureRenderApp() = default;
    AzureRenderApp(const AzureRenderApp&) = delete;
    AzureRenderApp& operator=(const AzureRenderApp&) = delete;
    ~AzureRenderApp();

    void run(const AzureRenderOptions& options = {});

private:
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

    struct UniformBufferObject {
        std::array<float, 16> model{};
        std::array<float, 16> modelViewProjection{};
        std::array<float, 16> lightModelViewProjection{};
        std::array<float, 4> cameraPosition{};
        std::array<float, 4> renderingParameters{};
        std::array<float, 4> showcaseParameters{};
        std::array<float, 4> qaParameters{};
    };

    struct GpuTexture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct GpuMaterial {
        GpuTexture baseColor;
        GpuTexture normal;
        GpuTexture metallicRoughness;
        GpuTexture specularEmissive;
        GpuTexture styleMask;
        GpuTexture matcap;
        GpuTexture hairData;
    };

    struct MaterialPushConstants {
        float alphaCutoff = 0.5F;
        std::uint32_t alphaMode = 0;
        float emissiveStrength = 0.0F;
        float showcasePlatform = 0.0F;
        std::array<float, 4> aoColor{1.0F, 1.0F, 1.0F, 0.0F};
        std::array<float, 4> lamShadowColor{1.0F, 1.0F, 1.0F, 0.0F};
        std::array<float, 4> matcapColor{1.0F, 1.0F, 1.0F, 0.0F};
        std::array<float, 4> hairParameters{64.0F, 0.15F, 4.0F, 0.0F};
        std::array<float, 4> styleParameters{1.0F, 1.0F, 1.0F, 1.0F};
        std::array<float, 4> featureParameters{1.0F, 1.0F, 1.0F, 1.0F};
        std::uint32_t materialClass = 0;
        std::uint32_t materialFeatures = 0;
        std::uint32_t materialProfileVersion = 1;
        std::uint32_t padding = 0;
    };
    static_assert(sizeof(MaterialPushConstants) == 128);

    struct PostProcessPushConstants {
        float strength = 0.40F;
        float depthThreshold = 0.18F;
        float normalThreshold = 0.20F;
        float diagnosticView = 0.0F;
        float exposureEv = 0.0F;
        float toneMappingEnabled = 1.0F;
        float padding0 = 0.0F;
        float padding1 = 0.0F;
    };
    static_assert(sizeof(PostProcessPushConstants) == 32);

    struct GpuTimingAccumulator {
        std::uint64_t samples = 0;
        double shadowTotalMs = 0.0;
        double sceneTotalMs = 0.0;
        double postProcessTotalMs = 0.0;
        double frameTotalMs = 0.0;
        double frameMinMs = 0.0;
        double frameMaxMs = 0.0;
    };

    struct HudVertex {
        std::array<float, 2> position{};
        std::array<std::uint8_t, 4> color{};
    };
    static_assert(sizeof(HudVertex) == 12);

    GLFWwindow* window_ = nullptr;
    bool glfwInitialized_ = false;
    bool framebufferResized_ = false;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    double timestampPeriodNanoseconds_ = 0.0;
    std::uint32_t timestampValidBits_ = 0;
    bool hdrSceneColorFormatSupported_ = false;
    std::vector<VkQueryPool> timestampQueryPools_;
    std::array<bool, kMaxFramesInFlight> timestampQuerySubmitted_{};
    GpuTimingAccumulator gpuTiming_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkImage> sceneColorImages_;
    std::vector<VkDeviceMemory> sceneColorImageMemories_;
    std::vector<VkImageView> sceneColorImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    std::vector<VkFramebuffer> postProcessFramebuffers_;
    std::vector<VkImage> depthImages_;
    std::vector<VkDeviceMemory> depthImageMemories_;
    std::vector<VkImageView> depthImageViews_;
    VkFormat normalFormat_ = VK_FORMAT_R8G8B8A8_UNORM;
    std::vector<VkImage> normalImages_;
    std::vector<VkDeviceMemory> normalImageMemories_;
    std::vector<VkImageView> normalImageViews_;
    VkFormat shadowFormat_ = VK_FORMAT_UNDEFINED;
    VkImage shadowImage_ = VK_NULL_HANDLE;
    VkDeviceMemory shadowImageMemory_ = VK_NULL_HANDLE;
    VkImageView shadowImageView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;
    VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkRenderPass postProcessRenderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout postProcessDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorPool postProcessDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkDescriptorSet> postProcessDescriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout postProcessPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline opaquePipeline_ = VK_NULL_HANDLE;
    VkPipeline opaqueDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline outlinePipeline_ = VK_NULL_HANDLE;
    VkPipeline backgroundPipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline innerOutlinePipeline_ = VK_NULL_HANDLE;
    VkPipeline hudPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout hudPipelineLayout_ = VK_NULL_HANDLE;
    VkSampler screenAttachmentSampler_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    AzureRenderOptions runOptions_;
    std::string resolvedAssetPath_;
    std::string selectedGpuName_;
    LoadedAsset asset_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    std::vector<GpuMaterial> gpuMaterials_;
    GpuTexture environmentTexture_;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBufferMemories_;
    std::vector<void*> uniformBufferMapped_;
    std::vector<VkBuffer> jointBuffers_;
    std::vector<VkDeviceMemory> jointBufferMemories_;
    std::vector<void*> jointBufferMapped_;
    std::vector<VkBuffer> hudVertexBuffers_;
    std::vector<VkDeviceMemory> hudVertexBufferMemories_;
    std::vector<void*> hudVertexBufferMapped_;
    std::array<std::uint32_t, kMaxFramesInFlight> hudVertexCounts_{};
    std::array<float, 16> currentModel_{};
    std::array<float, 3> cameraPosition_{2.8F, 2.1F, 3.2F};
    std::array<float, 3> cameraTarget_{0.0F, 0.0F, 0.0F};
    float rotationAngle_ = 0.0F;
    float rotationSpeed_ = 0.65F;
    double lastRotationTime_ = 0.0;
    bool autoRotate_ = true;
    std::size_t animationIndex_ = 0;
    float animationTime_ = 0.0F;
    bool animationPlaying_ = true;
    bool stylizedLightingEnabled_ = true;
    float styleMaskStrength_ = 1.0F;
    float diffuseBandThreshold_ = 0.40F;
    std::uint32_t showcasePreset_ = 0;
    bool innerOutlineEnabled_ = true;
    bool silhouetteOutlineEnabled_ = true;
    std::uint32_t diagnosticView_ = 0;
    bool hudEnabled_ = false;
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
    bool fixedSimulation_ = false;
    bool fixedSimulationStarted_ = false;
    float fixedDeltaSeconds_ = 0.0F;
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
    void mainLoop(std::uint64_t smokeFrameLimit);
    void activatePortfolioOrbit();
    void configureQaHarness();
    void updateTechnicalSequenceState(std::uint64_t frameIndex);
    void prepareCaptureDirectory();
    void writeCaptureManifest(std::uint64_t renderedFrames) const;
    void createTimestampQueryPools();
    void collectGpuTiming(std::size_t frameIndex);
    void printGpuTimingSummary() const;
    void cleanup();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createSceneColorResources();
    void createDepthResources();
    void createNormalResources();
    void createShadowResources();
    void createRenderPass();
    void createPostProcessRenderPass();
    void createDescriptorSetLayout();
    void createPostProcessDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createPostProcessFramebuffers();
    void createSwapchainSemaphores();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createTexture();
    void createUniformBuffers();
    void createJointBuffers();
    void createHudBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createPostProcessDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();

    void drawFrame();
    void updateUniformBuffer(std::size_t frameIndex);
    void updateHudBuffer(std::size_t frameIndex);
    void recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        std::uint32_t imageIndex,
        VkBuffer screenshotBuffer);
    void saveScreenshot(
        VkDeviceMemory screenshotMemory,
        std::uint32_t width,
        std::uint32_t height,
        const std::string& outputPath = {}) const;
    void recreateSwapchain();
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
    [[nodiscard]] std::uint32_t findMemoryType(
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const;
    [[nodiscard]] VkFormat findDepthFormat() const;
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& memory) const;
    void copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size) const;
    void transitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout) const;
    void copyBufferToImage(
        VkBuffer source,
        VkImage destination,
        std::uint32_t width,
        std::uint32_t height) const;
    void createImage(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImage& image,
        VkDeviceMemory& memory) const;
    [[nodiscard]] VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect) const;

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
