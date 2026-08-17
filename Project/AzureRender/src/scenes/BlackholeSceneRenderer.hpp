#pragma once

#include "extensions/ISceneRenderer.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace azurerender {

// The Schwarzschild black hole scene renderer: a per-pixel null-geodesic
// tracer implemented as a fullscreen pass. It writes the engine HDR Scene
// Color directly; the depth/normal attachments stay cleared (the renderer
// has no geometry to depth-test against).
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
    void onUnload(const RenderContext& context) override;
    void appendHudText(std::ostringstream& text) const override;
    void appendCaptureManifestFields(std::ostream& json) const override;

private:
    static constexpr std::size_t kMaxFramesInFlight = 2;
    // 2x2 stratified supersampling per pixel (denoise without TAA buffers).
    static constexpr int kSupersampleLevels = 4;

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
    };

    struct TaaUniform {
        float blendWeight = 0.35F;
        float bloomThreshold = 1.2F;
        float bloomIntensity = 0.55F;
        float renderWidth = 1280.0F;
    };

    // Engine context snapshot taken on onLoad.
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::string shaderDirectory_;
    const RenderSettings* renderSettings_ = nullptr;

    // Trace pass (writes the private ping-pong HDR texture).
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBufferMemories_;
    std::vector<void*> uniformBufferMapped_;

    // Private full-screen HDR ping-pong textures for the tracer.
    VkRenderPass traceRenderPass_ = VK_NULL_HANDLE;
    std::array<VkFramebuffer, 2> traceFramebuffers_{};
    std::array<VkImage, 2> traceImages_{};
    std::array<VkDeviceMemory, 2> traceImageMemories_{};
    std::array<VkImageView, 2> traceImageViews_{};
    VkSampler traceSampler_ = VK_NULL_HANDLE;
    std::size_t tracePing_ = 0;

    // TAA + bloom pass (reads the ping-pong textures, writes Scene Color).
    VkDescriptorSetLayout taaDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool taaDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet taaDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout taaPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline taaPipeline_ = VK_NULL_HANDLE;
    std::vector<VkBuffer> taaUniformBuffers_;
    std::vector<VkDeviceMemory> taaUniformBufferMemories_;
    std::vector<void*> taaUniformBufferMapped_;

    std::size_t currentFrame_ = 0;
    std::array<float, 3> cameraPosition_{0.0F, 0.4F, 12.0F};
    std::array<float, 3> cameraTarget_{0.0F, 0.0F, 0.0F};
    float rotationAngle_ = 0.0F;
    float aspect_ = 1.0F;
    float simulationTime_ = 0.0F;
    std::uint32_t renderWidth_ = 1280;
    std::uint32_t renderHeight_ = 720;
    float blendWeight_ = 0.35F;

    void createUniformBuffers();
    void createTraceResources(const RenderContext& context);
    void transitionInitialLayouts();
    void createTaaPipeline(const RenderContext& context);
    void createGraphicsPipeline(const RenderContext& context);
    void destroyResources();
    void updateUniformBuffer();
    void updateTaaUniform();
};

}  // namespace azurerender
