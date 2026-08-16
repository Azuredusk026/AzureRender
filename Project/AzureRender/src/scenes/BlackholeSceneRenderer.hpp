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

private:
    static constexpr std::size_t kMaxFramesInFlight = 2;

    struct BlackholeUniform {
        std::array<float, 4> cameraPosition{};
        std::array<float, 4> cameraRight{};
        std::array<float, 4> cameraUp{};
        std::array<float, 4> cameraForward{};
        std::array<float, 4> physics{1.0F, 40.0F, 900.0F, 1.0F};
        std::array<float, 4> cameraFov{0.9F, 1.7777F, 0.0F, 0.0F};
        std::array<float, 4> diskParameters{3.0F, 12.0F, 1.0F, 1.0F};
    };

    // Engine context snapshot taken on onLoad.
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::string shaderDirectory_;
    const RenderSettings* renderSettings_ = nullptr;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBufferMemories_;
    std::vector<void*> uniformBufferMapped_;

    std::size_t currentFrame_ = 0;
    std::array<float, 3> cameraPosition_{0.0F, 1.0F, 9.0F};
    std::array<float, 3> cameraTarget_{0.0F, 0.0F, 0.0F};
    float rotationAngle_ = 0.0F;
    float aspect_ = 1.0F;

    void createUniformBuffers();
    void createGraphicsPipeline(const RenderContext& context);
    void destroyResources();
    void updateUniformBuffer();
};

}  // namespace azurerender
