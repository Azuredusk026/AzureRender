#pragma once

#include "extensions/ISceneRenderer.hpp"
#include "assets/GltfLoader.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace azurerender {

// The stylized character pipeline, migrated from the monolith application
// class into the first concrete ISceneRenderer. It owns the loaded glTF
// asset, its GPU resources (buffers/textures/descriptors/pipelines), the
// animation state and the shadow/main scene passes that write the engine's
// HDR Scene Color attachment.
class CharacterSceneRenderer final : public ISceneRenderer {
public:
    CharacterSceneRenderer() = default;
    CharacterSceneRenderer(const CharacterSceneRenderer&) = delete;
    CharacterSceneRenderer& operator=(const CharacterSceneRenderer&) = delete;
    ~CharacterSceneRenderer() override = default;

    [[nodiscard]] std::string_view name() const noexcept override {
        return "character";
    }
    [[nodiscard]] SceneRendererCapabilities capabilities() const override;
    void onLoad(const RenderContext& context) override;
    void onSwapchainRecreate(const RenderContext& context) override;
    void updateFrame(const SceneFrameData& frame) override;
    void recordScene(const RenderContext& context) override;
    void onUnload(const RenderContext& context) override;
    void appendHudText(std::ostringstream& text) const override;
    [[nodiscard]] const RendererSceneState* sceneState() const noexcept override;
    void onAnimationKey(int key, int action) override;
    void restartPlayback() override;
    void setPlaybackPlaying(bool playing) override;
    void appendCaptureManifestFields(std::ostream& json) const override;

private:
    static constexpr std::size_t kMaxFramesInFlight = 2;

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
        GpuTexture faceSdf;
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

    struct MorphPushConstants {
        std::array<float, 2> weights{{0.0F, 0.0F}};
        std::array<float, 2> padding{{0.0F, 0.0F}};
        std::array<float, 16> gizmoTransform{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F,
        };
    };
    static_assert(sizeof(MorphPushConstants) == 80);

    struct UniformBufferObject {
        std::array<float, 16> model{};
        std::array<float, 16> modelViewProjection{};
        std::array<float, 16> lightModelViewProjection{};
        std::array<float, 4> cameraPosition{};
        std::array<float, 4> renderingParameters{};
        std::array<float, 4> showcaseParameters{};
        std::array<float, 4> qaParameters{};
        std::array<float, 4> faceLightDirection{};
        std::array<float, 4> faceSdfParameters{};
        std::array<float, 4> faceSdfShadowColor{};
    };

    // Engine context snapshot taken on onLoad.
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::string rampAtlasPath_;
    std::string environmentPath_;
    const RenderSettings* renderSettings_ = nullptr;
    // Engine-owned shadow map sampled by the material descriptor sets.
    VkImageView shadowImageView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;

    LoadedAsset asset_;
    std::optional<std::uint32_t> faceSdfHeadNode_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    std::vector<GpuMaterial> gpuMaterials_;
    GpuTexture environmentTexture_;
    GpuTexture toonRampTexture_;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBufferMemories_;
    std::vector<void*> uniformBufferMapped_;
    std::vector<VkBuffer> jointBuffers_;
    std::vector<VkDeviceMemory> jointBufferMemories_;
    std::vector<void*> jointBufferMapped_;
    std::vector<VkBuffer> oitIndexBuffers_;
    std::vector<VkDeviceMemory> oitIndexBufferMemories_;
    std::vector<void*> oitIndexBufferMapped_;
    std::size_t oitIndexBufferSize_ = 0;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline opaquePipeline_ = VK_NULL_HANDLE;
    VkPipeline opaqueDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline outlinePipeline_ = VK_NULL_HANDLE;
    VkPipeline backgroundPipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;

    // Per-frame state.
    std::size_t currentFrame_ = 0;
    std::size_t animationIndex_ = 0;
    float animationTime_ = 0.0F;
    bool animationPlaying_ = true;
    std::array<float, 3> cameraPosition_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> cameraTarget_{0.0F, 0.0F, 0.0F};
    float rotationAngle_ = 0.0F;
    std::int32_t selectedPrimitiveIndex_ = -1;
    std::array<float, 3> gizmoTranslation_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> gizmoRotation_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> gizmoScale_{1.0F, 1.0F, 1.0F};
    bool gizmoActive_ = false;
    std::uint32_t qaIsolationMode_ = 0;
    std::uint32_t qaEffectMode_ = 0;
    bool qaEffectEnabled_ = true;
    bool qaHarnessEnabled_ = false;
    std::array<float, 16> currentModel_{};
    RendererSceneState state_;

    // Resource creation.
    void createVertexBuffer();
    void createIndexBuffer();
    void createTexture();
    void createUniformBuffers();
    void createJointBuffers();
    void createOitIndexBuffers();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createGraphicsPipeline(const RenderContext& context);
    void destroyResources();

    // Frame recording.
    void updateUniformBuffer(const SceneFrameData& frame);
    void recordShadowPass(const RenderContext& context);
    void recordMainPass(const RenderContext& context);
    void drawPrimitive(
        const VkCommandBuffer commandBuffer,
        const AssetPrimitive& primitive,
        const std::uint32_t firstIndexOffset);
    void buildSceneState();
    void destroyGraphicsPipelinesForRecreate();
};

}  // namespace azurerender
