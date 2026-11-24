#pragma once

#include "extensions/ISceneRenderer.hpp"
#include "render/RenderSettings.hpp"
#include "render/EnvironmentAsset.hpp"
#include "render/ShaderSharedTypes.hpp"
#include "rhi/IGpuAllocator.hpp"
#include "rhi/Rhi.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace azurerender {

// The Schwarzschild black hole scene renderer: a per-pixel null-geodesic
// tracer implemented as fullscreen passes. A raw trace is accumulated into
// ping-pong HDR history before being composited into engine Scene Color.
class BlackholeSceneRenderer final : public ISceneRenderer {
public:
    BlackholeSceneRenderer() = default;
    BlackholeSceneRenderer(const BlackholeSceneRenderer&) = delete;
    BlackholeSceneRenderer& operator=(const BlackholeSceneRenderer&) = delete;
    ~BlackholeSceneRenderer() override = default;

    [[nodiscard]] std::string_view name() const noexcept override {
        return "blackhole";
    }
    [[nodiscard]] SceneRendererCapabilities capabilities() const override;
    void onLoad(const RenderContext& context) override;
    void onSwapchainRecreate(const RenderContext& context) override;
    void updateFrame(const SceneFrameData& frame) override;
    void recordScene(const RenderContext& context) override;
    void registerPasses(RenderGraph& graph, const SceneGraphResources& resources,
                        const RenderContext& context) override;
    void onUnload(const RenderContext& context) override;
    void appendHudText(std::ostringstream& text) const override;
    void appendCaptureManifestFields(std::ostream& json) const override;

private:
    void recordShadowClear(const RenderContext& context);
    void recordTrace(const RenderContext& context);
    void recordTemporal(const RenderContext& context);
    void recordComposite(const RenderContext& context);
    static constexpr std::size_t kMaxFramesInFlight = 2;
    // 2x2 stratified supersampling per pixel (denoise without TAA buffers).
    struct BlackholeUniform {
        std::array<float, 4> cameraPosition{};
        std::array<float, 4> cameraRight{};
        std::array<float, 4> cameraUp{};
        std::array<float, 4> cameraForward{};
        // rs, escapeRadius, maxSteps, simulationTime
        std::array<float, 4> physics{1.0F, 40.0F, 900.0F, 0.0F};
        // fovRadians, aspect, supersampleLevels, renderWidth
        std::array<float, 4> cameraFov{0.9F, 1.7777F, 4.0F, 1280.0F};
        // diskInner, diskOuter, temperatureScale, shiftMax
        std::array<float, 4> diskParameters{2.1F, 12.0F, 1.0F, 1.25F};
        // nearStepScale, reserved...
        std::array<float, 4> quality{0.48F, 0.0F, 0.0F, 0.0F};
    };

    struct TaaUniform {
        float blendWeight = 0.35F;
        float bloomThreshold = 1.2F;
        float bloomIntensity = 0.30F;
        float renderWidth = 1280.0F;
    };

    using BloomPushConstants = shader::BloomParameters;
    static_assert(sizeof(BloomPushConstants) == 8);
    static constexpr std::size_t kBloomLevelCount = 4;

    // Engine-owned allocator borrowed for the renderer's lifetime.
    rhi::IGpuAllocator* allocator_ = nullptr;
    // Resource creation and recording backend borrowed for the lifetime.
    rhi::IRhi* rhi_ = nullptr;
    std::string shaderDirectory_;
    SceneEnvironmentSource environmentSource_;
    const RenderSettings* renderSettings_ = nullptr;
    RenderSettings frameRenderSettings_;
    bool computeBloomEnabled_ = false;

    struct GpuEnvironment {
        rhi::GpuImage image;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    } environment_;

    // Trace pass (writes the private ping-pong HDR texture).
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<rhi::GpuBuffer> uniformBuffers_;

    // Private full-screen HDR raw trace and accumulated history textures.
    VkRenderPass traceRenderPass_ = VK_NULL_HANDLE;
    VkFramebuffer traceFramebuffer_ = VK_NULL_HANDLE;
    rhi::GpuImage traceImage_;
    VkImageView traceImageView_ = VK_NULL_HANDLE;
    std::array<VkFramebuffer, 2> historyFramebuffers_{};
    std::array<rhi::GpuImage, 2> historyImages_{};
    std::array<VkImageView, 2> historyImageViews_{};
    VkSampler traceSampler_ = VK_NULL_HANDLE;
    std::size_t historyWriteIndex_ = 0;
    bool historyValid_ = false;
    bool frameSeen_ = false;
    bool captureActive_ = false;
    std::uint64_t historyResetCount_ = 0;
    float previousRotationAngle_ = 0.0F;

    // TAA + bloom pass (raw trace + previous history -> next history).
    VkDescriptorSetLayout taaDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool taaDescriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight * 2> taaDescriptorSets_{};
    VkPipelineLayout taaPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline taaPipeline_ = VK_NULL_HANDLE;
    std::vector<rhi::GpuBuffer> taaUniformBuffers_;

    // Compute bloom pyramid: four downsample levels plus a half-resolution
    // composite image sampled by the temporal accumulation fragment pass.
    std::vector<rhi::GpuImage> bloomLevels_;
    std::vector<VkImageView> bloomLevelViews_;
    std::vector<VkExtent2D> bloomLevelExtents_;
    rhi::GpuImage bloomCompositeImage_;
    VkImageView bloomCompositeView_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout bloomDownsampleSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool bloomDownsamplePool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> bloomDownsampleSets_;
    VkPipelineLayout bloomDownsamplePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline bloomDownsamplePipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout bloomCompositeSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool bloomCompositePool_ = VK_NULL_HANDLE;
    VkDescriptorSet bloomCompositeSet_ = VK_NULL_HANDLE;
    VkPipelineLayout bloomCompositePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline bloomCompositePipeline_ = VK_NULL_HANDLE;

    // Final copy from accumulated history into engine Scene Color.
    VkDescriptorSetLayout compositeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool compositeDescriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> compositeDescriptorSets_{};
    VkPipelineLayout compositePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline compositePipeline_ = VK_NULL_HANDLE;

    std::size_t currentFrame_ = 0;
    std::array<float, 3> cameraPosition_{0.0F, 4.0F, 24.0F};
    std::array<float, 3> cameraTarget_{0.0F, 0.0F, 0.0F};
    float rotationAngle_ = 0.0F;
    float aspect_ = 1.0F;
    float simulationTime_ = 0.0F;
    std::uint32_t renderWidth_ = 1280;
    std::uint32_t renderHeight_ = 720;
    float blendWeight_ = 0.35F;
    BlackholeQuality quality_ = BlackholeQuality::Cinematic;
    BlackholeCamera camera_ = BlackholeCamera::Front;
    std::uint32_t maxTraceSteps_ = 1800;
    std::uint32_t samplesPerPixel_ = 4;
    float nearStepScale_ = 0.48F;

    void createUniformBuffers();
    void createEnvironmentTexture();
    void createTraceResources(const RenderContext& context);
    void createBloomResources(const RenderContext& context);
    void createBloomDescriptors();
    void createBloomPipelines();
    void recordBloom(const RenderContext& context);
    void transitionInitialLayouts();
    void updateTemporalDescriptorSets();
    void createTaaPipeline(const RenderContext& context);
    void createCompositePipeline(const RenderContext& context);
    void createGraphicsPipeline(const RenderContext& context);
    void destroySizeDependentResources();
    void destroyResources();
    void invalidateHistory();
    void updateUniformBuffer();
    void updateTaaUniform();

    VkPipeline createFullscreenPipeline(
        const std::string& fragmentShader,
        VkPipelineLayout layout,
        VkRenderPass renderPass,
        std::uint32_t colorAttachmentCount = 1);
};

}  // namespace azurerender
