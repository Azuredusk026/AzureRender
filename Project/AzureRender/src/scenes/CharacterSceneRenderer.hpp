#include "assets/TextureMipChain.hpp"
#pragma once
#include "render/IrradianceVolume.hpp"
#include "render/EnvironmentCubeTexture.hpp"
#include "render/SampledTextureCache.hpp"
#include "render/OutlineProjectionResources.hpp"
#include "render/SceneMultisampling.hpp"

#include "extensions/ISceneRenderer.hpp"
#include "assets/GltfLoader.hpp"
#include "rhi/IGpuAllocator.hpp"
#include "rhi/Rhi.hpp"
#include "render/ClusteredLightGrid.hpp"
#include "render/LightBuffer.hpp"
#include "render/RenderSettings.hpp"
#include "render/GpuCullingResources.hpp"
#include "render/DeformedBounds.hpp"
#include "render/SceneInstanceSnapshot.hpp"
#include "render/ObjectShadowAtlas.hpp"
#include "render/SceneInstanceGpuData.hpp"
#include "render/SkinningBatch.hpp"
#include "render/MorphGpuData.hpp"
#include "render/TemporalViewState.hpp"
#include "render/MotionVectorPass.hpp"
#include "render/ShadowTileBounds.hpp"
#include "scene/Frustum.hpp"
#include "scene/RenderBatching.hpp"
#include "scene/SceneDescription.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <deque>
#include <functional>

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
    bool prepareLoad(const RenderContext& context,double budgetMs) override;
    bool reuseScene(const RenderContext& context) override;
    void onSwapchainRecreate(const RenderContext& context) override;
    void updateFrame(const SceneFrameData& frame) override;
    void onFrameSubmitted(std::uint64_t submission) noexcept override;
    void onFrameAbandoned() noexcept override;
    void recordScene(const RenderContext& context) override;
    void registerPasses(RenderGraph& graph, const SceneGraphResources& resources,
                        const RenderContext& context) override;
    void onUnload(const RenderContext& context) override;
    void appendHudText(std::ostringstream& text) const override;
    [[nodiscard]] const RendererSceneState* sceneState() const noexcept override;
    void onAnimationKey(int key, int action) override;
    void restartPlayback() override;
    void setPlaybackPlaying(bool playing) override;
    void appendCaptureManifestFields(std::ostream& json) const override;

private:
    TemporalViewState temporalView_;
    MotionPoseHistory motionPoses_;
    std::unique_ptr<MotionVectorPass> motionPass_;
    MotionVectorTarget* motionTarget_=nullptr;
    std::vector<MotionVectorDraw> motionDraws_;
    std::uint32_t motionPreviousValidObjects_=0;
    void initializeMotion(const RenderContext&);
    void prepareMotion();
    void prepareTemporalFrame(const SceneFrameData&);
    std::uint32_t materialFeaturesForFrame(const AssetMaterial&) const noexcept;
    bool temporalMaterialSamplesActive() const noexcept;
    void initializeHairIntegralPipelines(const RenderContext&,const rhi::GraphicsPipelineDesc&);
    void destroyHairIntegralPipelines() noexcept;
    void initializeOrdinaryOpaquePipelines(const RenderContext&,const rhi::GraphicsPipelineDesc&);
    void destroyOrdinaryOpaquePipelines() noexcept;
    std::array<VkPipeline,3> ordinaryOpaquePipelines_{};
    VkPipeline hairIntegralPipeline(const AssetMaterial&,bool mirrored,bool snapshot) const;
    std::array<VkPipeline,6> hairIntegralPipelines_{};
    std::array<VkPipeline,3> hairIntegralSnapshotPipelines_{};
    VkSampleCountFlagBits rasterizationSamples_=VK_SAMPLE_COUNT_1_BIT;
    std::array<float,2> samplingJitter_{};
    std::uint64_t temporalSceneGeneration_=1;
    bool faceSdfPrepared_=false,platformPrepared_=false;
    std::uint32_t faceSdfTextureCount_=0;
    struct GpuMaterial;
    void initializeLoad(const RenderContext& context);
    std::deque<std::function<void()>> loadJobs_;
    bool loadInitialized_=false;
    struct LoadTimings {
        double cpuPrepareMs=0,meshUploadMs=0,materialUploadMs=0,sharedTexturesMs=0;
        double buffersMs=0,skinningMs=0,descriptorsMs=0,pipelinesMs=0;
    } loadTimings_;
    std::uint64_t decodedMaterialPixelBytes_=0;
    void uploadMaterialTexture(LoadedAsset& asset,std::vector<GpuMaterial>& materials,std::size_t index,unsigned slot);
    static constexpr std::size_t kMaxFramesInFlight = 2;
    static constexpr std::uint32_t kClusterGridX = 16;
    static constexpr std::uint32_t kClusterGridY = 9;
    static constexpr std::uint32_t kClusterGridZ = 24;
    static constexpr std::uint32_t kMaxSceneLights = 128;
    static constexpr std::uint32_t kShadowCascadeCount = 4;
    static constexpr VkDeviceSize kMaxClusterLightIndices =
        static_cast<VkDeviceSize>(kClusterGridX)
        * kClusterGridY * kClusterGridZ * kMaxSceneLights;
    static constexpr std::uint32_t kEnvironmentMipLevels = 7;
    static constexpr std::uint32_t kIblPrefilterSamples = 32;
    // Indexed textures start with environment and toon ramp. Depth images
    // use an independent binding with a shared filtering sampler.
    static constexpr std::uint32_t kTextureBindingLayoutVersion = 1;
    static constexpr std::uint32_t kSharedTextureSlots = 2;
    static constexpr std::uint32_t kMaterialTextureSlots = 9;
    static constexpr std::uint32_t kMaterialColorAttachmentCount = 2;
    static constexpr std::uint32_t kAssetVertexWordCount = 27;

    struct GpuTexture {
        rhi::GpuImage image;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct IblPrefilterPushConstants {
        float roughness = 0.0F;
        std::uint32_t sampleCount = kIblPrefilterSamples;
    };
    static_assert(sizeof(IblPrefilterPushConstants) == 8);

    struct GpuMaterial {
        SampledTextureBinding baseColor;
        SampledTextureBinding normal;
        SampledTextureBinding metallicRoughness;
        SampledTextureBinding specularEmissive;
        SampledTextureBinding styleMask;
        SampledTextureBinding matcap;
        SampledTextureBinding hairData;
        SampledTextureBinding faceSdf;
        SampledTextureBinding shadowColorLut;
    };

    std::array<GpuTexture,kMaxFramesInFlight> overlaySceneDepth_;
    std::unique_ptr<SceneMultisampling> snapshotMultisampling_;
    VkRenderPass snapshotPass() const {return snapshotMultisampling_?snapshotMultisampling_->renderPass():overlayDepthPass_;}
    VkFramebuffer snapshotFramebuffer(std::uint32_t slot) const {return snapshotMultisampling_?snapshotMultisampling_->framebuffer(slot):overlayDepthFramebuffers_.at(slot);}
    std::array<GpuTexture,kMaxFramesInFlight> opaqueSceneColor_;
    std::array<rhi::GpuImage,kMaxFramesInFlight> overlayDepthAttachments_;
    std::array<VkImageView,kMaxFramesInFlight> overlayDepthViews_{};
    std::array<VkFramebuffer,kMaxFramesInFlight> overlayDepthFramebuffers_{};
    VkRenderPass overlayDepthPass_=VK_NULL_HANDLE;
    std::array<VkPipeline,3> snapshotDepthOnlyPipelines_{};
    std::array<VkPipeline,3> snapshotPrefillPipelines_{};
    std::array<VkPipeline,3> mainPrefillPipelines_{};
    VkPipeline overlayDepthPipeline_=VK_NULL_HANDLE;
    VkPipeline overlayDepthMirroredPipeline_=VK_NULL_HANDLE;
    VkPipeline overlayDepthDoubleSidedPipeline_=VK_NULL_HANDLE;
    std::array<GpuTexture,kMaxFramesInFlight> outlineMasks_;
    std::array<VkFramebuffer,kMaxFramesInFlight> outlineMaskFramebuffers_{};
    VkRenderPass outlineMaskPass_=VK_NULL_HANDLE;
    VkPipeline outlineMaskPipeline_=VK_NULL_HANDLE;
    VkPipeline outlineMaskMirroredPipeline_=VK_NULL_HANDLE;
    VkPipeline outlineMaskDoubleSidedPipeline_=VK_NULL_HANDLE;
    VkExtent2D outlineMaskViewExtent_{};
    std::uint32_t outlineMaskMaxDimension_=0;
    void prepareOutlineMaskResources(const std::vector<std::vector<float>>& opacities,RecordingBufferSet& buffers);
    void recordOutlineMask(const RenderContext& context,const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands);
    void createOverlayDepth(const RenderContext& context);
    void destroyOverlayDepth();
    void recordOverlayDepth(const RenderContext& context, const SceneInstanceSnapshot& snapshot, rhi::ICommandRecorder& commands);
    enum class DepthCoverageMode {SnapshotDepth,SnapshotColor,SnapshotPrefill,MainPrefill};
    void recordDepthCoverage(const RenderContext& context,const SceneInstanceSnapshot& snapshot,
        rhi::ICommandRecorder& commands,DepthCoverageMode mode,SceneSubmissionCounters* counters=nullptr);
    void recordOutlineShells(const RenderContext& context,const SceneInstanceSnapshot& snapshot,
        rhi::ICommandRecorder& commands,bool depthShell,SceneSubmissionCounters* counters=nullptr);
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
        // Bindless mode: the draw's first material slot in the global texture
        // array. Ignored by the fixed-table shaders.
        std::uint32_t textureBaseIndex = 0;
        float nodeOpacity = 1.0F;
        std::array<float, 16> gizmoTransform{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F,
        };
    };
    static_assert(sizeof(MorphPushConstants) == 80);
    static_assert(offsetof(MorphPushConstants, nodeOpacity) == 12);
    static_assert(offsetof(MorphPushConstants, gizmoTransform) == 16);

    struct UniformBufferObject {
        std::array<float, 4> cameraPosition{};
        std::array<float, 4> cameraForward{};
        std::array<float, 4> clusterGrid{};
        std::array<float, 4> clusterDepth{};
        std::array<float, 4> clusterLighting{};
        std::array<float, 4> cascadeSplits{};
        std::array<float, 4> renderingParameters{};
        std::array<float, 4> showcaseParameters{};
        std::array<float, 4> qaParameters{};
        std::array<float, 4> faceLightDirection{};
        std::array<float, 4> faceSdfParameters{};
        std::array<float, 4> faceSdfShadowColor{};
        std::array<float, 4> mainLightDirection{};
        std::array<float, 4> cascadeDepthRanges{};
        std::array<float, 4> skyRight{},skyUp{},skyForward{},skyColor{},skyParameters{};
        std::array<float, 4> probeCenter{},probeHalfExtents{},probeCapture{},probeParameters{};
        std::array<float,4> irradianceOriginSpacing{},irradianceDimensionsEnabled{},irradianceParameters{};
        // xyz: view basis; w: current shading jitter in output pixels, X/Y.
        std::array<float,4> cameraRight{},cameraUp{};
        std::array<float,16> screenReflectionWorldToClip{};
        std::array<float,4> screenReflectionTrace{},screenReflectionQuality{};
        std::array<float,4> nprDirectionBias{},nprGainFloor{},nprAmbientColor{},nprComposition{},nprMainRadiance{},nprLightingParameters{};
    };
    static_assert(sizeof(UniformBufferObject)==40*16);
    static_assert(offsetof(UniformBufferObject,nprDirectionBias)==34*16);
    static_assert(offsetof(UniformBufferObject,nprLightingParameters)==39*16);
    static_assert(offsetof(UniformBufferObject,screenReflectionWorldToClip)==28*16);
    static_assert(offsetof(UniformBufferObject,screenReflectionTrace)==32*16);
    static_assert(offsetof(UniformBufferObject,screenReflectionQuality)==33*16);
    static_assert(offsetof(UniformBufferObject,cameraRight)==26*16);
    static_assert(offsetof(UniformBufferObject,cameraUp)==27*16);

    // Per-instance transforms uploaded to the instance storage buffer. The
    // host computes them with the same matrix helpers as the single-model
    // path, so one instance reproduces the original values bit for bit.
    using InstanceGpuData=SceneInstanceGpuData;
    static_assert(sizeof(InstanceGpuData) == 496);

    using SkinningPushConstants = SkinningBatch;

    // A scene-referenced asset beyond the hero. Renders at bind pose in the
    // current stage; its joints and material textures append after the hero
    // data in the shared buffers.
    struct AdditionalResource {
        std::string path;
        LoadedAsset asset;
        rhi::GpuBuffer vertexBuffer;
        rhi::GpuBuffer indexBuffer;
        std::vector<GpuMaterial> gpuMaterials;
        std::vector<rhi::GpuBuffer> skinnedVertexBuffers;
        std::uint32_t jointBase = 0;
        std::uint32_t textureBase = 0;
        std::size_t globalMaterialBase = 0;
    };

    // Engine-owned allocator borrowed for the renderer's lifetime.
    rhi::IGpuAllocator* allocator_ = nullptr;
    // Resource creation and recording backend borrowed for the lifetime.
    rhi::IRhi* rhi_ = nullptr;
    // Global texture array path, enabled when the device offers descriptor
    // indexing. False keeps the per-material fixed descriptor tables.
    bool bindlessTextures_ = false;
    bool computeIblEnabled_ = false;
    bool computeSkinningEnabled_ = false;
    std::string shaderDirectory_;
    std::string rampAtlasPath_;
    SceneEnvironmentSource environmentSource_;
    const RenderSettings* renderSettings_ = nullptr;
    RenderSettings frameRenderSettings_;
    std::array<std::unique_ptr<GpuCullingResources>, kMaxFramesInFlight> gpuCullingFrames_;
    std::array<std::size_t, kMaxFramesInFlight> gpuCullingCapacities_{};
    std::vector<std::size_t> indirectInstanceOffsets_;
    std::vector<std::array<std::uint32_t, 3>> opaqueRecordingSpans_;
    std::vector<std::size_t> transparentIndexOffsets_;
    bool transparentDrawsPrepared_ = false;
    std::vector<std::vector<const AssetPrimitive*>> transparentPrimitivesByMesh_;
    std::shared_ptr<const SceneInstanceSnapshot> instanceSnapshot_;
    std::shared_ptr<const MaterialOriginInstances> materialOriginInstances_;
    std::vector<std::vector<std::uint32_t>> materialOriginNodes_;
    std::uint32_t materialOriginFirstInstance(const AssetMaterial& material,
        const SceneInstanceSnapshot* snapshot,std::uint32_t first,std::uint32_t count) const;
    void prepareTransparentIndices();
    bool gpuCullingEnabled_ = false;
    bool visibilityPrototype_ = false;
    bool multiDrawIndirect_ = false;
    std::uint32_t maxDrawIndirectCount_ = 1;
    // Non-owning per-frame submission counters, refreshed from RenderContext at
    // the start of every recordScene call. Null when collection is disabled.
    // Engine-owned shadow map sampled by the material descriptor sets.
    VkImageView shadowImageView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;
    std::array<std::unique_ptr<ShadowTileBounds>,kMaxFramesInFlight> shadowBounds_;
    std::unique_ptr<ObjectShadowAtlas> objectShadowAtlas_;
    std::array<VkImageView,kMaxFramesInFlight> objectShadowBoundViews_{};
    bool objectShadowPrepared_=false;
    unsigned objectShadowMaxDimension_=0;
    std::vector<ObjectShadowPage> objectShadowPages_;

    LoadedAsset asset_;
    std::vector<JointBounds> jointBounds_;
    std::optional<std::uint32_t> faceSdfHeadNode_;
    // Scene content snapshot from onLoad; nodes drive instance building.
    scene::SceneDescription scene_;
    bool sceneWorldCoordinates_ = false;
    std::vector<std::unique_ptr<AdditionalResource>> additionalResources_;
    // Normalized bind-pose head basis. Imported joint axes are not guaranteed
    // to match the model's semantic left/up/forward axes, so runtime lighting
    // uses the current head rotation relative to this reference basis.
    std::array<float, 9> faceSdfBindBasis_{
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 1.0F,
    };
    rhi::GpuBuffer vertexBuffer_;
    rhi::GpuBuffer indexBuffer_;
    std::vector<GpuMaterial> gpuMaterials_;
    std::unique_ptr<SampledTextureCache> materialTextureCache_;
    std::unique_ptr<OutlineProjectionResources> outlineProjection_;
    bool outlineProjectionEnabled_=false;
    GpuTexture environmentTexture_;
    std::unique_ptr<EnvironmentCubeTexture> reflectionProbeTexture_;
    IrradianceVolume irradianceVolume_;
    rhi::GpuBuffer irradianceBuffer_;
    VkDeviceSize irradianceBufferBudget_=0;
    GpuTexture toonRampTexture_;
    VkImageView environmentBaseMipView_ = VK_NULL_HANDLE;
    std::vector<VkImageView> environmentPrefilterViews_;
    VkDescriptorSetLayout iblPrefilterSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool iblPrefilterPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> iblPrefilterSets_;
    VkPipelineLayout iblPrefilterPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline iblPrefilterPipeline_ = VK_NULL_HANDLE;
    std::vector<rhi::GpuBuffer> uniformBuffers_;
    // Immutable material table. Owned from load through retirement at unload.
    rhi::GpuBuffer materialSurfaceProfiles_;
    std::vector<rhi::GpuBuffer> jointBuffers_;
    rhi::GpuBuffer morphTargetBuffer_;
    std::array<rhi::GpuBuffer,kMaxFramesInFlight> morphWeightBuffers_{};
    std::vector<MorphGpuMesh> morphGpuMeshes_;
    std::vector<rhi::GpuBuffer> oitIndexBuffers_;
    std::vector<rhi::GpuBuffer> instanceBuffers_;
    std::vector<rhi::GpuBuffer> lightBuffers_;
    std::vector<rhi::GpuBuffer> clusterHeaderBuffers_;
    std::vector<rhi::GpuBuffer> clusterIndexBuffers_;
    LightBuffer frameLights_;
    std::vector<RenderLightGpu> packedLights_;
    std::vector<std::array<std::uint32_t, 2>> packedClusterHeaders_;
    std::vector<std::uint32_t> packedClusterIndices_;
    bool clusteredLightingReported_ = false;
    std::uint32_t instanceCapacity_ = 0;
    std::uint32_t qaInstanceCount_ = 1;
    std::vector<rhi::GpuBuffer> skinnedVertexBuffers_;
    VkDescriptorSetLayout skinningDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool skinningDescriptorPool_ = VK_NULL_HANDLE;
    VkPipelineLayout skinningPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline skinningPipeline_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> skinningDescriptorSets_;
    std::uint32_t meshResourceCount_ = 0;
    std::uint32_t shadowCascadeResolution_ = 1024;
    std::size_t oitIndexBufferSize_ = 0;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline opaquePipeline_ = VK_NULL_HANDLE;
    VkPipeline opaqueMirroredPipeline_ = VK_NULL_HANDLE;
    VkPipeline opaqueDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendMirroredPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline multiplyPipeline_ = VK_NULL_HANDLE;
    VkPipeline multiplyMirroredPipeline_ = VK_NULL_HANDLE;
    VkPipeline multiplyDoubleSidedPipeline_ = VK_NULL_HANDLE;
    VkPipeline sceneTintPipeline_ = VK_NULL_HANDLE;
    VkPipeline outlinePipeline_ = VK_NULL_HANDLE;
    VkPipeline outlineDepthPipeline_=VK_NULL_HANDLE;
    VkPipeline outlineDepthMirroredPipeline_=VK_NULL_HANDLE;
    VkPipeline outlineMirroredPipeline_ = VK_NULL_HANDLE;
    VkPipeline backgroundPipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline objectShadowPipeline_ = VK_NULL_HANDLE;

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
    // Frame camera and light matrices, kept so rebuildSceneInstances can
    // compose per-instance matrices with identical operation order.
    std::array<float, 16> viewMatrix_{};
    std::array<float, 16> projectionMatrix_{};
    std::array<float, 16> samplingProjectionMatrix_{};
    std::array<std::array<float, 16>, kShadowCascadeCount>
        cascadeLightViewProjections_{};
    std::array<float, kShadowCascadeCount> cascadeSplits_{};
    // Bind-pose contact pivot estimated from the lowest character vertices.
    // X/Z define both the turntable axis and showcase-platform centre.
    std::array<float, 3> footPivot_{0.0F, 0.0F, 0.0F};
    // Per-frame scene instances and the frustum-culled visible subset. The
    // default asset scene contributes one instance; the draw loops consume
    // the visible list.
    std::vector<scene::SceneInstance> sceneInstances_;
    std::vector<const scene::SceneInstance*> visibleInstances_;
    struct InstancePose {
        AssetPose pose;
        std::uint32_t meshKey = 0, jointBase = 0, vertexBase = 0;
        std::array<float, 2> morph{};
        std::uint32_t morphWeightBase=0;
    };
    std::vector<InstancePose> instancePoses_;
    struct SkinningDispatch { std::uint32_t meshKey; SkinningPushConstants parameters; };
    std::vector<SkinningDispatch> skinningDispatches_;
    std::vector<NodeAnimationFrame> animationFrames_;
    std::uint32_t jointCapacity_ = 0, vertexCapacity_ = 0;
    void ensureInstanceCapacity();
    internal::Vector3 frameMainLight_{0,1,0};
    void prepareInstancePoses();
    std::array<float,4> instanceFaceLight(const LoadedAsset& mesh, const AssetPose& pose, const internal::Matrix4& model) const;
    internal::Matrix4 instanceFaceFrame(const LoadedAsset& mesh,const AssetPose& pose,const internal::Matrix4& model) const;
    // Per-resource visible span: (meshKey, firstSlot, count) in the
    // instance buffer's visible order.
    std::vector<std::array<std::uint32_t, 3>> visibleSpansByMeshKey_;
    std::vector<std::array<std::uint32_t, 3>> visibleShadowSpansByMeshKey_;
    scene::FrustumPlanes viewFrustum_;
    std::array<scene::FrustumPlanes, kShadowCascadeCount>
        shadowCascadeFrusta_{};
    bool cullingEnabled_ = true;
    RendererSceneState state_;

    // Resource creation.
    void createVertexBuffer();
    void createIndexBuffer();
    void createTexture(bool uploadMaterials=true);
    void createEnvironmentPrefilter();
    void createAdditionalResources();
    void createComputeSkinningResources();
    void recordComputeSkinning(const RenderContext& context);
    void recordComputeSkinningMesh(
        const RenderContext& context,
        std::uint32_t meshKey, rhi::ICommandRecorder* recorder = nullptr,
        const std::vector<SkinningDispatch>* dispatches = nullptr);
    [[nodiscard]] const rhi::GpuBuffer& renderVertexBuffer(
        std::uint32_t meshKey,
        std::uint32_t frameIndex) const;
    [[nodiscard]] const rhi::GpuBuffer& renderIndexBuffer(
        std::uint32_t meshKey) const;
    void createMeshBuffers(
        LoadedAsset& asset,
        rhi::GpuBuffer& vertexBuffer,
        rhi::GpuBuffer& indexBuffer);
    template <typename Pixels>
    void uploadTextureData(
        const Pixels& pixels,
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        bool clampVertical,
        GpuTexture& texture,
        std::uint32_t mipLevels = 1,TextureFilterSemantic semantic=TextureFilterSemantic::Linear,float alphaCutoff=0,const TextureImportSettings* importSettings=nullptr);
    void uploadMaterialTextures(
        LoadedAsset& asset,
        std::vector<GpuMaterial>& gpuMaterials);
    void createUniformBuffers();
    void createJointBuffers();
    void createOitIndexBuffers();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    [[nodiscard]] std::size_t totalMaterialCount() const noexcept {
        std::size_t total = asset_.materials.size();
        for (const auto& resource : additionalResources_) {
            total += resource->asset.materials.size();
        }
        return total;
    }
    void createGraphicsPipeline(const RenderContext& context);
    void destroyResources();

    // Frame recording.
    void updateUniformBuffer(const SceneFrameData& frame);
    void updateClusteredLighting(
        const azurerender::internal::Matrix4& view,
        const azurerender::internal::Matrix4& projection,
        std::uint32_t width,
        std::uint32_t height);
    void rebuildSceneInstances();
    void recordShadowPass(const RenderContext& context);
    void prepareObjectShadows();
    void recordObjectShadows(const RenderContext& context,const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands);
    void recordShadowDraws(const RenderContext& context,
        std::uint32_t firstCascade = 0, std::uint32_t cascadeCount = kShadowCascadeCount,
        const SceneInstanceSnapshot* snapshot = nullptr,
        rhi::ICommandRecorder* recordingCommands = nullptr,
        const ObjectShadowPage* objectPage = nullptr);
    void recordMainPass(const RenderContext& context);
    void recordMainDraws(const RenderContext& context, std::uint32_t stage = 0,
        const SceneInstanceSnapshot* snapshot = nullptr,
        std::size_t firstTransparent = 0, std::size_t transparentCount = static_cast<std::size_t>(-1),
        rhi::ICommandRecorder* recordingCommands = nullptr, SceneSubmissionCounters* recordingCounters = nullptr);
    rhi::RenderPassBeginDesc mainPassDescription(const RenderContext& context) const;
    void drawPrimitive(
        rhi::ICommandRecorder& commands,
        const LoadedAsset& mesh,
        const AssetPrimitive& primitive,
        const std::uint32_t firstIndexOffset,
        std::uint32_t instanceCount = 1,
        std::uint32_t firstInstance = 0,
        std::uint32_t textureBase = 0,
        std::size_t globalMaterialBase = 0,
        SceneSubmissionCounters* counters = nullptr,
        const SceneInstanceSnapshot* snapshot = nullptr);
    void buildSceneState();
    void destroyGraphicsPipelinesForRecreate();
};

}  // namespace azurerender
