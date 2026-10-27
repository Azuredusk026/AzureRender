#include "animation/PresentedPose.hpp"
#include "assets/AssetDeformation.hpp"
#include "animation/ExpressionPose.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "render/MaterialSampling.hpp"
#include "render/MaterialPipelinePolicy.hpp"
#include "render/IrradianceClipmapGpu.hpp"
#include "render/IrradianceResource.hpp"
#include "scene/DirectionalLightSelection.hpp"
#include "assets/MaterialData.hpp"
#include "assets/MaterialSurfaceProfile.hpp"
#include "assets/MaterialFrame.hpp"
#include "render/ResourceAccessProfile.hpp"
#include "render/ResourceIndexTable.hpp"
#include "render/PrimitiveOpacity.hpp"
#include "render/OutlineMaskLayout.hpp"

#include "render/RenderMath.hpp"
#include "resources/BinaryFile.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "render/RenderSettings.hpp"
#include "render/EnvironmentAsset.hpp"
#include "render/ComputePass.hpp"
#include "render/CascadedShadow.hpp"
#include "scene/TransformParity.hpp"
#include "render/CameraProjection.hpp"

#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

using namespace azurerender::internal;

namespace azurerender {
CharacterSceneRenderer::CharacterSceneRenderer() = default;
CharacterSceneRenderer::~CharacterSceneRenderer() = default;

namespace {

std::vector<std::uint32_t> materialOriginNodes(const LoadedAsset& mesh) {
    std::vector<std::uint32_t> result;
    for(const auto& material:mesh.materials)if(material.nprDiffuse.rimOrigin.present) {
        const auto node=material.nprDiffuse.rimOrigin.node;
        if(node>=mesh.nodes.size())throw std::invalid_argument("Material rim origin node is outside the model");
        result.push_back(node);
    }
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());
    return result;
}

std::uint64_t stableSceneLightId(std::string_view id){
    std::uint64_t result=1469598103934665603ULL;
    for(const unsigned char value:id){result^=value;result*=1099511628211ULL;}
    return result;
}

static_assert(sizeof(AssetVertex) == 27 * sizeof(std::uint32_t));

Matrix4 primitiveGizmoTransform(const RecordingGizmoState& state,std::size_t primitive) {
    if(!state.active||state.selectedPrimitive!=static_cast<std::int32_t>(primitive))
        return scale(1.F,1.F,1.F);
    constexpr float radians=3.14159265358979323846F/180.F;
    return multiply(translation(state.translation[0],state.translation[1],state.translation[2]),
        multiply(multiply(rotationX(state.rotation[0]*radians),rotationY(state.rotation[1]*radians)),
            multiply(rotationZ(state.rotation[2]*radians),scale(state.scale[0],state.scale[1],state.scale[2]))));
}

bool primitiveMirroredTransform(const Matrix4& model,const RecordingGizmoState& gizmo,std::size_t primitive) {
    const bool gizmoMirrored=gizmo.active&&gizmo.selectedPrimitive==static_cast<std::int32_t>(primitive)
        &&gizmo.scale[0]*gizmo.scale[1]*gizmo.scale[2]<0;
    return scene::mirroredTransform(model)!=gizmoMirrored;
}

std::uint64_t materialSourcePixelBytes(const LoadedAsset& asset) {
    std::uint64_t bytes=0;
    for(const auto& m:asset.materials)
        for(const auto* pixels:{&m.baseColorPixels,&m.normalPixels,&m.metallicRoughnessPixels,
                &m.specularEmissivePixels,&m.styleMaskPixels,&m.matcapPixels,&m.hairDataPixels,
                &m.faceSdf.pixels,&m.shadowColorLutPixels,&m.projectedHairRamp.parameterPixels,&m.projectedHairRamp.line.pixels})bytes+=pixels->size();
    for(const auto& material:asset.materials)if(material.outlineSurface.present)
        bytes+=material.outlineSurface.maskPixels.size();
    return bytes;
}

Vector3 estimateFootPivot(const LoadedAsset& asset) {
    const float height = asset.boundsMax[1] - asset.boundsMin[1];
    const float footBand = asset.boundsMin[1] + height * 0.12F;
    std::vector<float> xCoordinates;
    std::vector<float> zCoordinates;
    xCoordinates.reserve(asset.vertices.size() / 16);
    zCoordinates.reserve(asset.vertices.size() / 16);
    for (const AssetVertex& vertex : asset.vertices) {
        if (vertex.position[1] <= footBand) {
            xCoordinates.push_back(vertex.position[0]);
            zCoordinates.push_back(vertex.position[2]);
        }
    }
    if (xCoordinates.empty()) {
        return {
            (asset.boundsMin[0] + asset.boundsMax[0]) * 0.5F,
            asset.boundsMin[1],
            (asset.boundsMin[2] + asset.boundsMax[2]) * 0.5F,
        };
    }
    const auto median = [](std::vector<float>& values) {
        const std::size_t middle = values.size() / 2;
        std::nth_element(values.begin(), values.begin() + middle, values.end());
        return values[middle];
    };
    return {median(xCoordinates), asset.boundsMin[1], median(zCoordinates)};
}

void appendShowcasePlatform(LoadedAsset& asset, const Vector3& footPivot) {
    constexpr std::uint32_t kSegments = 96;
    const Vector3 boundsCenter = {
        footPivot[0],
        (asset.boundsMin[1] + asset.boundsMax[1]) * 0.5F,
        footPivot[2],
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

    if(!asset.texcoord1.empty())asset.texcoord1.resize(asset.vertices.size(),{0,0});
    asset.primitives.push_back({
        firstIndex,
        static_cast<std::uint32_t>(asset.indices.size()) - firstIndex,
        materialIndex,
        {boundsCenter[0], (topY + bottomY) * 0.5F, boundsCenter[2]},
    });
}

constexpr std::array<const char*, 10> kMaterialClassShortNames = {
    "GENERIC", "SKIN", "FACE", "HAIR", "FABRIC", "METAL",
    "EYE", "OVERLAY", "SHOWCASE", "OTHER",
};

}  // namespace

SceneRendererCapabilities CharacterSceneRenderer::capabilities() const {
    SceneRendererCapabilities caps;
    caps.requiresSceneDepth = true;
    caps.requiresSceneNormal = true;
    caps.supportsProjectionOffset=true;
    caps.supportsMotionVectors=true;
    caps.supportsMultisampleShading=true;
    caps.diagnosticViewNames = {
        "Beauty",
        "World Normal",
        "Internal Outline",
        "Shadow Map",
        "Depth",
    };
    return caps;
}

void CharacterSceneRenderer::initializeLoad(const RenderContext& context) {
    const auto prepareStart=std::chrono::steady_clock::now();
    loadTimings_={};decodedMaterialPixelBytes_=0;
    shadowCascadeResolution_=context.shadowMapSize/2;
    allocator_ = context.allocator;
    if (allocator_ == nullptr) {
        throw std::runtime_error(
            "RenderContext must carry the engine GPU allocator");
    }
    rhi_ = context.rhi;
    if (rhi_ == nullptr) {
        throw std::runtime_error("RenderContext must carry the engine RHI");
    }
    materialTextureCache_=std::make_unique<SampledTextureCache>(*allocator_,*rhi_);
    bindlessTextures_ = context.bindlessTextures;
    cullingEnabled_ = context.cullingEnabled;
    computeIblEnabled_ = context.rgba16fStorageImage;
    computeSkinningEnabled_ = context.computeSkinning;
    shaderDirectory_ = context.shaderDirectory;
    qaInstanceCount_ = std::max(context.qaInstanceCount, 1U);
    renderSettings_ = context.renderSettings;
    faceSdfPrepared_=renderSettings_->faceSdfFeature;platformPrepared_=renderSettings_->showcasePlatformFeature;
    faceSdfTextureCount_=0;
    gpuCullingEnabled_ = context.gpuCulling;
    visibilityPrototype_ = context.visibilityPrototype && context.gpuCulling;
    multiDrawIndirect_ = context.multiDrawIndirect;
    maxDrawIndirectCount_ = std::max(1U, context.maxDrawIndirectCount);
    rampAtlasPath_ = context.rampAtlasPath;
    environmentSource_ = context.environment;
    shadowImageView_ = context.shadowImageView;
    shadowSampler_ = context.shadowSampler;

    // Load and validate the glTF asset (mirrors the former application init).
    if (context.scene.resources.empty()) {
        throw std::runtime_error("Scene must reference at least one asset");
    }
    scene_ = context.scene;
    sceneWorldCoordinates_ = context.sceneWorldCoordinates;
    const std::string resolvedAssetPath = context.scene.resources[0].path;
    asset_ = context.preparedMeshes && context.preparedMeshes->count(std::filesystem::u8path(resolvedAssetPath).lexically_normal().generic_u8string())
        ? *context.preparedMeshes->at(std::filesystem::u8path(resolvedAssetPath).lexically_normal().generic_u8string()) : loadGltfAsset(resolvedAssetPath);
    for (const AssetMaterial& material : asset_.materials) {
        if (!faceSdfPrepared_||!material.faceSdf.present) {
            continue;
        }
        if (faceSdfHeadNode_.has_value()
            && *faceSdfHeadNode_ != material.faceSdf.headNode) {
            throw std::runtime_error(
                "Face SDF materials must share one headNode");
        }
        faceSdfHeadNode_ = material.faceSdf.headNode;
        azurerender::RuntimeDiagnostics::instance().print(
            "asset",
            "Face SDF: material=" + material.name
                + ", texture=" + std::to_string(material.faceSdf.width) + 'x'
                + std::to_string(material.faceSdf.height)
                + ", headNode=" + material.faceSdf.headNodeName
                + ", schemaVersion=" + std::to_string(material.faceSdf.schemaVersion));
    }
    if (faceSdfHeadNode_.has_value()
        && *faceSdfHeadNode_ < asset_.nodeWorldMatrices.size()) {
        const auto& head = asset_.nodeWorldMatrices[*faceSdfHeadNode_];
        const Vector3 bindX = normalize({head[0], head[1], head[2]});
        const Vector3 bindY = normalize({head[4], head[5], head[6]});
        const Vector3 bindZ = normalize({head[8], head[9], head[10]});
        faceSdfBindBasis_ = {
            bindX[0], bindX[1], bindX[2],
            bindY[0], bindY[1], bindY[2],
            bindZ[0], bindZ[1], bindZ[2],
        };
        azurerender::RuntimeDiagnostics::instance().print(
            "asset", "Face SDF light frame: evaluated per instance");
    }
    footPivot_ = estimateFootPivot(asset_);
    if(platformPrepared_)appendShowcasePlatform(asset_, footPivot_);
    azurerender::RuntimeDiagnostics::instance().print(
        "asset",
        "Turntable foot pivot: [" + std::to_string(footPivot_[0]) + ", "
            + std::to_string(footPivot_[1]) + ", "
            + std::to_string(footPivot_[2]) + "]");
    azurerender::RuntimeDiagnostics::instance().print(
        "asset", "Asset path: " + resolvedAssetPath);
    azurerender::RuntimeDiagnostics::instance().print(
        "asset",
        "Loaded asset: " + std::to_string(asset_.vertices.size())
            + " vertices, " + std::to_string(asset_.indices.size())
            + " indices, " + std::to_string(asset_.primitives.size())
            + " primitives, " + std::to_string(asset_.materials.size())
            + " materials");
    azurerender::RuntimeDiagnostics::instance().print(
        "asset", "Material Class v1 inventory:");
    for (std::size_t index = 0; index < asset_.materials.size(); ++index) {
        const AssetMaterial& material = asset_.materials[index];
        std::stringstream inventory;
        inventory << "  [" << index << "] " << material.name
                  << " -> " << assetMaterialClassName(material.materialClass)
                  << ", flags=0x" << std::hex << material.materialFeatures
                  << std::dec
                  << (material.materialProfileExplicit
                      ? ", source=asset-extras"
                      : ", source=fallback/inferred");
        azurerender::RuntimeDiagnostics::instance().print(
            "asset", inventory.str());
    }
    std::stringstream skinning;
    skinning << "Skinning: "
             << (asset_.hasSkin ? "enabled" : "static fallback")
             << ", " << asset_.jointMatrices.size() << " joint matrices"
             << ", " << asset_.morphTargetCount << " Morph targets";
    azurerender::RuntimeDiagnostics::instance().print("asset", skinning.str());
    azurerender::RuntimeDiagnostics::instance().print(
        "asset",
        "Morph weights: "
            + std::to_string(renderSettings_->morphWeights[0]) + ", "
            + std::to_string(renderSettings_->morphWeights[1]));
    std::string animationLine = "Animations: "
        + std::to_string(asset_.animations.size());
    if (!asset_.animations.empty()) {
        const auto& animation = asset_.animations.front();
        animationLine += " (playing \"" + animation.name + "\", "
            + std::to_string(animation.endTime - animation.startTime)
            + " s loop)";
    }
    azurerender::RuntimeDiagnostics::instance().print(
        "asset", animationLine);
    animationIndex_ = 0;
    animationTime_ = 0.0F;
    animationPlaying_ = true;

    const scene::ResourceLayout layout(scene_);
    applyMaterialBindings(asset_,layout.materials.at(0));
    jointBounds_.clear();jointBounds_.push_back(JointBounds::build(asset_));
    const auto enqueue=[this](std::string label,double& elapsed,std::function<void()> action,bool alwaysReport=false) {
        loadJobs_.push_back([label=std::move(label),&elapsed,action=std::move(action),alwaysReport] {
            const auto start=std::chrono::steady_clock::now();action();
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            elapsed+=ms;
            if(alwaysReport||ms>=1000)RuntimeDiagnostics::instance().info("render","Scene load "+label+": "+std::to_string(ms)+" ms");
        });
    };
    enqueue("mesh 0",loadTimings_.meshUploadMs,[this]{createVertexBuffer();});
    const auto materials=[this,&enqueue](LoadedAsset& mesh,std::vector<GpuMaterial>& gpu,std::size_t key){
        gpu.resize(mesh.materials.size());
        for(std::size_t index=0;index<mesh.materials.size();++index)for(unsigned slot=0;slot<kMaterialTextureSlots;++slot)
            enqueue("material resource="+std::to_string(key)+" material="+std::to_string(index)+" slot="+std::to_string(slot),
                loadTimings_.materialUploadMs,[this,&mesh,&gpu,index,slot]{uploadMaterialTexture(mesh,gpu,index,slot);});
    };
    materials(asset_,gpuMaterials_,0);
    decodedMaterialPixelBytes_=materialSourcePixelBytes(asset_);
    enqueue("shared textures",loadTimings_.sharedTexturesMs,[this]{createTexture(false);},true);
    std::uint32_t textureBase=kSharedTextureSlots+kMaterialTextureSlots*static_cast<std::uint32_t>(asset_.materials.size());
    std::size_t materialBase=asset_.materials.size();
    for(std::size_t key=1;key<layout.paths.size();++key){
        auto resource=std::make_unique<AdditionalResource>();resource->path=layout.paths[key];
        resource->asset=context.preparedMeshes && context.preparedMeshes->count(resource->path)
            ? *context.preparedMeshes->at(resource->path):loadGltfAsset(resource->path);
        applyMaterialBindings(resource->asset,layout.materials.at(key));
        resource->textureBase=textureBase;resource->globalMaterialBase=materialBase;
        textureBase+=kMaterialTextureSlots*static_cast<std::uint32_t>(resource->asset.materials.size());materialBase+=resource->asset.materials.size();
        auto* item=resource.get();jointBounds_.push_back(JointBounds::build(item->asset));
        decodedMaterialPixelBytes_+=materialSourcePixelBytes(item->asset);
        enqueue("mesh "+std::to_string(key),loadTimings_.meshUploadMs,[this,item]{createMeshBuffers(item->asset,item->vertexBuffer,item->indexBuffer);});
        materials(item->asset,item->gpuMaterials,key);additionalResources_.push_back(std::move(resource));
        if(key%16==0||key+1==layout.paths.size())loadJobs_.push_back([this,key] {
            const auto& stats=materialTextureCache_->statistics();
            RuntimeDiagnostics::instance().info("render","Scene material progress: resource="+std::to_string(key)
                +", elapsedMs="+std::to_string(loadTimings_.materialUploadMs)+", hashMs="+std::to_string(stats.hashMs)
                +", mipMs="+std::to_string(stats.mipMs)+", uploadMs="+std::to_string(stats.uploadMs)
                +", transferredBytes="+std::to_string(stats.transferredPixelBytes)+", discardedBytes="+std::to_string(stats.discardedPixelBytes));
        });
    }
    const auto hasProjection=[](const LoadedAsset& mesh) {
        return std::any_of(mesh.materials.begin(),mesh.materials.end(),[](const AssetMaterial& material){return material.outlineSurface.present;});
    };
    outlineProjectionEnabled_=hasProjection(asset_);
    for(const auto& resource:additionalResources_)outlineProjectionEnabled_|=hasProjection(resource->asset);
    if(outlineProjectionEnabled_)enqueue("outline projection",loadTimings_.materialUploadMs,[this,context] {
        std::vector<LoadedAsset*> meshes{&asset_};
        for(auto& resource:additionalResources_)meshes.push_back(&resource->asset);
        const auto data=buildOutlineProjectionData(meshes,context.descriptorLimits.maxStorageBufferRange);
        outlineProjection_=std::make_unique<OutlineProjectionResources>(*allocator_,context.descriptorLimits.maxStorageBufferRange);
        outlineProjection_->initialize(data,*materialTextureCache_);
    });
    // The full material inventory must fit an ordinary descriptor layout.
    // Combined image samplers consume both sampler and sampled-image limits;
    // The fragment stage also carries one uniform and six storage buffers.
    const std::uint64_t indexedSlots = kSharedTextureSlots
        + std::uint64_t{kMaterialTextureSlots} * totalMaterialCount();
    const auto& limits = context.descriptorLimits;
    if((limits.maxPerStageDescriptorStorageBuffers&&limits.maxPerStageDescriptorStorageBuffers<6)
        ||(limits.maxDescriptorSetStorageBuffers&&limits.maxDescriptorSetStorageBuffers<(outlineProjectionEnabled_?12U:11U)))
        throw std::runtime_error("Scene storage buffers cannot fit the selected material layout");
    irradianceBufferBudget_=limits.maxStorageBufferRange;
    DeviceCapabilities capabilities;
    capabilities.descriptorIndexingSupported=context.bindlessTextures;
    capabilities.descriptorIndexingEnabled=context.bindlessTextures;
    capabilities.limits=limits;
    irradianceClipmapActive_=!scene_.irradianceVolume.asset.empty()
        &&readIrradianceResourceKind(std::filesystem::u8path(scene_.irradianceVolume.asset))==IrradianceResourceKind::Clipmap;
    const std::uint32_t irradianceSlots=irradianceClipmapActive_?6U:0U;
    ResourceAccessRequirements requirements{indexedSlots+5+irradianceSlots,16+irradianceSlots,7};
    requirements.separateImageSlots=requirements.fixedSeparateImageSlots=1;
    requirements.separateSamplerSlots=requirements.fixedSeparateSamplerSlots=1;
    requirements.fragmentColorAttachments=kMaterialColorAttachmentCount;
    requirements.vertexCombinedImageSamplers=outlineProjectionEnabled_?totalMaterialCount():0;
    requirements.fixedVertexCombinedImageSamplers=outlineProjectionEnabled_?1:0;
    requirements.vertexBuffers=outlineProjectionEnabled_?7:6;
    requirements.descriptorSetBuffers=outlineProjectionEnabled_?13:12;
    auto access=ResourceAccessProfile::select(capabilities,requirements);
    auto objectRequirements=requirements;
    ++objectRequirements.sampledImageSlots;++objectRequirements.fixedSampledImageSlots;
    ++objectRequirements.separateImageSlots;++objectRequirements.fixedSeparateImageSlots;
    const auto objectAccess=ResourceAccessProfile::select(capabilities,objectRequirements);
    const bool objectRequested=std::any_of(scene_.nodes.begin(),scene_.nodes.end(),[](const auto& node){return node.objectShadow&&node.objectShadow->enabled;});
    objectShadowMaxDimension_=limits.maxImageDimension2D;
    const bool objectFormat=rhi_->supportsSampledImage(VK_FORMAT_D16_UNORM,true)
        &&(!objectShadowMaxDimension_||objectShadowMaxDimension_>=ObjectShadowAtlas::kWidth);
    if(objectRequested){
        if(!objectFormat||objectAccess.mode==ResourceAccessMode::Unsupported)
            throw std::runtime_error("Object shadows require sampled D16, a 4096 atlas and one additional sampled-image slot: "+objectAccess.diagnostic);
        access=objectAccess;
    }
    objectShadowPrepared_=objectFormat&&objectAccess.mode!=ResourceAccessMode::Unsupported&&objectAccess.mode==access.mode;
    if(access.mode==ResourceAccessMode::Unsupported) throw std::runtime_error(access.diagnostic);
    bindlessTextures_ = access.mode==ResourceAccessMode::Indexed;
    azurerender::RuntimeDiagnostics::instance().info(
        "render", "Character texture access: "
            + std::string(bindlessTextures_ ? "indexed" : "fixed")
            + ", requested slots=" + std::to_string(indexedSlots)
            + ", stage sampler limit="
            + std::to_string(limits.maxPerStageDescriptorSamplers));
    enqueue("buffers",loadTimings_.buffersMs,[this]{createUniformBuffers();createJointBuffers();},true);
    enqueue("skinning",loadTimings_.skinningMs,[this]{createComputeSkinningResources();createOitIndexBuffers();},true);
    enqueue("descriptors",loadTimings_.descriptorsMs,[this,context]{createOverlayDepth(context);if(objectShadowPrepared_){objectShadowAtlas_=std::make_unique<ObjectShadowAtlas>(*rhi_);objectShadowAtlas_->initialize();}for(auto& bounds:shadowBounds_){bounds=std::make_unique<ShadowTileBounds>(*rhi_);bounds->initialize(readBinaryFile(shaderDirectory_+"/shadow_bounds.comp.spv"),shadowImageView_,shadowSampler_,context.shadowMapSize,objectShadowPrepared_?kObjectShadowCapacity:0);}createDescriptorSetLayout();createDescriptorPool();createDescriptorSets();},true);
    enqueue("pipelines",loadTimings_.pipelinesMs,[this,context]{createGraphicsPipeline(context);initializeMotion(context);buildSceneState();},true);
    loadTimings_.cpuPrepareMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-prepareStart).count();
    RuntimeDiagnostics::instance().info("render","Scene load CPU preparation: "+std::to_string(loadTimings_.cpuPrepareMs)
        +" ms, materialPixelBytes="+std::to_string(decodedMaterialPixelBytes_));
}

void CharacterSceneRenderer::onLoad(const RenderContext& context){
    while(!prepareLoad(context,std::numeric_limits<double>::max())){}
    RuntimeDiagnostics::instance().info("render","Optional resources: faceSdfTextures="+std::to_string(faceSdfTextureCount_)+", showcasePlatform="+(platformPrepared_?"true":"false"));
}
bool CharacterSceneRenderer::prepareLoad(const RenderContext& context,double budgetMs){
    const auto start=std::chrono::steady_clock::now();
    if(!loadInitialized_){loadInitialized_=true;initializeLoad(context);}
    do{
        if(loadJobs_.empty())return true;
        auto job=std::move(loadJobs_.front());loadJobs_.pop_front();job();
    }while(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<budgetMs);
    return loadJobs_.empty();
}
bool CharacterSceneRenderer::reuseScene(const RenderContext& context){
    if(!objectShadowPrepared_&&std::any_of(context.scene.nodes.begin(),context.scene.nodes.end(),
        [](const auto& node){return node.objectShadow&&node.objectShadow->enabled;}))return false;
    if(scene_.reflectionProbe.texture!=context.scene.reflectionProbe.texture)return false;
    if(scene_.irradianceVolume.asset!=context.scene.irradianceVolume.asset)return false;
    if(context.renderSettings->faceSdfFeature!=faceSdfPrepared_||context.renderSettings->showcasePlatformFeature!=platformPrepared_)return false;
    if(scene::ResourceLayout(scene_).keys!=scene::ResourceLayout(context.scene).keys)return false;
    scene_=context.scene;animationIndex_=0;animationTime_=0;++temporalSceneGeneration_;temporalView_.clear();motionPoses_.clear();return true;
}

void CharacterSceneRenderer::onSwapchainRecreate(
    const RenderContext& context) {
    temporalView_.clear();
    motionPoses_.clear();motionPass_.reset();
    renderSettings_ = context.renderSettings;
    // The scene render pass is recreated by the engine; the character
    // pipelines reference it and must be rebuilt.
    destroyGraphicsPipelinesForRecreate();
    destroyOverlayDepth();createOverlayDepth(context);
    if(descriptorPool_!=VK_NULL_HANDLE)rhi_->destroyDescriptorPool(descriptorPool_);
    createDescriptorPool();createDescriptorSets();
    createGraphicsPipeline(context);
    initializeMotion(context);
}

void CharacterSceneRenderer::updateFrame(const SceneFrameData& frame) {
    if (frame.sceneSnapshot != nullptr) {
        // GPU resources retain their load-time ordering; runtime nodes and
        // lights are refreshed before uniforms, culling and draw snapshots.
        scene_.nodes = frame.sceneSnapshot->nodes;
        scene_.lights = frame.sceneSnapshot->lights;
        scene_.reflectionProbe = frame.sceneSnapshot->reflectionProbe;
        scene_.irradianceVolume = frame.sceneSnapshot->irradianceVolume;
    }
    if (frame.renderSettings != nullptr) {
        frameRenderSettings_ = *frame.renderSettings;
        renderSettings_ = &frameRenderSettings_;
    }
    currentFrame_ = frame.currentFrame;
    cameraPosition_ = {frame.cameraPosition[0], frame.cameraPosition[1], frame.cameraPosition[2]};
    cameraTarget_ = {frame.cameraTarget[0], frame.cameraTarget[1], frame.cameraTarget[2]};
    rotationAngle_ = frame.rotationAngle;
    selectedPrimitiveIndex_ = frame.selectedPrimitiveIndex;
    gizmoTranslation_ = {frame.gizmoTranslation[0], frame.gizmoTranslation[1], frame.gizmoTranslation[2]};
    gizmoRotation_ = {frame.gizmoRotation[0], frame.gizmoRotation[1], frame.gizmoRotation[2]};
    gizmoScale_ = {frame.gizmoScale[0], frame.gizmoScale[1], frame.gizmoScale[2]};
    gizmoActive_ = frame.gizmoActive;
    qaIsolationMode_ = frame.qaIsolationMode;
    qaEffectMode_ = frame.qaEffectMode;
    qaEffectEnabled_ = frame.qaEffectEnabled;
    qaHarnessEnabled_ = frame.qaHarnessEnabled;
    updateUniformBuffer(frame);
    rebuildSceneInstances();
    prepareTransparentIndices();
    // CPU visibility already participates in this renderer. Preserve stable
    // GPU command slots while omitting known-empty runs from main submission.
    opaqueRecordingSpans_ = visibleSpansByMeshKey_;
    buildSceneState();
    std::vector<std::uint32_t> visibleIndices;
    visibleIndices.reserve(visibleInstances_.size());
    for (const auto* instance : visibleInstances_) visibleIndices.push_back(instance->sourceIndex);
    RecordingBufferSet recordingBuffers;
    recordingBuffers.vertices.reserve(meshResourceCount_);
    recordingBuffers.indices.reserve(meshResourceCount_);
    recordingBuffers.multiDrawIndirect = multiDrawIndirect_;
    recordingBuffers.maxDrawIndirectCount = maxDrawIndirectCount_;
    recordingBuffers.transparentPrimitives.resize(transparentPrimitivesByMesh_.size());
    for (std::size_t key = 0; key < transparentPrimitivesByMesh_.size(); ++key) {
        const auto& mesh = key == 0 ? asset_ : additionalResources_[key - 1]->asset;
        for (const auto* primitive : transparentPrimitivesByMesh_[key])
            recordingBuffers.transparentPrimitives[key].push_back(static_cast<std::size_t>(primitive - mesh.primitives.data()));
    }
    for (std::uint32_t key = 0; key < meshResourceCount_; ++key) {
        recordingBuffers.vertices.push_back(renderVertexBuffer(key, static_cast<std::uint32_t>(currentFrame_)).buffer);
        recordingBuffers.indices.push_back(renderIndexBuffer(key).buffer);
    }
    if (!oitIndexBuffers_.empty()) recordingBuffers.transparentIndices = oitIndexBuffers_[currentFrame_].buffer;
    std::vector<std::vector<std::array<std::uint32_t,3>>> shadowViews;
    for(const auto& frustum:shadowCascadeFrusta_)
        shadowViews.push_back(scene::visibleInstanceSpans(sceneInstances_,frustum,cullingEnabled_));
    std::vector<std::vector<float>> nodeOpacities;
    nodeOpacities.reserve(sceneInstances_.size());
    for (const auto& instance : sceneInstances_)
        nodeOpacities.push_back(instancePoses_.at(instance.sourceIndex).pose.nodeWorldOpacities);
    prepareOutlineMaskResources(nodeOpacities,recordingBuffers);
    instanceSnapshot_ = std::make_shared<const SceneInstanceSnapshot>(
        std::move(sceneInstances_), std::move(visibleIndices), std::move(opaqueRecordingSpans_),
        std::move(visibleShadowSpansByMeshKey_), std::move(indirectInstanceOffsets_), std::move(transparentIndexOffsets_), std::move(visibleSpansByMeshKey_), *renderSettings_,
        gpuCullingFrames_[currentFrame_] ? gpuCullingFrames_[currentFrame_]->output().buffer : VK_NULL_HANDLE, static_cast<std::uint32_t>(currentFrame_),
        RecordingGizmoState{selectedPrimitiveIndex_, gizmoActive_, gizmoTranslation_, gizmoRotation_, gizmoScale_}, std::move(recordingBuffers),std::move(shadowViews),std::move(nodeOpacities),std::move(objectShadowPages_),materialOriginInstances_);
    visibleInstances_.clear();
    prepareMotion();
}

void CharacterSceneRenderer::ensureInstanceCapacity() {
    // The host has retired this frame slot's fence before updateFrame.
    // Grow only its storage; the other slots keep their in-flight buffers.
    VkDeviceSize joints=0,vertices=0,morphBytes=0;
    for(const auto& instance:sceneInstances_){
        const auto& mesh=instance.meshKey==0?asset_:additionalResources_[instance.meshKey-1]->asset;
        joints+=mesh.jointMatrices.size()*sizeof(std::array<float,16>);
        vertices+=mesh.vertices.size()*sizeof(AssetVertex);
        morphBytes+=morphGpuMeshes_.at(instance.meshKey).weightCount*sizeof(float);
    }
    if(morphBytes>kMorphGpuBudgetBytes)throw std::runtime_error("Instance morph weights exceed 128 MiB frame budget");
    const std::array<VkDeviceSize,4> required{joints,vertices,sceneInstances_.size()*sizeof(InstanceGpuData),morphBytes};
    const std::array<rhi::GpuBuffer*,4> slots{&jointBuffers_[currentFrame_],
        computeSkinningEnabled_?&skinnedVertexBuffers_[currentFrame_]:nullptr,&instanceBuffers_[currentFrame_],&morphWeightBuffers_[currentFrame_]};
    std::array<rhi::GpuBuffer,4> candidates{};
    try{
        for(unsigned i=0;i<slots.size();++i)if(slots[i]&&required[i]>slots[i]->size){
            const auto growth=std::max(required[i],slots[i]->size+slots[i]->size/2);
            const auto bytes=i==3?std::min<VkDeviceSize>(growth,kMorphGpuBudgetBytes):growth;
            candidates[i]=allocator_->createBuffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |(i==1?VK_BUFFER_USAGE_VERTEX_BUFFER_BIT:0),i!=1);
        }
    }catch(...){for(auto& candidate:candidates)allocator_->destroyBuffer(candidate);throw;}
    bool changed=false;
    for(unsigned i=0;i<slots.size();++i)if(candidates[i].buffer){
        allocator_->destroyBuffer(*slots[i]);*slots[i]=candidates[i];changed=true;
    }
    if(!changed)return;
    const auto count=bindlessTextures_?1:totalMaterialCount();
    for(std::size_t material=0;material<count;++material){
        const auto set=descriptorSets_[currentFrame_*count+material];
        rhi_->writeDescriptorBuffer({set,10,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,jointBuffers_[currentFrame_].buffer,jointBuffers_[currentFrame_].size});
        rhi_->writeDescriptorBuffer({set,13,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,instanceBuffers_[currentFrame_].buffer,instanceBuffers_[currentFrame_].size});
        rhi_->writeDescriptorBuffer({set,22,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphWeightBuffers_[currentFrame_].buffer,morphWeightBuffers_[currentFrame_].size});
        if(computeSkinningEnabled_)rhi_->writeDescriptorBuffer({set,17,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            skinnedVertexBuffers_[currentFrame_].buffer,skinnedVertexBuffers_[currentFrame_].size});
    }
    if(computeSkinningEnabled_)for(std::uint32_t key=0;key<meshResourceCount_;++key){
        const auto set=skinningDescriptorSets_[currentFrame_*meshResourceCount_+key];
        rhi_->writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,jointBuffers_[currentFrame_].buffer,jointBuffers_[currentFrame_].size});
        rhi_->writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,skinnedVertexBuffers_[currentFrame_].buffer,skinnedVertexBuffers_[currentFrame_].size});
        rhi_->writeDescriptorBuffer({set,4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphWeightBuffers_[currentFrame_].buffer,morphWeightBuffers_[currentFrame_].size});
    }
}
void CharacterSceneRenderer::prepareInstancePoses() {
    ensureInstanceCapacity();
    constexpr float kPi = 3.14159265358979323846F;
    instancePoses_.clear();
    skinningDispatches_.clear();
    std::uint32_t jointBase = 0, vertexBase = 0, morphWeightBase=0;
    for (auto& instance : sceneInstances_) {
        const auto& mesh = instance.meshKey == 0 ? asset_ : additionalResources_[instance.meshKey-1]->asset;
        const auto animation = std::find_if(animationFrames_.begin(), animationFrames_.end(),
            [&](const auto& value) { return value.node == instance.nodeId; });
        auto pose = bindAnimationPose(mesh);
        auto morph = renderSettings_->morphWeights;
        if (animation != animationFrames_.end()) {
            pose = samplePresentedAnimationPose(mesh,*animation);
            morph = animation->morph;
            if (instance.meshKey == 0) { animationIndex_ = animation->clip; }
        } else if (!mesh.animations.empty()) {
            pose = sampleAnimationPose(mesh, instance.meshKey == 0 ? animationIndex_ : 0,
                animationTime_);
        }
        if(animation!=animationFrames_.end()&&animation->expression)
            pose=applyExpressionPose(mesh,pose,*animation->expression,animation->expressionWeight);
        if(!mesh.morphRanges.empty()){
            for(auto& values:pose.morphWeights)for(unsigned index=0;index<std::min<std::size_t>(2,values.size());++index)
                if(morph[index]!=0)values[index]=morph[index];
            if(animation!=animationFrames_.end())applyMorphOverrides(mesh,pose,animation->morphOverrides);
        }
        const auto weights=flattenMorphGpuWeights(mesh,pose);
        if(!weights.empty())std::memcpy(static_cast<float*>(morphWeightBuffers_[currentFrame_].mapped)+morphWeightBase,weights.data(),weights.size()*sizeof(float));
        std::memcpy(static_cast<std::array<float,16>*>(jointBuffers_[currentFrame_].mapped) + jointBase,
            pose.jointMatrices.data(), pose.jointMatrices.size()*sizeof(std::array<float,16>));
        auto bounds = jointBounds_.at(instance.meshKey).evaluate(mesh, morph, pose.jointMatrices,&pose);
        bounds = expandBounds(bounds, materialDisplacementMargin(mesh));
        if (gizmoActive_) {
            const auto gizmo = multiply(translation(gizmoTranslation_[0],gizmoTranslation_[1],gizmoTranslation_[2]),
                multiply(multiply(rotationX(gizmoRotation_[0]*kPi/180),rotationY(gizmoRotation_[1]*kPi/180)),
                    multiply(rotationZ(gizmoRotation_[2]*kPi/180),scale(gizmoScale_[0],gizmoScale_[1],gizmoScale_[2]))));
            bounds = includeTransformedBounds(bounds, gizmo);
        }
        instance.worldBounds = scene::transformBounds(bounds, instance.model);
        instancePoses_.push_back({std::move(pose), instance.meshKey, jointBase, vertexBase, morph,morphWeightBase});
        // Preserve stable pose slices for every instance. Only views that
        // consume deformed geometry require writing the GPU vertex slice.
        const auto& worldBounds = instance.worldBounds;
        const bool requiredByMain = scene::boundsInsideFrustum(viewFrustum_,
            worldBounds.minimum, worldBounds.maximum);
        const bool requiredByShadow = std::any_of(shadowCascadeFrusta_.begin(),
            shadowCascadeFrusta_.end(), [&](const scene::FrustumPlanes& frustum) {
                return scene::boundsInsideFrustum(frustum, worldBounds.minimum, worldBounds.maximum);
            });
        if (!cullingEnabled_ || requiredByMain || requiredByShadow) {
            const SkinningBatch slice{static_cast<std::uint32_t>(mesh.vertices.size()),jointBase,morph,
                vertexBase,static_cast<std::uint32_t>(instancePoses_.back().pose.jointMatrices.size()),1,
                morphGpuMeshes_.at(instance.meshKey).tableBase,morphWeightBase,morphGpuMeshes_.at(instance.meshKey).weightCount};
            if (skinningDispatches_.empty() || skinningDispatches_.back().meshKey!=instance.meshKey
                || !appendSkinningSlice(skinningDispatches_.back().parameters,slice))
                skinningDispatches_.push_back({instance.meshKey,slice});
        }
        jointBase += static_cast<std::uint32_t>(instancePoses_.back().pose.jointMatrices.size());
        vertexBase += static_cast<std::uint32_t>(mesh.vertices.size());
        morphWeightBase+=static_cast<std::uint32_t>(weights.size());
    }
    allocator_->flush(jointBuffers_[currentFrame_],0,jointBuffers_[currentFrame_].size);
    allocator_->flush(morphWeightBuffers_[currentFrame_],0,morphWeightBuffers_[currentFrame_].size);
}
std::array<float,4> CharacterSceneRenderer::instanceFaceLight(
    const LoadedAsset& mesh, const AssetPose& pose, const Matrix4& model) const {
    const AssetFaceSdfProfile* profile = nullptr;
    if(!faceSdfPrepared_)return {0,0,-1,0};
    for (const auto& material : mesh.materials) if (material.faceSdf.present) { profile = &material.faceSdf; break; }
    if (!profile || profile->headNode >= pose.nodeWorldMatrices.size()) return {0,0,-1,0};
    const auto head = multiply(model, pose.nodeWorldMatrices[profile->headNode]);
    const auto& bind = mesh.nodeWorldMatrices.at(profile->headNode);
    const Vector3 local{dot(frameMainLight_,normalize({head[0],head[1],head[2]})),
        dot(frameMainLight_,normalize({head[4],head[5],head[6]})), dot(frameMainLight_,normalize({head[8],head[9],head[10]}))};
    const auto x = normalize({bind[0],bind[1],bind[2]});
    const auto y = normalize({bind[4],bind[5],bind[6]});
    const auto z = normalize({bind[8],bind[9],bind[10]});
    const auto light = normalize({x[0]*local[0]+y[0]*local[1]+z[0]*local[2],
        x[1]*local[0]+y[1]*local[1]+z[1]*local[2], x[2]*local[0]+y[2]*local[1]+z[2]*local[2]});
    return {light[0],light[1],light[2],1};
}

Matrix4 CharacterSceneRenderer::instanceFaceFrame(const LoadedAsset& mesh,const AssetPose& pose,const Matrix4& model) const {
    Matrix4 result=scale(1,1,1);
    if(const auto frame=resolveMaterialFrame(mesh)) {
        const auto& profile=*frame;
        const auto head=multiply(model,pose.nodeWorldMatrices.at(profile.node));
        Matrix4 basis=scale(1,1,1);
        for(unsigned a=0;a<3;++a)for(unsigned c=0;c<3;++c)basis[a*4+c]=profile.basis[a*3+c];
        result=multiply(head,basis);
        for(unsigned a=0;a<3;++a) {
            const auto axis=normalize({result[a*4],result[a*4+1],result[a*4+2]});
            for(unsigned c=0;c<3;++c)result[a*4+c]=axis[c];
        }
        return result;
    }
    return result;
}

void CharacterSceneRenderer::rebuildSceneInstances() {
    const auto primaryBounds = jointBounds_.at(0).evaluate(asset_, renderSettings_->morphWeights, asset_.jointMatrices);
    std::vector<scene::AxisAlignedBounds> meshBounds{primaryBounds};
    for (const auto& resource : additionalResources_)
        meshBounds.push_back(jointBounds_.at(meshBounds.size()).evaluate(resource->asset, renderSettings_->morphWeights, resource->asset.jointMatrices));
    for (std::size_t meshKey = 0; meshKey < meshBounds.size(); ++meshKey) {
        const auto& mesh = meshKey == 0 ? asset_ : additionalResources_[meshKey - 1]->asset;
        meshBounds[meshKey] = expandBounds(meshBounds[meshKey], materialDisplacementMargin(mesh));
    }
    if (gizmoActive_) {
        constexpr float kPi = 3.14159265358979323846F;
        const Matrix4 gizmo = multiply(translation(gizmoTranslation_[0], gizmoTranslation_[1], gizmoTranslation_[2]),
            multiply(multiply(rotationX(gizmoRotation_[0] * kPi / 180.0F), rotationY(gizmoRotation_[1] * kPi / 180.0F)),
                multiply(rotationZ(gizmoRotation_[2] * kPi / 180.0F), scale(gizmoScale_[0], gizmoScale_[1], gizmoScale_[2]))));
        for (auto& bounds : meshBounds) {
            bounds = includeTransformedBounds(bounds, gizmo);
        }
        // View-direction material offset is applied after the gizmo matrix;
        // preserve its unscaled margin even when the gizmo shrinks geometry.
        for (std::size_t meshKey = 0; meshKey < meshBounds.size(); ++meshKey) {
            const auto& mesh = meshKey == 0 ? asset_ : additionalResources_[meshKey - 1]->asset;
            meshBounds[meshKey] = expandBounds(meshBounds[meshKey], materialDisplacementMargin(mesh));
        }
    }
    // Instances come from scene nodes; the QA stress knob clones a
    // single-node scene onto a grid to prove draw counts stay flat as
    // instances grow.
    sceneInstances_.clear();
    if (scene_.nodes.size() == 1 && scene_.nodes.front().visible && qaInstanceCount_ > 1) {
        sceneInstances_.reserve(qaInstanceCount_);
        const std::uint32_t gridSide = static_cast<std::uint32_t>(
            std::ceil(std::sqrt(static_cast<double>(qaInstanceCount_))));
        constexpr float kGridSpacing = 2.2F;
        for (std::uint32_t index = 0; index < qaInstanceCount_; ++index) {
            const float offsetX =
                static_cast<float>(index % gridSide) * kGridSpacing;
            const float offsetZ =
                static_cast<float>(index / gridSide) * kGridSpacing;
            scene::SceneInstance instance{};
            instance.model = multiply(
                translation(offsetX, 0.0F, offsetZ), currentModel_);
            instance.worldBounds = scene::transformBounds(
                meshBounds.front(), instance.model);
            instance.sourceIndex = index;
            instance.nodeId = scene_.nodes.front().id;
            instance.meshKey = 0;
            sceneInstances_.push_back(instance);
        }
    } else {
        const std::vector<Matrix4> nodeWorldTransforms =
            scene::resolveNodeWorldTransforms(scene_);
        const scene::ResourceLayout layout(scene_);
        const auto meshKeyOf = [&](const scene::SceneNodeDesc& node) { return layout.meshKey(node.resourceId); };
        // Instances are grouped by resource so each draw section gets one
        // contiguous span in the instance buffer.
        for (std::size_t meshKey = 0; meshKey < meshBounds.size();
             ++meshKey) {
            for (std::size_t nodeIndex = 0; nodeIndex < scene_.nodes.size();
                 ++nodeIndex) {
                const scene::SceneNodeDesc& node = scene_.nodes[nodeIndex];
                if (!node.visible || meshKeyOf(node) != meshKey) {
                    continue;
                }
                const scene::AxisAlignedBounds& localBounds = meshBounds.at(meshKey);
                const Matrix4 nodeTransform = nodeIndex
                    < nodeWorldTransforms.size()
                    ? nodeWorldTransforms[nodeIndex]
                    : scene::composeTrs(
                        node.translation, node.rotation, node.scale);
                scene::SceneInstance instance{};
                instance.model = meshKey == 0
                    ? multiply(nodeTransform, currentModel_)
                    : nodeTransform;
                instance.worldBounds =
                    scene::transformBounds(localBounds, instance.model);
                instance.sourceIndex =
                    static_cast<std::uint32_t>(sceneInstances_.size());
                instance.meshKey = static_cast<std::uint32_t>(meshKey);
                instance.nodeId = node.id;
                sceneInstances_.push_back(instance);
            }
        }
    }

    materialOriginInstances_=materialOriginNodes_.empty()?nullptr:
        std::make_shared<const MaterialOriginInstances>(sceneInstances_,materialOriginNodes_);
    if(materialOriginInstances_&&materialOriginInstances_->gpuCount()>instanceCapacity_)
        throw std::length_error("Material origin instances exceed the allocated frame buffer");
    prepareInstancePoses();
    if (gpuCullingEnabled_ && !sceneInstances_.empty()) {
        std::vector<GpuCullBounds> bounds;
        std::vector<VkDrawIndexedIndirectCommand> commands;
        std::vector<GpuSurfaceIdentity> surfaces;
        indirectInstanceOffsets_.clear();
        for (const auto& instance : sceneInstances_) {
            indirectInstanceOffsets_.push_back(commands.size());
            bounds.push_back({
                {instance.worldBounds.minimum[0], instance.worldBounds.minimum[1], instance.worldBounds.minimum[2], 0},
                {instance.worldBounds.maximum[0], instance.worldBounds.maximum[1], instance.worldBounds.maximum[2], 0}});
            const auto& mesh = instance.meshKey == 0 ? asset_ : additionalResources_[instance.meshKey - 1]->asset;
            std::uint32_t primitiveIndex=0;
            const auto materialBase=instance.meshKey==0?0U:
                static_cast<std::uint32_t>(additionalResources_[instance.meshKey-1]->globalMaterialBase);
            for (const auto& primitive : mesh.primitives) {
                commands.push_back({primitive.indexCount, 1, primitive.firstIndex, 0, instance.sourceIndex});
                if(visibilityPrototype_)surfaces.push_back({instance.sourceIndex,primitiveIndex,materialBase+primitive.materialIndex,0});
                ++primitiveIndex;
            }
        }
        const auto capacity = std::max(bounds.size(), commands.size());
        if (gpuCullingCapacities_[currentFrame_] < capacity) {
            auto resources = std::make_unique<GpuCullingResources>(*rhi_);
            resources->initialize(azurerender::readBinaryFile(shaderDirectory_ +
                                  (visibilityPrototype_?"/visibility_prototype.comp.spv":"/cull_indirect.comp.spv")),
                                  static_cast<std::uint32_t>(capacity),visibilityPrototype_);
            gpuCullingFrames_[currentFrame_] = std::move(resources);
            gpuCullingCapacities_[currentFrame_] = capacity;
        }
        if(visibilityPrototype_) {
            VisibilityPrototype::validateSources(bounds,commands,surfaces);
            gpuCullingFrames_[currentFrame_]->upload(bounds,commands,surfaces);
        } else gpuCullingFrames_[currentFrame_]->upload(bounds, commands);
    }
    visibleInstances_.clear();
    if (cullingEnabled_) {
        static_cast<void>(scene::appendVisibleInstances(
            sceneInstances_, viewFrustum_, visibleInstances_));
    } else {
        for (const scene::SceneInstance& entry : sceneInstances_) {
            visibleInstances_.push_back(&entry);
        }
    }

    visibleSpansByMeshKey_.clear();
    for (std::size_t slot = 0; slot < visibleInstances_.size(); ++slot) {
        const std::uint32_t meshKey = visibleInstances_[slot]->meshKey;
        const std::uint32_t sourceIndex =
            visibleInstances_[slot]->sourceIndex;
        if (!visibleSpansByMeshKey_.empty()
            && visibleSpansByMeshKey_.back()[0] == meshKey
            && scene::mirroredTransform(sceneInstances_[visibleSpansByMeshKey_.back()[1]].model)
                == scene::mirroredTransform(visibleInstances_[slot]->model)
            && visibleSpansByMeshKey_.back()[1]
                    + visibleSpansByMeshKey_.back()[2]
                == sourceIndex) {
            ++visibleSpansByMeshKey_.back()[2];
        } else {
            visibleSpansByMeshKey_.push_back(
                {meshKey, sourceIndex, 1U});
        }
    }
    visibleShadowSpansByMeshKey_.clear();
    for (std::size_t slot = 0; slot < sceneInstances_.size(); ++slot) {
        const scene::SceneInstance* instance = &sceneInstances_[slot];
        const bool intersectsShadowCascade = std::any_of(
            shadowCascadeFrusta_.begin(),
            shadowCascadeFrusta_.end(),
            [instance](const scene::FrustumPlanes& frustum) {
                return scene::boundsInsideFrustum(
                    frustum,
                    instance->worldBounds.minimum,
                    instance->worldBounds.maximum);
            });
        if (cullingEnabled_ && !intersectsShadowCascade) {
            continue;
        }
        const std::uint32_t meshKey = instance->meshKey;
        if (!visibleShadowSpansByMeshKey_.empty()
            && visibleShadowSpansByMeshKey_.back()[0] == meshKey
            && visibleShadowSpansByMeshKey_.back()[1]
                    + visibleShadowSpansByMeshKey_.back()[2]
                == instance->sourceIndex) {
            ++visibleShadowSpansByMeshKey_.back()[2];
        } else {
            visibleShadowSpansByMeshKey_.push_back(
                {meshKey, instance->sourceIndex, 1U});
        }
    }

    prepareObjectShadows();
    if (sceneInstances_.empty() || instanceBuffers_.empty()) {
        return;
    }
    auto* destination = static_cast<InstanceGpuData*>(
        instanceBuffers_[currentFrame_].mapped);
    const std::size_t instanceCount = sceneInstances_.size();
    for (std::size_t slot = 0; slot < instanceCount; ++slot) {
        const Matrix4& model = sceneInstances_[slot].model;
        destination[slot].model = model;
        const std::uint32_t meshKey = sceneInstances_[slot].meshKey;
        const auto& pose = instancePoses_.at(slot);
        const auto& mesh = meshKey == 0 ? asset_ : additionalResources_[meshKey-1]->asset;
        destination[slot].meta = {pose.jointBase, pose.vertexBase, morphGpuMeshes_.at(meshKey).tableBase, pose.morphWeightBase};
        destination[slot].faceLight = instanceFaceLight(mesh, pose.pose, model);
        destination[slot].morph = {pose.morph[0], pose.morph[1]};
        destination[slot].objectShadowIndex=0;
        destination[slot].instanceFlags=0;
        destination[slot].faceFrame = instanceFaceFrame(mesh,pose.pose,model);
        // The same association as the per-frame uniform path so a single
        // instance reproduces the original values exactly.
        destination[slot].modelViewProjection =
            multiply(samplingProjectionMatrix_, multiply(viewMatrix_, model));
        for (std::size_t cascade = 0;
             cascade < kShadowCascadeCount;
             ++cascade) {
            destination[slot].cascadeLightModelViewProjection[cascade] =
                multiply(cascadeLightViewProjections_[cascade], model);
        }
    }
    for(std::size_t index=0;index<objectShadowPages_.size();++index)
        destination[objectShadowPages_[index].instanceIndex].objectShadowIndex=static_cast<std::uint32_t>(index+1);
    if(materialOriginInstances_)for(const auto& copy:materialOriginInstances_->copies()) {
        const auto& instance=sceneInstances_.at(copy.sourceIndex);
        const auto& pose=instancePoses_.at(copy.sourceIndex).pose;
        const auto origin=multiply(instance.model,pose.nodeWorldMatrices.at(copy.node));
        auto& alias=destination[copy.gpuIndex];alias=destination[copy.sourceIndex];
        for(unsigned axis=0;axis<3;++axis)alias.faceFrame[12+axis]=origin[12+axis];
        alias.instanceFlags|=InstanceGpuData::kMaterialRimOrigin;
    }
    allocator_->flush(instanceBuffers_[currentFrame_],0,instanceBuffers_[currentFrame_].size);
}

std::uint32_t CharacterSceneRenderer::materialOriginFirstInstance(const AssetMaterial& material,
    const SceneInstanceSnapshot* snapshot,std::uint32_t first,std::uint32_t count) const {
    const auto& origin=material.nprDiffuse.rimOrigin;
    if(!origin.present)return first;
    const auto& plan=snapshot?snapshot->materialOrigins:materialOriginInstances_;
    if(!plan)throw std::logic_error("Material rim origin instance plan is unavailable");
    return plan->firstInstance(origin.node,first,count);
}

void CharacterSceneRenderer::prepareObjectShadows(){
    std::unordered_map<std::string,const ecs::ObjectShadowComponent*> settings;
    for(const auto& node:scene_.nodes)if(node.objectShadow&&node.objectShadow->enabled)settings.emplace(node.id,&*node.objectShadow);
    std::vector<ObjectShadowInput> inputs;
    for(const auto& instance:sceneInstances_){
        const auto found=settings.find(instance.nodeId);
        if(found!=settings.end())inputs.push_back({*found->second,instance.model,instance.sourceIndex});
    }
    if(!inputs.empty()&&!objectShadowPrepared_)throw std::runtime_error("Object shadow layout is unavailable; reload the scene with a supported resource profile");
    objectShadowPages_=buildObjectShadowFrame(inputs,internal::scaleVector(frameMainLight_,-1.F)).pages;
    if(!objectShadowPrepared_)return;
    std::vector<ObjectShadowGpuRecord> records;
    for(const auto& page:objectShadowPages_)records.push_back(page.gpu);
    shadowBounds_[currentFrame_]->writeObjectRecords(records);
    if(!objectShadowPages_.empty())objectShadowAtlas_->ensureImage(objectShadowMaxDimension_);
    const auto view=objectShadowAtlas_->view()?objectShadowAtlas_->view():shadowImageView_;
    if(objectShadowBoundViews_[currentFrame_]==view)return;
    const std::size_t count=bindlessTextures_?1:totalMaterialCount();
    for(std::size_t index=0;index<count;++index)
        rhi_->writeDescriptorImage({descriptorSets_.at(currentFrame_*count+index),29,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
    objectShadowBoundViews_[currentFrame_]=view;
}

const rhi::GpuBuffer& CharacterSceneRenderer::renderVertexBuffer(
    const std::uint32_t meshKey,
    const std::uint32_t frameIndex) const {
    (void)frameIndex;
    return meshKey == 0 ? vertexBuffer_ : additionalResources_[meshKey - 1]->vertexBuffer;
}

const rhi::GpuBuffer& CharacterSceneRenderer::renderIndexBuffer(
    const std::uint32_t meshKey) const {
    return meshKey == 0
        ? indexBuffer_
        : additionalResources_[meshKey - 1]->indexBuffer;
}

void CharacterSceneRenderer::registerPasses(
    RenderGraph& graph, const SceneGraphResources& resources, const RenderContext& context) {
    auto commandContext = context;
    commandContext.scene = {};
    const auto frozenContext = std::make_shared<const RenderContext>(std::move(commandContext));
    std::vector<RenderGraph::ResourceId> vertexResources(meshResourceCount_);
    std::vector<RenderGraph::ResourceId> indexResources(meshResourceCount_);
    const auto sourceVertexBuffer = [this](const std::uint32_t meshKey)
        -> const rhi::GpuBuffer& {
        return meshKey == 0
            ? vertexBuffer_
            : additionalResources_[meshKey - 1]->vertexBuffer;
    };
    const auto meshIndexBuffer = [this](const std::uint32_t meshKey)
        -> const rhi::GpuBuffer& {
        return meshKey == 0
            ? indexBuffer_
            : additionalResources_[meshKey - 1]->indexBuffer;
    };

    rhi::BufferBarrierDesc jointInitial{};
    jointInitial.buffer = jointBuffers_[context.currentFrame].buffer;
    jointInitial.size = jointBuffers_[context.currentFrame].size;
    jointInitial.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
    jointInitial.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    const auto jointResource = graph.importBuffer(
        "character-joints", jointInitial);
    const auto importHostStorage = [&graph](
        const std::string& name,
        const rhi::GpuBuffer& buffer) {
        rhi::BufferBarrierDesc initial{};
        initial.buffer = buffer.buffer;
        initial.size = buffer.size;
        initial.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
        initial.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        return graph.importBuffer(name, initial);
    };
    const auto lightResource = importHostStorage(
        "character-lights", lightBuffers_[context.currentFrame]);
    const auto morphTargetResource=importHostStorage("character-morph-targets-v1",morphTargetBuffer_);
    const auto morphWeightResource=importHostStorage("character-morph-weights",morphWeightBuffers_[context.currentFrame]);
    const auto instanceResource=importHostStorage("character-instances-v2",instanceBuffers_[context.currentFrame]);
    const auto clusterHeaderResource = importHostStorage(
        "character-cluster-headers",
        clusterHeaderBuffers_[context.currentFrame]);
    const auto clusterIndexResource = importHostStorage(
        "character-cluster-indices",
        clusterIndexBuffers_[context.currentFrame]);

    std::optional<RenderGraph::PassId> skinningPass;
    if (computeSkinningEnabled_) {
        skinningPass = graph.addCommandPass("character-skinning",
            [this, frozenContext, dispatches = skinningDispatches_,
             meshCount = meshResourceCount_](rhi::ICommandRecorder& commands) {
                for (std::uint32_t meshKey = 0; meshKey < meshCount; ++meshKey)
                    recordComputeSkinningMesh(*frozenContext, meshKey, &commands, &dispatches);
            }, true);
        graph.use(*skinningPass, jointResource, RenderGraphUsage::Storage, false);
        graph.use(*skinningPass,morphTargetResource,RenderGraphUsage::Storage,false);
        graph.use(*skinningPass,morphWeightResource,RenderGraphUsage::Storage,false);
    }

    std::optional<RenderGraph::ResourceId> skinnedResource;
    if (skinningPass) {
        rhi::BufferBarrierDesc initial{};
        initial.buffer = skinnedVertexBuffers_[context.currentFrame].buffer;
        initial.size = skinnedVertexBuffers_[context.currentFrame].size;
        initial.dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        initial.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        skinnedResource = graph.importBuffer("character-instance-skinned", initial);
        graph.use(*skinningPass, *skinnedResource, RenderGraphUsage::Storage, true);
    }

    for (std::uint32_t meshKey = 0; meshKey < meshResourceCount_; ++meshKey) {
        const rhi::GpuBuffer& input = sourceVertexBuffer(meshKey);
        rhi::BufferBarrierDesc vertexInitial{};
        vertexInitial.buffer = input.buffer;
        vertexInitial.size = input.size;
        vertexInitial.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        vertexInitial.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        const auto inputResource = graph.importBuffer(
            "character-vertex-source-" + std::to_string(meshKey),
            vertexInitial);

        vertexResources[meshKey] = inputResource;

        const rhi::GpuBuffer& indices = meshIndexBuffer(meshKey);
        rhi::BufferBarrierDesc indexInitial{};
        indexInitial.buffer = indices.buffer;
        indexInitial.size = indices.size;
        indexInitial.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        indexInitial.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        indexResources[meshKey] = graph.importBuffer(
            "character-index-" + std::to_string(meshKey), indexInitial);

        if (skinningPass) {
            graph.use(*skinningPass, inputResource, RenderGraphUsage::Storage, false);
        }
    }

    rhi::RenderPassBeginDesc shadowBegin{};
    shadowBegin.renderPass = context.shadowRenderPass;
    shadowBegin.framebuffer = context.shadowFramebuffer;
    shadowBegin.extent = {context.shadowMapSize, context.shadowMapSize};
    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0F, 0};
    shadowBegin.clearValues = {shadowClear};
    const auto frozenInstances = instanceSnapshot_;
    if (!frozenInstances) throw std::logic_error("Scene passes require a prepared instance snapshot");
    frozenInstances->validateForRecording();
    if (frozenInstances->buffers.transparentPrimitives.size() != meshResourceCount_)
        throw std::logic_error("Snapshot transparent mesh mapping incomplete");
    for (std::size_t key = 0; key < meshResourceCount_; ++key) {
        const auto& mesh = key == 0 ? asset_ : additionalResources_[key - 1]->asset;
        for (const auto index : frozenInstances->buffers.transparentPrimitives[key])
            if (index >= mesh.primitives.size())
                throw std::out_of_range("Snapshot transparent primitive mapping");
    }
    const auto shadow = graph.addGraphicsPass("character-shadow", shadowBegin, [this, frozenContext, frozenInstances](rhi::ICommandRecorder& commands) {
        const auto& context = *frozenContext;
        recordShadowDraws(context, 0, kShadowCascadeCount, frozenInstances.get(), &commands);
    }, [frozenContext](rhi::ICommandRecorder& commands) {
        const auto& context = *frozenContext;
        if (context.gpuTimingEnabled)
            commands.writeTimestamp(context.timestampQueryPool, 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });
    for (std::uint32_t meshKey = 0; meshKey < meshResourceCount_; ++meshKey) {
        graph.use(shadow, vertexResources[meshKey], RenderGraphUsage::VertexBuffer, false);
        graph.use(shadow, indexResources[meshKey], RenderGraphUsage::IndexBuffer, false);
    }
    graph.use(shadow, jointResource, RenderGraphUsage::VertexStorage, false);
    graph.use(shadow,instanceResource,RenderGraphUsage::VertexStorage,false);
    graph.use(shadow,morphTargetResource,RenderGraphUsage::VertexStorage,false);
    graph.use(shadow,morphWeightResource,RenderGraphUsage::VertexStorage,false);
    if (skinnedResource) graph.use(shadow, *skinnedResource, RenderGraphUsage::VertexStorage, false);
    graph.attachment(shadow, resources.shadow, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    auto* bounds=shadowBounds_[context.currentFrame].get();
    rhi::BufferBarrierDesc boundsInitial{};boundsInitial.buffer=bounds->buffer().buffer;boundsInitial.size=bounds->depthBoundsBytes();
    boundsInitial.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;boundsInitial.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    const auto boundsResource=graph.importBuffer("shadow-tile-bounds",boundsInitial);
    const auto boundsPass=graph.addCommandPass("shadow-tile-bounds",[bounds](rhi::ICommandRecorder& commands){bounds->record(commands);});
    graph.use(boundsPass,resources.shadow,RenderGraphUsage::ComputeSampled,false);
    graph.use(boundsPass,boundsResource,RenderGraphUsage::Storage,true);
    std::optional<RenderGraph::ResourceId> objectRecordsResource,objectAtlasResource;
    if(objectShadowPrepared_){
        auto initial=boundsInitial;initial.offset=bounds->objectRecordsOffset();initial.size=bounds->buffer().size-initial.offset;
        initial.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;initial.dstAccessMask=VK_ACCESS_HOST_WRITE_BIT;
        objectRecordsResource=graph.importBuffer("object-shadow-records-v1",initial);
    }
    if(!frozenInstances->objectShadowPages.empty()){
        rhi::ImageBarrierDesc initial{};initial.image=objectShadowAtlas_->image().image;
        initial.newLayout=VK_IMAGE_LAYOUT_UNDEFINED;initial.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
        objectAtlasResource=graph.importImage("object-shadow-atlas",initial);
        const auto objectPass=graph.addGraphicsPass("object-shadow-atlas",objectShadowAtlas_->beginDescription(),
            [this,frozenContext,frozenInstances](rhi::ICommandRecorder& commands){recordObjectShadows(*frozenContext,*frozenInstances,commands);});
        for(std::uint32_t key=0;key<meshResourceCount_;++key){
            graph.use(objectPass,vertexResources[key],RenderGraphUsage::VertexBuffer,false);
            graph.use(objectPass,indexResources[key],RenderGraphUsage::IndexBuffer,false);
        }
        for(const auto resource:{jointResource,instanceResource,morphTargetResource,morphWeightResource})
            graph.use(objectPass,resource,RenderGraphUsage::VertexStorage,false);
        if(skinnedResource)graph.use(objectPass,*skinnedResource,RenderGraphUsage::VertexStorage,false);
        graph.attachment(objectPass,*objectAtlasResource,RenderGraphUsage::DepthAttachment,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }
    const auto mainCounters = std::make_shared<SceneSubmissionCounters>();
    mainCounters->instances = frozenInstances->instances.size();
    mainCounters->visibleInstances = frozenInstances->visibleIndices.size();
    const auto transparentChunks = transparentDrawsPrepared_
        ? std::min<std::size_t>(4, std::max<std::size_t>(1, (frozenInstances->visibleIndices.size() + 7) / 8)) : 0;
    const auto stageCounters = std::make_shared<std::vector<SceneSubmissionCounters>>(1 + transparentChunks);
    std::optional<RenderGraph::ResourceId> indirectResource;
    if (gpuCullingFrames_[context.currentFrame]) {
        auto* culling = gpuCullingFrames_[context.currentFrame].get();
        const auto importHostBuffer = [&graph](const char* name, const rhi::GpuBuffer& buffer) {
            rhi::BufferBarrierDesc initial{};
            initial.buffer = buffer.buffer;
            initial.size = buffer.size;
            initial.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
            initial.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            return graph.importBuffer(name, initial);
        };
        const auto bounds = importHostBuffer("cull-bounds", culling->bounds());
        const auto source = importHostBuffer("cull-source", culling->source());
        rhi::BufferBarrierDesc outputInitial{};
        outputInitial.buffer = culling->output().buffer;
        outputInitial.size = culling->output().size;
        outputInitial.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        outputInitial.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        indirectResource = graph.importBuffer("cull-output", outputInitial);
        GpuCullParameters parameters{viewFrustum_.planes, 0, cullingEnabled_ ? 1U : 0U};
        const auto cull = graph.addCommandPass("character-gpu-culling",
            [culling, parameters](rhi::ICommandRecorder& commands) { culling->record(commands, parameters); }, true);
        graph.use(cull, bounds, RenderGraphUsage::Storage, false);
        graph.use(cull, source, RenderGraphUsage::Storage, false);
        graph.use(cull, *indirectResource, RenderGraphUsage::Storage, true);
        if(visibilityPrototype_) {
            const auto identities=importHostBuffer("visibility-surface-source",culling->surfaceSource());
            rhi::BufferBarrierDesc initial{};
            initial.buffer=culling->surfaceOutput().buffer;initial.size=culling->surfaceOutput().size;
            initial.dstStageMask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            initial.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
            const auto output=graph.importBuffer("visibility-surface-output",initial);
            graph.use(cull,identities,RenderGraphUsage::Storage,false);
            graph.use(cull,output,RenderGraphUsage::Storage,true);
        }
    }
    rhi::ImageBarrierDesc overlayInitial{};overlayInitial.image=overlaySceneDepth_[context.currentFrame].image.image;
    overlayInitial.newLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    const auto overlayDepth=graph.importImage("character-overlay-depth",overlayInitial);
    rhi::RenderPassBeginDesc overlayBegin{};overlayBegin.renderPass=snapshotPass();
    overlayBegin.framebuffer=snapshotFramebuffer(context.currentFrame);overlayBegin.extent=context.renderExtent;
    VkClearValue linearClear{};linearClear.color.float32[0]=100000;VkClearValue depthClear{};depthClear.depthStencil={1,0};
    VkClearValue colorClear{};overlayBegin.clearValues=snapshotMultisampling_?std::vector<VkClearValue>{colorClear,depthClear,linearClear}:std::vector<VkClearValue>{colorClear,linearClear,depthClear};
    std::array<RenderGraph::ResourceId,3> snapshotSources{};
    if(snapshotMultisampling_)snapshotSources=snapshotMultisampling_->importSources(graph,context.currentFrame,"character-opaque-source");
    const auto prepass=graph.addGraphicsPass("character-opaque-depth",overlayBegin,[this,frozenContext,frozenInstances](rhi::ICommandRecorder& commands){
        recordOverlayDepth(*frozenContext,*frozenInstances,commands);
    });

    rhi::ImageBarrierDesc snapshotState{};snapshotState.image=opaqueSceneColor_[context.currentFrame].image.image;
    const auto snapshotColor=graph.importImage("character-opaque-color",snapshotState);
    if(snapshotMultisampling_){
        for(unsigned i=0;i<3;++i)graph.attachment(prepass,snapshotSources[i],i==1?RenderGraphUsage::DepthAttachment:RenderGraphUsage::ColorAttachment,
            i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        rhi::ImageBarrierDesc depthState{};depthState.image=overlayDepthAttachments_[context.currentFrame].image;depthState.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
        const auto snapshotDepth=graph.importImage("character-opaque-hardware-depth",depthState);
        snapshotMultisampling_->schedule(graph,context.currentFrame,context.currentFrame,snapshotSources,{snapshotColor,snapshotDepth,overlayDepth},"character-opaque-resolve");
    }else{
        graph.attachment(prepass,overlayDepth,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        graph.attachment(prepass,snapshotColor,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    graph.use(prepass,resources.shadow,RenderGraphUsage::Sampled,false);
    graph.use(prepass,jointResource,RenderGraphUsage::VertexStorage,false);
    graph.use(prepass,morphTargetResource,RenderGraphUsage::VertexStorage,false);
    graph.use(prepass,morphWeightResource,RenderGraphUsage::VertexStorage,false);
    graph.use(prepass,boundsResource,RenderGraphUsage::FragmentStorage,false);
    graph.use(prepass,instanceResource,RenderGraphUsage::VertexStorage,false);
    if(objectRecordsResource)graph.use(prepass,*objectRecordsResource,RenderGraphUsage::FragmentStorage,false);
    if(objectAtlasResource)graph.use(prepass,*objectAtlasResource,RenderGraphUsage::Sampled,false);
    graph.use(prepass,lightResource,RenderGraphUsage::FragmentStorage,false);
    graph.use(prepass,clusterHeaderResource,RenderGraphUsage::FragmentStorage,false);
    graph.use(prepass,clusterIndexResource,RenderGraphUsage::FragmentStorage,false);
    if(skinnedResource)graph.use(prepass,*skinnedResource,RenderGraphUsage::VertexStorage,false);
    for(std::uint32_t key=0;key<meshResourceCount_;++key){graph.use(prepass,vertexResources[key],RenderGraphUsage::VertexBuffer,false);graph.use(prepass,indexResources[key],RenderGraphUsage::IndexBuffer,false);}
    std::optional<RenderGraph::ResourceId> outlineMaskResource;
    if(frozenInstances->buffers.outlineMaskImage){
        rhi::ImageBarrierDesc maskInitial{};maskInitial.image=frozenInstances->buffers.outlineMaskImage;
        outlineMaskResource=graph.importImage("character-outline-mask",maskInitial);
        rhi::RenderPassBeginDesc maskBegin{};maskBegin.renderPass=outlineMaskPass_;
        maskBegin.framebuffer=frozenInstances->buffers.outlineMaskFramebuffer;maskBegin.extent=frozenInstances->buffers.outlineMaskExtent;
        maskBegin.clearValues={VkClearValue{}};
        const auto maskPass=graph.addGraphicsPass("character-outline-mask",maskBegin,[this,frozenContext,frozenInstances](rhi::ICommandRecorder& commands){
            recordOutlineMask(*frozenContext,*frozenInstances,commands);
        });
        graph.attachment(maskPass,*outlineMaskResource,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        graph.use(maskPass,jointResource,RenderGraphUsage::VertexStorage,false);
        graph.use(maskPass,morphTargetResource,RenderGraphUsage::VertexStorage,false);
        graph.use(maskPass,morphWeightResource,RenderGraphUsage::VertexStorage,false);
        if(skinnedResource)graph.use(maskPass,*skinnedResource,RenderGraphUsage::VertexStorage,false);
        for(std::uint32_t key=0;key<meshResourceCount_;++key){
            graph.use(maskPass,vertexResources[key],RenderGraphUsage::VertexBuffer,false);
            graph.use(maskPass,indexResources[key],RenderGraphUsage::IndexBuffer,false);
        }
    }
    const auto main = graph.addGraphicsPass("character-main", mainPassDescription(context), [this, frozenContext, mainCounters, frozenInstances](rhi::ICommandRecorder& commands) {
        const auto& context = *frozenContext;
        recordMainDraws(context, 0, frozenInstances.get(), 0, static_cast<std::size_t>(-1),
            &commands, context.submissionCounters != nullptr ? mainCounters.get() : nullptr);
    }, [frozenContext, mainCounters, stageCounters](rhi::ICommandRecorder& commands) {
        const auto& context = *frozenContext;
        for (const auto& counters : *stageCounters) {
            mainCounters->drawCalls += counters.drawCalls;
            mainCounters->indirectDrawCalls += counters.indirectDrawCalls;
            mainCounters->descriptorSetBinds += counters.descriptorSetBinds;
            mainCounters->pipelineBinds += counters.pipelineBinds;
            mainCounters->pushConstantUpdates += counters.pushConstantUpdates;
        }
        if (context.submissionCounters != nullptr) {
            context.submissionCounters->instances += mainCounters->instances;
            context.submissionCounters->visibleInstances += mainCounters->visibleInstances;
            context.submissionCounters->drawCalls += mainCounters->drawCalls;
            context.submissionCounters->indirectDrawCalls += mainCounters->indirectDrawCalls;
            context.submissionCounters->descriptorSetBinds += mainCounters->descriptorSetBinds;
            context.submissionCounters->pipelineBinds += mainCounters->pipelineBinds;
            context.submissionCounters->pushConstantUpdates += mainCounters->pushConstantUpdates;
        }
        if (context.gpuTimingEnabled)
            commands.writeTimestamp(context.timestampQueryPool, 2, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });
    std::vector<std::function<void(rhi::ICommandRecorder&)>> mainChunks;
    for (std::size_t chunk = 0; chunk <= transparentChunks; ++chunk) {
        const auto count = frozenInstances->visibleIndices.size();
        const auto first = chunk == 0 ? 0 : count * (chunk - 1) / transparentChunks;
        const auto end = chunk == 0 ? 0 : count * chunk / transparentChunks;
        mainChunks.push_back([this, frozenContext, stageCounters, chunk, first, end, frozenInstances](rhi::ICommandRecorder& commands) {
            recordMainDraws(*frozenContext, chunk == 0 ? 4U : 3U, frozenInstances.get(), first, end - first,
                &commands, frozenContext->submissionCounters != nullptr ? &(*stageCounters)[chunk] : nullptr);
        });
    }
    graph.setRecordingChunks(main, std::move(mainChunks));
    graph.use(main, overlayDepth,RenderGraphUsage::Sampled,false);
    graph.use(main,snapshotColor,RenderGraphUsage::Sampled,false);
    if(outlineMaskResource)graph.use(main,*outlineMaskResource,RenderGraphUsage::Sampled,false);
    graph.use(main, resources.shadow, RenderGraphUsage::Sampled, false);
    if (!oitIndexBuffers_.empty() && oitIndexBuffers_[context.currentFrame].buffer != VK_NULL_HANDLE) {
        const auto& buffer = oitIndexBuffers_[context.currentFrame];
        rhi::BufferBarrierDesc initial{};
        initial.buffer = buffer.buffer;
        initial.size = buffer.size;
        initial.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
        initial.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        const auto transparentIndices = graph.importBuffer("transparent-sorted-indices", initial);
        graph.use(main, transparentIndices, RenderGraphUsage::IndexBuffer, false);
    }
    if (indirectResource) graph.use(main, *indirectResource, RenderGraphUsage::IndirectBuffer, false);
    graph.use(main, boundsResource, RenderGraphUsage::FragmentStorage, false);
    graph.use(main,instanceResource,RenderGraphUsage::VertexStorage,false);
    if(objectRecordsResource)graph.use(main,*objectRecordsResource,RenderGraphUsage::FragmentStorage,false);
    if(objectAtlasResource)graph.use(main,*objectAtlasResource,RenderGraphUsage::Sampled,false);
    graph.use(main, lightResource, RenderGraphUsage::FragmentStorage, false);
    graph.use(main, clusterHeaderResource, RenderGraphUsage::FragmentStorage, false);
    graph.use(main, clusterIndexResource, RenderGraphUsage::FragmentStorage, false);
    for (std::uint32_t meshKey = 0; meshKey < meshResourceCount_; ++meshKey) {
        graph.use(main, vertexResources[meshKey], RenderGraphUsage::VertexBuffer, false);
        graph.use(main, indexResources[meshKey], RenderGraphUsage::IndexBuffer, false);
    }
    graph.use(main, jointResource, RenderGraphUsage::VertexStorage, false);
    graph.use(main,morphTargetResource,RenderGraphUsage::VertexStorage,false);
    graph.use(main,morphWeightResource,RenderGraphUsage::VertexStorage,false);
    if (skinnedResource) graph.use(main, *skinnedResource, RenderGraphUsage::VertexStorage, false);
    graph.attachment(main, resources.color, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    graph.attachment(main, resources.depth, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    graph.attachment(main, resources.normal, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if(motionPass_){
        if(resources.motion==UINT32_MAX||resources.motionSurfaceDepth==UINT32_MAX)
            throw std::invalid_argument("Motion-enabled renderer requires imported view outputs");
        const auto pass=graph.addCommandPass("character-motion",[this,frozenContext,draws=motionDraws_](rhi::ICommandRecorder& commands){
            motionPass_->record(commands,frozenContext->currentFrame,frozenContext->imageIndex,frozenContext->renderExtent,draws);
        });
        graph.use(pass,instanceResource,RenderGraphUsage::VertexStorage,false);
        graph.use(pass,jointResource,RenderGraphUsage::VertexStorage,false);
        graph.use(pass,morphTargetResource,RenderGraphUsage::VertexStorage,false);
        graph.use(pass,morphWeightResource,RenderGraphUsage::VertexStorage,false);
        if(outlineMaskResource)graph.use(pass,*outlineMaskResource,RenderGraphUsage::Sampled,false);
        if(outlineProjectionEnabled_)graph.use(pass,importHostStorage("character-motion-outline",outlineProjection_->buffer()),RenderGraphUsage::VertexStorage,false);
        graph.use(pass,importHostStorage("character-previous-poses",motionPass_->previous(context.currentFrame)),RenderGraphUsage::VertexStorage,false);
        for(std::uint32_t key=0;key<meshResourceCount_;++key){
            graph.use(pass,indexResources[key],RenderGraphUsage::IndexBuffer,false);
            if(!computeSkinningEnabled_)graph.use(pass,vertexResources[key],RenderGraphUsage::VertexBuffer,false);
            else {rhi::BufferBarrierDesc initial;const auto& source=sourceVertexBuffer(key);initial.buffer=source.buffer;initial.size=source.size;
                initial.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;initial.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                graph.use(pass,graph.importBuffer("character-motion-base-"+std::to_string(key),initial),RenderGraphUsage::VertexBuffer,false);}
        }
        graph.attachment(pass,resources.depth,RenderGraphUsage::DepthReadAttachment,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        graph.attachment(pass,resources.motion,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        graph.attachment(pass,resources.motionSurfaceDepth,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void CharacterSceneRenderer::recordScene(const RenderContext& context) {
    if (!instanceSnapshot_) throw std::logic_error("Direct recording requires a prepared snapshot");
    instanceSnapshot_->validateForRecording();
    for(const auto* buffer:{&morphTargetBuffer_,&morphWeightBuffers_[context.currentFrame],&jointBuffers_[context.currentFrame],&instanceBuffers_[context.currentFrame]})
        context.commands->bufferBarrier({buffer->buffer,0,buffer->size,VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    recordComputeSkinning(context);
    recordShadowPass(context);
    if(!instanceSnapshot_->objectShadowPages.empty()){
        context.commands->beginRenderPass(objectShadowAtlas_->beginDescription());
        recordObjectShadows(context,*instanceSnapshot_,*context.commands);
        context.commands->endRenderPass();
        context.commands->imageBarrier({objectShadowAtlas_->image().image,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    }
    context.commands->imageBarrier({context.shadowImage,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    const auto& bounds=shadowBounds_[context.currentFrame];
    context.commands->bufferBarrier({bounds->buffer().buffer,0,bounds->depthBoundsBytes(),VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT});
    bounds->record(*context.commands);
    context.commands->bufferBarrier({bounds->buffer().buffer,0,bounds->depthBoundsBytes(),VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    if(objectShadowPrepared_)context.commands->bufferBarrier({bounds->buffer().buffer,bounds->objectRecordsOffset(),bounds->buffer().size-bounds->objectRecordsOffset(),
        VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    rhi::RenderPassBeginDesc begin{};begin.renderPass=snapshotPass();begin.framebuffer=snapshotFramebuffer(context.currentFrame);begin.extent=context.renderExtent;
    VkClearValue color{};VkClearValue linear{};linear.color.float32[0]=100000;VkClearValue depth{};depth.depthStencil={1,0};begin.clearValues=snapshotMultisampling_?std::vector<VkClearValue>{color,depth,linear}:std::vector<VkClearValue>{color,linear,depth};
    context.commands->beginRenderPass(begin);recordOverlayDepth(context,*instanceSnapshot_,*context.commands);context.commands->endRenderPass();
    if(snapshotMultisampling_)snapshotMultisampling_->recordResolve(*context.commands,context.currentFrame,context.currentFrame);
    if(instanceSnapshot_->buffers.outlineMaskImage){
        rhi::RenderPassBeginDesc mask{};mask.renderPass=outlineMaskPass_;
        mask.framebuffer=instanceSnapshot_->buffers.outlineMaskFramebuffer;mask.extent=instanceSnapshot_->buffers.outlineMaskExtent;
        mask.clearValues={VkClearValue{}};
        context.commands->beginRenderPass(mask);recordOutlineMask(context,*instanceSnapshot_,*context.commands);context.commands->endRenderPass();
    }
    recordMainPass(context);
}

void CharacterSceneRenderer::onUnload(const RenderContext& context) {
    temporalView_.clear();
    motionPoses_.clear();state_.temporalFrame=nullptr;
    (void)context;
    loadJobs_.clear();loadInitialized_=false;destroyResources();
    faceSdfHeadNode_.reset();faceSdfTextureCount_=0;
}

void CharacterSceneRenderer::appendHudText(std::ostringstream& text) const {
    std::array<std::size_t, 10> materialClassCounts{};
    const AssetMaterial* faceProfile = nullptr;
    const AssetMaterial* hairProfile = nullptr;
    for (const AssetMaterial& material : asset_.materials) {
        const std::size_t classIndex = static_cast<std::size_t>(
            material.materialClass);
        if (classIndex < materialClassCounts.size()) {
            ++materialClassCounts[classIndex];
        }
        if (material.materialClass == AssetMaterialClass::Face) {
            faceProfile = &material;
        } else if (material.materialClass == AssetMaterialClass::Hair) {
            hairProfile = &material;
        }
    }
    std::string animationName = "NONE";
    float animationDuration = 0.0F;
    float animationPlayhead = 0.0F;
    if (!asset_.animations.empty()) {
        const AssetAnimation& animation = asset_.animations[animationIndex_];
        animationName = animation.name.empty() ? "UNNAMED" : animation.name;
        animationDuration =
            std::max(animation.endTime - animation.startTime, 0.0F);
        if (animationDuration > 1.0e-8F) {
            animationPlayhead = std::fmod(
                std::max(animationTime_, 0.0F),
                animationDuration);
        }
    }
    text << "ANIM : " << animationName << "  "
         << std::fixed << std::setprecision(2)
         << animationPlayhead << '/' << animationDuration << " S  "
         << (animationPlaying_ ? "PLAYING" : "PAUSED") << '\n';
    text << "MAT V1: SKIN " << materialClassCounts[1]
         << " FACE " << materialClassCounts[2]
         << " HAIR " << materialClassCounts[3]
         << " FABRIC " << materialClassCounts[4]
         << " METAL " << materialClassCounts[5]
         << " EYE " << materialClassCounts[6]
         << " OVERLAY " << materialClassCounts[7] << '\n';
    if (faceProfile != nullptr && hairProfile != nullptr) {
        text << "MAT PARAM: FACE T" << faceProfile->styleParameters[0]
             << " S" << faceProfile->styleParameters[2]
             << " R" << faceProfile->styleParameters[3]
             << " | HAIR T" << hairProfile->styleParameters[0]
             << " S" << hairProfile->styleParameters[2]
             << " R" << hairProfile->styleParameters[3] << '\n';
    }
}

const RendererSceneState* CharacterSceneRenderer::sceneState() const noexcept {
    return &state_;
}

void CharacterSceneRenderer::restartPlayback() {
    animationTime_ = 0.0F;
    animationPlaying_ = true;
}

void CharacterSceneRenderer::setPlaybackPlaying(const bool playing) {
    animationPlaying_ = playing;
    if (playing) {
        animationTime_ = 0.0F;
    }
}

void CharacterSceneRenderer::appendCaptureManifestFields(
    std::ostream& json) const {
    const auto reportPrecision=json.precision();
    const std::string animationName = asset_.animations.empty()
        ? std::string()
        : asset_.animations[animationIndex_].name;
    std::size_t triangles=0,skinned=0,transparent=0;
    const auto& instances=instanceSnapshot_?instanceSnapshot_->instances:sceneInstances_;
    for(const auto& instance:instances){
        const auto& mesh=instance.meshKey==0?asset_:additionalResources_[instance.meshKey-1]->asset;
        triangles+=mesh.indices.size()/3;if(mesh.hasSkin)skinned+=mesh.vertices.size();
        for(const auto& primitive:mesh.primitives)if(mesh.materials[primitive.materialIndex].alphaMode==AssetAlphaMode::Blend)transparent+=primitive.indexCount/3;
    }
    json << "  \"uniqueMeshes\": " << additionalResources_.size()+1 << ",\n"
         << "  \"motionPreviousValidObjects\": " << motionPreviousValidObjects_ << ",\n"
         << "  \"motionOutputActive\": " << (motionPass_?"true":"false") << ",\n"
         << "  \"temporalMaterialSamples\": " << (temporalMaterialSamplesActive()?"true":"false") << ",\n"
         << "  \"ordinaryOpaquePipelineCount\": " << std::count_if(ordinaryOpaquePipelines_.begin(),ordinaryOpaquePipelines_.end(),[](auto p){return p!=VK_NULL_HANDLE;}) << ",\n"
         << "  \"irradianceResourceKind\": " << std::quoted(irradianceClipmapActive_?"clipmap":irradianceVolume_.cells.empty()?"none":"grid") << ",\n"
         << "  \"irradianceQueryScope\": \"physical-surface\",\n"
         << "  \"irradianceQuery\": {\"normalOffset\":" << std::setprecision(std::numeric_limits<float>::max_digits10) << scene_.irradianceVolume.sampling.normalOffset
         << ",\"jitterAmplitude\":" << scene_.irradianceVolume.sampling.jitterAmplitude
         << ",\"phase\":" << scene_.irradianceVolume.sampling.phase << "},\n" << std::setprecision(reportPrecision)
         << "  \"pixelIntegralPipelineCount\": " << (std::count_if(hairIntegralPipelines_.begin(),hairIntegralPipelines_.end(),[](auto p){return p!=VK_NULL_HANDLE;})+std::count_if(hairIntegralSnapshotPipelines_.begin(),hairIntegralSnapshotPipelines_.end(),[](auto p){return p!=VK_NULL_HANDLE;})) << ",\n"
         << "  \"temporalSamplingPattern\": " << std::quoted(kTemporalSamplingPattern) << ",\n"
         << "  \"motionResponseEncoding\": \"coverage-reactivity-v3\",\n"
         << "  \"temporalViewSubmission\": " << temporalView_.lastSubmission() << ",\n"
         << "  \"temporalCameraStateValid\": " << (temporalView_.committed()?"true":"false") << ",\n"
         << "  \"temporalViewSampleIndex\": " << (temporalView_.committed()?temporalView_.committed()->sampleIndex:0) << ",\n"
         << "  \"materialSurfaceProfileLayoutVersion\": " << PackedMaterialSurfaceProfile::kLayoutVersion << ",\n"
         << "  \"outlineProjectionLayoutVersion\": " << PackedOutlineProjectionRecord::kLayoutVersion << ",\n"
         << "  \"snapshotRasterizationSamples\": " << (snapshotMultisampling_?4:1) << ",\n"
         << "  \"snapshotMultisampleBytes\": " << (snapshotMultisampling_?snapshotMultisampling_->byteSize():0) << ",\n"
         << "  \"snapshotMultisampleAllocatedBytes\": " << (snapshotMultisampling_?snapshotMultisampling_->allocatedByteSize():0) << ",\n"
         << "  \"outlineMaskSamples\": " << rasterizationSamples_ << ",\n"
         << "  \"outlineProjectionBytes\": " << (outlineProjection_?outlineProjection_->buffer().size:0) << ",\n"
         << "  \"outlineProjectionMaterials\": " << (outlineProjection_?outlineProjection_->recordCount():0) << ",\n"
         << "  \"outlineProjectionCoordinates\": " << (outlineProjection_?outlineProjection_->coordinateCount():0) << ",\n"
         << "  \"instanceDataLayoutVersion\": " << SceneInstanceGpuData::kLayoutVersion << ",\n"
         << "  \"objectShadowLayoutVersion\": " << ObjectShadowGpuHeader::kLayoutVersion << ",\n"
         << "  \"objectShadowPrepared\": " << (objectShadowPrepared_?"true":"false") << ",\n"
         << "  \"objectShadowExactDepthBias\": " << (objectShadowPrepared_&&rhi_->supportsExactDepthBias()?"true":"false") << ",\n"
         << "  \"objectShadowPages\": " << (instanceSnapshot_?instanceSnapshot_->objectShadowPages.size():0) << ",\n"
         << "  \"objectShadowDepthBytes\": " << (objectShadowAtlas_&&objectShadowAtlas_->image().image?ObjectShadowAtlas::kDepthBytes:0) << ",\n"
         << "  \"materialTextureBindingLayoutVersion\": " << kTextureBindingLayoutVersion << ",\n"
         << "  \"sharedMaterialTextureSlots\": " << kSharedTextureSlots << ",\n"
         << "  \"lightBufferLayoutVersion\": " << RenderLightGpu::kLayoutVersion << ",\n"
         << "  \"faceSdfTextures\": " << faceSdfTextureCount_ << ",\n"
         << "  \"showcasePlatformPrepared\": " << (platformPrepared_?"true":"false") << ",\n"
         << "  \"sceneTriangles\": " << triangles << ",\n"
         << "  \"skinnedVertices\": " << skinned << ",\n"
         << "  \"transparentTriangles\": " << transparent << ",\n"
         << "  \"jointCapacity\": " << jointCapacity_ << ",\n"
         << "  \"vertexCapacity\": " << vertexCapacity_ << ",\n";
    const auto textureStats=materialTextureCache_?materialTextureCache_->statistics():SampledTextureCacheStatistics{};
    auto sourcePixelBytes=materialSourcePixelBytes(asset_);
    for(const auto& resource:additionalResources_)sourcePixelBytes+=materialSourcePixelBytes(resource->asset);
    json << "  \"materialTextureCache\": {\"requests\": " << textureStats.requests
         << ", \"hits\": " << textureStats.hits << ", \"uniqueTextures\": " << textureStats.uniqueTextures
         << ", \"requestedBytes\": " << textureStats.requestedBytes << ", \"residentBytes\": " << textureStats.residentBytes
         << ", \"retainedPixelBytes\": " << textureStats.retainedPixelBytes
         << ", \"hashedPixelBytes\": " << textureStats.hashedPixelBytes << ", \"copiedPixelBytes\": " << textureStats.copiedPixelBytes
         << ", \"transferredPixelBytes\": " << textureStats.transferredPixelBytes << ", \"discardedPixelBytes\": " << textureStats.discardedPixelBytes
         << ", \"hashMs\": " << textureStats.hashMs << ", \"matchMs\": " << textureStats.matchMs
         << ", \"mipMs\": " << textureStats.mipMs << ", \"uploadMs\": " << textureStats.uploadMs << "},\n";
    json << "  \"sceneLoad\": {\"cpuPrepareMs\": " << loadTimings_.cpuPrepareMs
         << ", \"meshUploadMs\": " << loadTimings_.meshUploadMs << ", \"materialUploadMs\": " << loadTimings_.materialUploadMs
         << ", \"sharedTexturesMs\": " << loadTimings_.sharedTexturesMs << ", \"buffersMs\": " << loadTimings_.buffersMs
         << ", \"skinningMs\": " << loadTimings_.skinningMs << ", \"descriptorsMs\": " << loadTimings_.descriptorsMs
         << ", \"pipelinesMs\": " << loadTimings_.pipelinesMs << ", \"decodedMaterialPixelBytes\": " << decodedMaterialPixelBytes_
         << ", \"remainingMaterialPixelBytes\": " << sourcePixelBytes << "},\n";
    json << "  \"animationIndex\": " << animationIndex_ << ",\n"
         << "  \"animation\": " << std::quoted(animationName) << ",\n";
}

void CharacterSceneRenderer::onAnimationKey(
    const int key,
    const int action) {
    if (action != GLFW_PRESS) {
        return;
    }
    if (key == GLFW_KEY_F4) {
        animationPlaying_ = !animationPlaying_;
    } else if (key == GLFW_KEY_F11) {
        animationTime_ = 0.0F;
        animationPlaying_ = true;
    } else if (key == GLFW_KEY_7 || key == GLFW_KEY_8) {
        if (asset_.animations.empty()) {
            return;
        }
        const std::size_t count = asset_.animations.size();
        animationIndex_ = key == GLFW_KEY_7
            ? (animationIndex_ + count - 1) % count
            : (animationIndex_ + 1) % count;
        animationTime_ = 0.0F;
    } else if (key == GLFW_KEY_9) {
        const std::string& animationName = asset_.animations.empty()
            ? "none"
            : asset_.animations[animationIndex_].name;
        azurerender::RuntimeDiagnostics::instance().print(
            "input",
            "Animation: " + animationName + " index "
                + std::to_string(animationIndex_) + " time "
                + std::to_string(animationTime_) + "s "
                + (animationPlaying_ ? "playing" : "paused"));
    }
}

// ---------------------------------------------------------------------------
// Resource creation
// ---------------------------------------------------------------------------

void CharacterSceneRenderer::createMeshBuffers(
    LoadedAsset& asset,
    rhi::GpuBuffer& vertexBuffer,
    rhi::GpuBuffer& indexBuffer) {
    const VkDeviceSize vertexSize =
        sizeof(AssetVertex) * asset.vertices.size();
    rhi::GpuBuffer vertexStaging = allocator_->createBuffer(
        vertexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
    std::memcpy(
        vertexStaging.mapped,
        asset.vertices.data(),
        static_cast<std::size_t>(vertexSize));
    vertexBuffer = allocator_->createBuffer(
        vertexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        false);
    rhi_->copyBuffer(vertexStaging, vertexBuffer, vertexSize);
    allocator_->destroyBuffer(vertexStaging);

    const VkDeviceSize indexSize =
        sizeof(std::uint32_t) * asset.indices.size();
    rhi::GpuBuffer indexStaging = allocator_->createBuffer(
        indexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
    std::memcpy(
        indexStaging.mapped,
        asset.indices.data(),
        static_cast<std::size_t>(indexSize));
    indexBuffer = allocator_->createBuffer(
        indexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        false);
    rhi_->copyBuffer(indexStaging, indexBuffer, indexSize);
    allocator_->destroyBuffer(indexStaging);
}

void CharacterSceneRenderer::createVertexBuffer() {
    createMeshBuffers(asset_, vertexBuffer_, indexBuffer_);
}

void CharacterSceneRenderer::createIndexBuffer() {
}

template <typename Pixels>
void CharacterSceneRenderer::uploadTextureData(
    const Pixels& pixels,
    const std::uint32_t width,
    const std::uint32_t height,
    const VkFormat format,
    const bool clampVertical,
    GpuTexture& texture,
    const std::uint32_t requestedMipLevels,TextureFilterSemantic semantic,float alphaCutoff,const TextureImportSettings* importSettings) {
    std::vector<std::uint8_t> filtered;unsigned mipLevels=requestedMipLevels;
    if(!rhi_->supportsSampledImage(format,!importSettings||importSettings->filter=="linear"))throw std::runtime_error("Device does not support the decoded texture sampling format");
    if constexpr(sizeof(typename std::decay_t<Pixels>::value_type)==1){
        const auto baseBytes=std::size_t(width)*height*4;
        if(requestedMipLevels>1&&pixels.size()>baseBytes){
            std::size_t expected=0;unsigned w=width,h=height;for(unsigned level=0;level<requestedMipLevels;++level){expected+=std::size_t(w)*h*4;w=std::max(w/2,1U);h=std::max(h/2,1U);}
            if(pixels.size()!=expected)throw std::invalid_argument("Authored texture mip storage is invalid");filtered.assign(pixels.begin(),pixels.end());
        }else if(requestedMipLevels==1&&pixels.size()>baseBytes)filtered.assign(pixels.begin(),pixels.begin()+baseBytes);
        else if(requestedMipLevels==0){auto chain=buildTextureMipChain(pixels,width,height,semantic,alphaCutoff,!importSettings||importSettings->filter=="linear");filtered=std::move(chain.pixels);mipLevels=chain.levels;}
    }
    bool authoredHalfMips=false;
    if constexpr(sizeof(typename std::decay_t<Pixels>::value_type)==2) {
        if(requestedMipLevels>1&&pixels.size()>std::size_t(width)*height*4) {
            std::size_t expected=0;unsigned w=width,h=height;
            for(unsigned level=0;level<requestedMipLevels;++level){expected+=std::size_t(w)*h*4;w=std::max(w/2,1U);h=std::max(h/2,1U);}
            if(pixels.size()!=expected)throw std::invalid_argument("Authored HDR mip storage is invalid");
            authoredHalfMips=true;
        }
    }
    const bool cpuMips=!filtered.empty();
    const bool suppliedMips=cpuMips||authoredHalfMips;
    const VkDeviceSize size = static_cast<VkDeviceSize>(cpuMips?filtered.size():pixels.size())
        * sizeof(typename std::decay_t<Pixels>::value_type);
    rhi::GpuBuffer staging = allocator_->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
    std::memcpy(
        staging.mapped, cpuMips?static_cast<const void*>(filtered.data()):static_cast<const void*>(pixels.data()), static_cast<std::size_t>(size));
    texture.image = allocator_->createImage2D(
        width,
        height,
        format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            | (mipLevels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0)
            | (computeIblEnabled_
                   && mipLevels > 1
                   && format == VK_FORMAT_R16G16B16A16_SFLOAT
                ? VK_IMAGE_USAGE_STORAGE_BIT
                : 0),
        mipLevels);
    rhi_->transitionImageLayout(
        texture.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        mipLevels);
    rhi_->copyBufferToImage(staging, texture.image, width, height,suppliedMips?mipLevels:1,4*sizeof(typename std::decay_t<Pixels>::value_type));
    if (mipLevels > 1 && !suppliedMips) {
        rhi_->generateMipmaps(texture.image, format, width, height, mipLevels);
    } else {
        rhi_->transitionImageLayout(
            texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            mipLevels);
    }
    allocator_->destroyBuffer(staging);
    texture.view = rhi_->createImageView(
        texture.image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
    rhi::SamplerDesc samplerDesc{};
    samplerDesc.addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerDesc.addressV = clampVertical
        ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerDesc.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerDesc.maxLod = static_cast<float>(mipLevels - 1);
    samplerDesc.maxAnisotropy = clampVertical?1.0F:8.0F;
    if(importSettings)samplerDesc=importSettings->sampler(mipLevels,16);
    texture.sampler = rhi_->createSampler(samplerDesc);
}

void CharacterSceneRenderer::uploadMaterialTextures(LoadedAsset& asset,std::vector<GpuMaterial>& gpuMaterials){
    gpuMaterials.resize(asset.materials.size());
    for(std::size_t index=0;index<asset.materials.size();++index)for(unsigned slot=0;slot<kMaterialTextureSlots;++slot)uploadMaterialTexture(asset,gpuMaterials,index,slot);
}
void CharacterSceneRenderer::uploadMaterialTexture(LoadedAsset& asset,std::vector<GpuMaterial>& materials,std::size_t index,unsigned slot){
    auto& m=asset.materials.at(index);auto& g=materials.at(index);
    const auto& settings=m.textureSettings.at(slot);
    const auto mipLevels=settings.mipPolicy=="none"?1U:m.authoredMipLevels.at(slot);
    const auto colorFormat=settings.colorSpace=="srgb"?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM;
    const auto upload=[&](std::vector<std::uint8_t>& pixels,unsigned width,unsigned height,
                         SampledTextureBinding& binding,unsigned levels,float cutoff=0) {
        if(!materialTextureCache_)throw std::logic_error("Material texture cache is not initialized");
        SampledTextureUpload request;request.width=width;request.height=height;request.format=colorFormat;
        request.mipLevels=levels;request.semantic=settings.semantic();request.alphaCutoff=cutoff;
        request.sampler=settings.sampler(1,16);request.sampler.maxLod=VK_LOD_CLAMP_NONE;
        if(pixels.size()>=16U*1024U*1024U)RuntimeDiagnostics::instance().info("render",
            "Scene material texture begin: "+m.name+", slot="+std::to_string(slot)+", bytes="+std::to_string(pixels.size()));
        binding=materialTextureCache_->acquire(std::move(pixels),request);
    };
    switch(slot){
    case 0:upload(m.baseColorPixels,m.baseColorWidth,m.baseColorHeight,g.baseColor,mipLevels,m.alphaMode==AssetAlphaMode::Mask?m.alphaCutoff:0);break;
    case 1:upload(m.normalPixels,m.normalWidth,m.normalHeight,g.normal,mipLevels);break;
    case 2:upload(m.metallicRoughnessPixels,m.metallicRoughnessWidth,m.metallicRoughnessHeight,g.metallicRoughness,mipLevels);break;
    case 3:upload(m.specularEmissivePixels,m.specularEmissiveWidth,m.specularEmissiveHeight,g.specularEmissive,mipLevels);break;
    case 4:upload(m.styleMaskPixels,m.styleMaskWidth,m.styleMaskHeight,g.styleMask,mipLevels);break;
    case 5:upload(m.matcapPixels,m.matcapWidth,m.matcapHeight,g.matcap,mipLevels);break;
    case 6:upload(m.hairDataPixels,m.hairDataWidth,m.hairDataHeight,g.hairData,mipLevels);break;
    case 7:{if(m.projectedHairRamp.present){auto& h=m.projectedHairRamp;upload(h.parameterPixels,h.parameterWidth,h.parameterHeight,g.faceSdf,mipLevels);}
        else if(faceSdfPrepared_&&m.faceSdf.present){upload(m.faceSdf.pixels,m.faceSdf.width,m.faceSdf.height,g.faceSdf,mipLevels);++faceSdfTextureCount_;}break;}
    case 8:if(m.projectedHairRamp.line.present){auto& line=m.projectedHairRamp.line;upload(line.pixels,line.width,line.height,g.shadowColorLut,mipLevels);}
        else upload(m.shadowColorLutPixels,m.shadowColorLutWidth,m.shadowColorLutHeight,g.shadowColorLut,1);break;
    default:throw std::runtime_error("Invalid material upload slot");
    }
}

void CharacterSceneRenderer::createTexture(bool uploadMaterials) {

    const auto loadPpmTexture = [](const std::string& path) {
        std::ifstream stream(std::filesystem::u8path(path));
        if (!stream) {
            throw std::runtime_error("Could not open toon-ramp atlas: " + path);
        }
        std::string magic;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t maximum = 0;
        stream >> magic >> width >> height >> maximum;
        if (magic != "P3" || width < 8 || height != 10 || maximum != 255) {
            throw std::runtime_error("Invalid P3 toon-ramp atlas: " + path);
        }
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width) * height * 4);
        for (std::size_t pixel = 0; pixel < pixels.size() / 4; ++pixel) {
            std::uint32_t red = 0;
            std::uint32_t green = 0;
            std::uint32_t blue = 0;
            if (!(stream >> red >> green >> blue)
                || red > maximum || green > maximum || blue > maximum) {
                throw std::runtime_error("Invalid toon-ramp pixel data: " + path);
            }
            pixels[pixel * 4 + 0] = static_cast<std::uint8_t>(red);
            pixels[pixel * 4 + 1] = static_cast<std::uint8_t>(green);
            pixels[pixel * 4 + 2] = static_cast<std::uint8_t>(blue);
            pixels[pixel * 4 + 3] = 255;
        }
        std::string trailing;
        if (stream >> trailing) {
            throw std::runtime_error("Unexpected trailing toon-ramp data: " + path);
        }
        struct LoadedPpm {
            std::vector<std::uint8_t> pixels;
            std::uint32_t width;
            std::uint32_t height;
        };
        return LoadedPpm{std::move(pixels), width, height};
    };

    if(uploadMaterials)uploadMaterialTextures(asset_, gpuMaterials_);

    constexpr std::uint32_t kEnvironmentWidth = 512;
    constexpr std::uint32_t kEnvironmentHeight = 256;
    constexpr float kPi = 3.14159265358979323846F;
    std::vector<std::uint16_t> environmentPixels;
    std::uint32_t environmentWidth = kEnvironmentWidth;
    std::uint32_t environmentHeight = kEnvironmentHeight;
    std::uint32_t authoredEnvironmentMips = 0;
    if (!environmentSource_.path.empty()) {
        const EnvironmentImage environment =
            loadEnvironmentImage(environmentSource_);
        if(environment.projection==EnvironmentProjection::CubeFaces)
            throw std::invalid_argument("Global environment requires an equirectangular source");
        environmentPixels = environment.rgba16f;
        environmentWidth = environment.width;
        environmentHeight = environment.height;
        authoredEnvironmentMips = environment.authoredMipLevels;
        azurerender::RuntimeDiagnostics::instance().print(
            "asset",
            "Environment: " + environment.description + " ("
                + std::to_string(environmentWidth) + "x"
                + std::to_string(environmentHeight) + ")");
    } else {
        environmentPixels.resize(
            static_cast<std::size_t>(kEnvironmentWidth)
            * kEnvironmentHeight
            * 4);
        environmentWidth = kEnvironmentWidth;
        environmentHeight = kEnvironmentHeight;
        const Vector3 sunDirection = {0.45F, 0.85F, 0.35F};
        const float sunLength = std::sqrt(dot(sunDirection, sunDirection));
        const Vector3 normalizedSun = {
            sunDirection[0] / sunLength,
            sunDirection[1] / sunLength,
            sunDirection[2] / sunLength,
        };
        for (std::uint32_t y = 0; y < kEnvironmentHeight; ++y) {
            const float v =
                (static_cast<float>(y) + 0.5F)
                / static_cast<float>(kEnvironmentHeight);
            const float theta = v * kPi;
            const float directionY = std::cos(theta);
            const float ringRadius = std::sin(theta);
            for (std::uint32_t x = 0; x < kEnvironmentWidth; ++x) {
                const float u =
                    (static_cast<float>(x) + 0.5F)
                    / static_cast<float>(kEnvironmentWidth);
                const float phi = (u - 0.5F) * 2.0F * kPi;
                const Vector3 direction = {
                    ringRadius * std::cos(phi),
                    directionY,
                    ringRadius * std::sin(phi),
                };
                const float skyAmount =
                    std::clamp(directionY * 0.5F + 0.5F, 0.0F, 1.0F);
                const float horizon = std::exp(-std::abs(directionY) * 9.0F);
                const float sun = std::pow(
                    std::max(dot(direction, normalizedSun), 0.0F),
                    320.0F);
                const std::array<float, 3> ground = {0.055F, 0.075F, 0.090F};
                const std::array<float, 3> zenith = {0.20F, 0.34F, 0.46F};
                const std::array<float, 3> horizonColor = {0.38F, 0.43F, 0.46F};
                const float sunIntensity = 24.0F * sun;
                const std::size_t pixel =
                    (static_cast<std::size_t>(y) * kEnvironmentWidth + x) * 4;
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    float color =
                        ground[channel] * (1.0F - skyAmount)
                        + zenith[channel] * skyAmount;
                    color = color * (1.0F - horizon * 0.55F)
                        + horizonColor[channel] * horizon * 0.55F;
                    color += sunIntensity * (channel == 2 ? 0.70F : 1.0F);
                    environmentPixels[pixel + channel] =
                        environmentFloatToHalf(std::clamp(color, 0.0F, 32.0F));
                }
                environmentPixels[pixel + 3] = environmentFloatToHalf(1.0F);
            }
        }
    }
    uploadTextureData(
        environmentPixels,
        environmentWidth,
        environmentHeight,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        true,
        environmentTexture_,
        authoredEnvironmentMips ? authoredEnvironmentMips : kEnvironmentMipLevels);
    if (authoredEnvironmentMips) {
        azurerender::RuntimeDiagnostics::instance().info("render", "Environment: preserved authored HDR mip chain; runtime prefilter skipped");
    } else if (computeIblEnabled_) {
        createEnvironmentPrefilter();
    } else {
        azurerender::RuntimeDiagnostics::instance().info(
            "render", "Character IBL: hardware mip fallback path");
    }

    scene_.reflectionProbe.validate();
    EnvironmentImage probeImage;
    if(!scene_.reflectionProbe.texture.empty()) {
        probeImage=loadEnvironmentImage({scene_.reflectionProbe.texture,EnvironmentProjection::Auto});
        RuntimeDiagnostics::instance().info("render","Local reflection probe: "+probeImage.description);
    } else {
        probeImage.width=probeImage.height=1;probeImage.authoredMipLevels=1;
        probeImage.projection=EnvironmentProjection::CubeFaces;probeImage.rgba16f.resize(24);
        for(unsigned face=0;face<6;++face)probeImage.rgba16f[face*4+3]=0x3c00;
    }
    reflectionProbeTexture_=std::make_unique<EnvironmentCubeTexture>(*rhi_,std::move(probeImage));
    scene_.irradianceVolume.validate();
    irradianceVolume_={};
    irradianceClipmap_.reset();irradianceClipmapDimensions_={1,1,1};
    std::vector<std::array<float,4>> irradianceWords(4);
    if(irradianceClipmapActive_){
        auto field=IrradianceClipmap::load(std::filesystem::u8path(scene_.irradianceVolume.asset));
        const auto bytes=sizeof(field.shaderParameters());
        if(irradianceBufferBudget_&&bytes>irradianceBufferBudget_)
            throw std::runtime_error("Irradiance clipmap parameters exceed device storage buffer range");
        irradianceClipmapDimensions_=field.dimensions;
        irradianceClipmap_=std::make_unique<IrradianceClipmapGpu>(*rhi_,field);
        RuntimeDiagnostics::instance().info("render","Irradiance clipmap: 3 layers, 6 volume textures");
    }else if(!scene_.irradianceVolume.asset.empty()){
        irradianceVolume_=IrradianceVolume::load(std::filesystem::u8path(scene_.irradianceVolume.asset));
        irradianceWords.resize(irradianceVolume_.cells.size()*4);
        for(std::size_t i=0;i<irradianceVolume_.cells.size();++i){
            const auto& cell=irradianceVolume_.cells[i];
            for(unsigned c=0;c<3;++c)irradianceWords[4*i+c]=cell.coefficients[c];
            irradianceWords[4*i+3]={cell.weight,0,0,0};
        }
        RuntimeDiagnostics::instance().info("render","Irradiance volume: "+std::to_string(irradianceVolume_.cells.size())+" samples");
    }
    if(!irradianceClipmapActive_){
    const auto irradianceBytes=irradianceWords.size()*sizeof(irradianceWords[0]);
    if(irradianceBufferBudget_&&irradianceBytes>irradianceBufferBudget_)
        throw std::runtime_error("Irradiance volume exceeds device storage buffer range");
    irradianceBuffer_=allocator_->createBuffer(irradianceBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(irradianceBuffer_.mapped,irradianceWords.data(),irradianceBytes);
    allocator_->flush(irradianceBuffer_,0,irradianceBytes);
    }
    const auto toonRamp = loadPpmTexture(rampAtlasPath_);
    uploadTextureData(
        toonRamp.pixels,
        toonRamp.width,
        toonRamp.height,
        VK_FORMAT_R8G8B8A8_UNORM,
        true,
        toonRampTexture_);
}

void CharacterSceneRenderer::createEnvironmentPrefilter() {
    if (environmentTexture_.image.image == VK_NULL_HANDLE
        || environmentTexture_.view == VK_NULL_HANDLE
        || kEnvironmentMipLevels < 2) {
        return;
    }
    constexpr VkShaderStageFlags kCompute = VK_SHADER_STAGE_COMPUTE_BIT;
    iblPrefilterSetLayout_ = rhi_->createDescriptorSetLayout({
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kCompute},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, kCompute},
    });
    const std::uint32_t prefilterCount = kEnvironmentMipLevels - 1;
    rhi::DescriptorPoolDesc pool{};
    pool.sizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, prefilterCount},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, prefilterCount},
    };
    pool.maxSets = prefilterCount;
    iblPrefilterPool_ = rhi_->createDescriptorPool(pool);
    iblPrefilterSets_ = rhi_->allocateDescriptorSets(
        iblPrefilterPool_, iblPrefilterSetLayout_, prefilterCount);

    environmentBaseMipView_ = rhi_->createImageView(
        environmentTexture_.image.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1,
        0);
    environmentPrefilterViews_.reserve(prefilterCount);
    for (std::uint32_t level = 1; level < kEnvironmentMipLevels; ++level) {
        const VkImageView destinationView = rhi_->createImageView(
            environmentTexture_.image.image,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            level);
        environmentPrefilterViews_.push_back(destinationView);
        const std::size_t setIndex = level - 1;

        rhi::DescriptorImageWrite source{};
        source.set = iblPrefilterSets_[setIndex];
        source.binding = 0;
        source.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        source.view = environmentBaseMipView_;
        source.sampler = environmentTexture_.sampler;
        source.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rhi_->writeDescriptorImage(source);

        rhi::DescriptorImageWrite destination{};
        destination.set = iblPrefilterSets_[setIndex];
        destination.binding = 1;
        destination.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        destination.view = destinationView;
        destination.layout = VK_IMAGE_LAYOUT_GENERAL;
        rhi_->writeDescriptorImage(destination);
    }

    const auto code = azurerender::readBinaryFile(
        shaderDirectory_ + "/ibl_prefilter.comp.spv");
    const VkShaderModule shader = rhi_->createShaderModule(code);
    const rhi::PushConstantRangeDesc pushRange{
        VK_SHADER_STAGE_COMPUTE_BIT,
        sizeof(IblPrefilterPushConstants),
    };
    iblPrefilterPipelineLayout_ =
        rhi_->createPipelineLayout(iblPrefilterSetLayout_, &pushRange);
    iblPrefilterPipeline_ = rhi_->createComputePipeline(
        {shader, iblPrefilterPipelineLayout_});
    rhi_->destroyShaderModule(shader);

    rhi_->executeOneShot([this](rhi::ICommandRecorder& commands) {
        for (std::uint32_t level = 1;
             level < kEnvironmentMipLevels;
             ++level) {
            rhi::ImageBarrierDesc toStorage{};
            toStorage.image = environmentTexture_.image.image;
            toStorage.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toStorage.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toStorage.mipLevels = 1;
            toStorage.baseMipLevel = level;
            toStorage.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            toStorage.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            toStorage.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toStorage.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            commands.imageBarrier(toStorage);

            commands.bindComputePipeline(iblPrefilterPipeline_);
            commands.bindComputeDescriptorSet(
                iblPrefilterPipelineLayout_,
                iblPrefilterSets_[level - 1]);
            const IblPrefilterPushConstants parameters{
                static_cast<float>(level)
                    / static_cast<float>(kEnvironmentMipLevels - 1),
                kIblPrefilterSamples,
            };
            commands.pushConstants(
                iblPrefilterPipelineLayout_,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                &parameters,
                sizeof(parameters));
            const VkExtent3D extent = {
                std::max(environmentTexture_.image.width >> level, 1U),
                std::max(environmentTexture_.image.height >> level, 1U),
                1,
            };
            commands.dispatch(
                (extent.width + 7) / 8,
                (extent.height + 7) / 8,
                1);

            rhi::ImageBarrierDesc toSampled{};
            toSampled.image = environmentTexture_.image.image;
            toSampled.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            toSampled.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toSampled.mipLevels = 1;
            toSampled.baseMipLevel = level;
            toSampled.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            toSampled.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            toSampled.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            toSampled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            commands.imageBarrier(toSampled);
        }
    });
    azurerender::RuntimeDiagnostics::instance().info(
        "render", "Character IBL: GPU-prefiltered environment mip chain");
}

void CharacterSceneRenderer::createAdditionalResources() {
    const scene::ResourceLayout layout(scene_);
    if (layout.paths.size() <= 1) {
        return;
    }
    std::uint32_t jointBase =
        static_cast<std::uint32_t>(asset_.jointMatrices.size());
    std::uint32_t textureBase =
        kSharedTextureSlots
        + kMaterialTextureSlots
            * static_cast<std::uint32_t>(asset_.materials.size());
    std::size_t globalMaterialBase = asset_.materials.size();
    for (std::size_t index = 1; index < layout.paths.size(); ++index) {
        auto resource = std::make_unique<AdditionalResource>();
        resource->path = layout.paths[index];
        resource->asset = loadGltfAsset(resource->path);
        applyMaterialBindings(resource->asset,layout.materials.at(index));
        createMeshBuffers(
            resource->asset, resource->vertexBuffer, resource->indexBuffer);
        uploadMaterialTextures(resource->asset, resource->gpuMaterials);
        resource->jointBase = jointBase;
        resource->textureBase = textureBase;
        resource->globalMaterialBase = globalMaterialBase;
        jointBase += static_cast<std::uint32_t>(
            resource->asset.jointMatrices.size());
        textureBase += kMaterialTextureSlots
            * static_cast<std::uint32_t>(resource->asset.materials.size());
        globalMaterialBase += resource->asset.materials.size();
        additionalResources_.push_back(std::move(resource));
    }
    azurerender::RuntimeDiagnostics::instance().print(
        "asset",
        "Additional scene resources: "
            + std::to_string(additionalResources_.size())
            + ", total joints " + std::to_string(jointBase));
}

void CharacterSceneRenderer::createComputeSkinningResources() {
    meshResourceCount_ =
        static_cast<std::uint32_t>(additionalResources_.size() + 1);
    std::vector<std::uint32_t> morphWords{0,0,0};
    morphGpuMeshes_.clear();
    for(std::uint32_t key=0;key<meshResourceCount_;++key)
        morphGpuMeshes_.push_back(appendMorphGpuMesh(morphWords,key==0?asset_:additionalResources_[key-1]->asset));
    morphTargetBuffer_=allocator_->createBuffer(morphWords.size()*sizeof(std::uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(morphTargetBuffer_.mapped,morphWords.data(),morphWords.size()*sizeof(std::uint32_t));
    allocator_->flush(morphTargetBuffer_,0,morphTargetBuffer_.size);
    for(auto& buffer:morphWeightBuffers_){buffer=allocator_->createBuffer(sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memset(buffer.mapped,0,sizeof(float));allocator_->flush(buffer,0,buffer.size);}
    if (!computeSkinningEnabled_) {
        azurerender::RuntimeDiagnostics::instance().info(
            "render", "Character skinning and morph: vertex-shader fallback path");
        return;
    }
    const auto createOutputBuffers = [this](
        const VkDeviceSize byteSize,
        std::vector<rhi::GpuBuffer>& buffers) {
        buffers.resize(kMaxFramesInFlight);
        for (rhi::GpuBuffer& buffer : buffers) {
            buffer = allocator_->createBuffer(
                byteSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                false);
        }
    };
    createOutputBuffers(static_cast<VkDeviceSize>(vertexCapacity_) * sizeof(AssetVertex), skinnedVertexBuffers_);

    skinningDescriptorSetLayout_ = rhi_->createDescriptorSetLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    });
    const std::uint32_t descriptorSetCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight) * meshResourceCount_;
    rhi::DescriptorPoolDesc descriptorPool{};
    descriptorPool.sizes = {{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        descriptorSetCount * 5,
    }};
    descriptorPool.maxSets = descriptorSetCount;
    skinningDescriptorPool_ = rhi_->createDescriptorPool(descriptorPool);
    skinningDescriptorSets_ = rhi_->allocateDescriptorSets(
        skinningDescriptorPool_, skinningDescriptorSetLayout_, descriptorSetCount);

    const auto shaderCode = azurerender::readBinaryFile(
        shaderDirectory_ + "/skin.comp.spv");
    const VkShaderModule shader = rhi_->createShaderModule(shaderCode);
    const rhi::PushConstantRangeDesc pushRange{
        VK_SHADER_STAGE_COMPUTE_BIT,
        sizeof(SkinningPushConstants),
    };
    skinningPipelineLayout_ =
        rhi_->createPipelineLayout(skinningDescriptorSetLayout_, &pushRange);
    skinningPipeline_ = rhi_->createComputePipeline(
        {shader, skinningPipelineLayout_});
    rhi_->destroyShaderModule(shader);

    const auto writeSet = [this](const std::size_t frame,
        const std::uint32_t meshKey,
        const rhi::GpuBuffer& source,
        const rhi::GpuBuffer& destination) {
        const std::size_t setIndex =
            frame * meshResourceCount_ + meshKey;
        const VkDescriptorSet set = skinningDescriptorSets_[setIndex];
        const std::array<rhi::DescriptorBufferWrite, 5> writes = {{
            {set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             source.buffer, source.size},
            {set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             jointBuffers_[frame].buffer, jointBuffers_[frame].size},
            {set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             destination.buffer, destination.size},
            {set,3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphTargetBuffer_.buffer,morphTargetBuffer_.size},
            {set,4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphWeightBuffers_[frame].buffer,morphWeightBuffers_[frame].size},
        }};
        for (const rhi::DescriptorBufferWrite& write : writes) {
            rhi_->writeDescriptorBuffer(write);
        }
    };
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        writeSet(frame, 0, vertexBuffer_, skinnedVertexBuffers_[frame]);
        for (std::size_t index = 0; index < additionalResources_.size(); ++index) {
            AdditionalResource& resource = *additionalResources_[index];
            writeSet(
                frame,
                static_cast<std::uint32_t>(index + 1),
                resource.vertexBuffer,
                skinnedVertexBuffers_[frame]);
        }
    }
    azurerender::RuntimeDiagnostics::instance().info(
        "render", "Character skinning and morph: Vulkan compute path");
}

void CharacterSceneRenderer::recordComputeSkinningMesh(
    const RenderContext& context,
    const std::uint32_t meshKey, rhi::ICommandRecorder* recorder,
    const std::vector<SkinningDispatch>* dispatches) {
    auto& commands = recorder != nullptr ? *recorder : *context.commands;
    const LoadedAsset& mesh = meshKey == 0
        ? asset_
        : additionalResources_[meshKey - 1]->asset;
    const std::size_t setIndex = context.currentFrame * meshResourceCount_ + meshKey;
    for (const auto& dispatchItem : dispatches ? *dispatches : skinningDispatches_) {
        if (dispatchItem.meshKey != meshKey) continue;
        const auto& parameters=dispatchItem.parameters;
        commands.pushConstants(skinningPipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, &parameters, sizeof(parameters));
        ComputePass dispatch({static_cast<std::uint32_t>(mesh.vertices.size()), parameters.instanceCount, 64, 1, 1, true});
        dispatch.record(commands, skinningPipeline_, skinningPipelineLayout_, skinningDescriptorSets_[setIndex]);
    }

}

void CharacterSceneRenderer::recordComputeSkinning(
    const RenderContext& context) {
    if (!computeSkinningEnabled_) {
        return;
    }
    for (std::uint32_t meshKey = 0; meshKey < meshResourceCount_; ++meshKey) {
        recordComputeSkinningMesh(context, meshKey);
    }
    const auto& output=skinnedVertexBuffers_[context.currentFrame];
    context.commands->bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
}

void CharacterSceneRenderer::createUniformBuffers() {
    // One typed, immutable record per stable asset material index.
    std::vector<PackedMaterialSurfaceProfile> profiles;
    const auto appendProfiles=[&](const LoadedAsset& mesh) {
        for(const auto& material:mesh.materials)profiles.push_back(packMaterialSurfaceProfile(material));
    };
    appendProfiles(asset_);for(const auto& resource:additionalResources_)appendProfiles(resource->asset);
    const auto profileBytes=sizeof(PackedMaterialSurfaceProfile)*profiles.size();
    if(profiles.empty()||profiles.size()>std::numeric_limits<std::uint32_t>::max()/2
        ||(irradianceBufferBudget_&&profileBytes>irradianceBufferBudget_))
        throw std::runtime_error("Material surface table exceeds the device storage budget");
    materialSurfaceProfiles_=allocator_->createBuffer(profileBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(materialSurfaceProfiles_.mapped,profiles.data(),profileBytes);
    allocator_->flush(materialSurfaceProfiles_,0,profileBytes);
    const VkDeviceSize size = sizeof(UniformBufferObject);
    uniformBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        uniformBuffers_[index] = allocator_->createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            true);
    }
    const VkDeviceSize lightBytes =
        sizeof(RenderLightGpu) * kMaxSceneLights;
    const VkDeviceSize clusterHeaderBytes =
        sizeof(std::array<std::uint32_t, 2>)
        * kClusterGridX * kClusterGridY * kClusterGridZ;
    const VkDeviceSize clusterIndexBytes =
        sizeof(std::uint32_t) * kMaxClusterLightIndices;
    lightBuffers_.resize(kMaxFramesInFlight);
    clusterHeaderBuffers_.resize(kMaxFramesInFlight);
    clusterIndexBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        lightBuffers_[index] = allocator_->createBuffer(
            lightBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        clusterHeaderBuffers_[index] = allocator_->createBuffer(
            clusterHeaderBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        clusterIndexBuffers_[index] = allocator_->createBuffer(
            clusterIndexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    }
    materialOriginNodes_.clear();
    materialOriginNodes_.push_back(materialOriginNodes(asset_));
    for(const auto& resource:additionalResources_)materialOriginNodes_.push_back(materialOriginNodes(resource->asset));
    std::uint64_t originRecords=0;
    if(std::any_of(materialOriginNodes_.begin(),materialOriginNodes_.end(),[](const auto& nodes){return !nodes.empty();})) {
        const scene::ResourceLayout layout(scene_);
        for(const auto& node:scene_.nodes) {
            const auto key=layout.meshKey(node.resourceId);
            if(key<materialOriginNodes_.size())originRecords+=materialOriginNodes_[key].size()
                *(scene_.nodes.size()==1?std::uint64_t(qaInstanceCount_):1U);
        }
    } else materialOriginNodes_.clear();
    const auto capacity=std::max<std::size_t>(qaInstanceCount_,scene_.nodes.size())+originRecords;
    if(capacity>std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Material origin frame buffer exceeds GPU indexing");
    if(originRecords&&irradianceBufferBudget_&&capacity*sizeof(InstanceGpuData)>irradianceBufferBudget_)
        throw std::length_error("Material origin instances exceed the device storage range");
    instanceCapacity_=static_cast<std::uint32_t>(capacity);
    instanceBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        instanceBuffers_[index] = allocator_->createBuffer(
            static_cast<VkDeviceSize>(instanceCapacity_)
                * sizeof(InstanceGpuData),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            true);
    }
}

void CharacterSceneRenderer::createJointBuffers() {
    if (asset_.jointMatrices.empty()) {
        throw std::runtime_error("Asset has no joint-matrix fallback");
    }
    jointCapacity_ = 0;
    vertexCapacity_ = 0;
    std::vector<std::uint32_t> instanceCounts(additionalResources_.size() + 1,
        scene_.nodes.size() == 1 ? qaInstanceCount_ : 0U);
    if (scene_.nodes.size() != 1) {
        const scene::ResourceLayout layout(scene_);
        for (const auto& node : scene_.nodes) {
            const auto key = layout.meshKey(node.resourceId);
            if (key < instanceCounts.size()) ++instanceCounts[key];
        }
    }
    for (std::size_t key = 0; key <= additionalResources_.size(); ++key) {
        const auto& mesh = key == 0 ? asset_ : additionalResources_[key-1]->asset;
        const auto count = instanceCounts[key];
        jointCapacity_ += static_cast<std::uint32_t>(mesh.jointMatrices.size()) * std::max(count, 1U);
        vertexCapacity_ += static_cast<std::uint32_t>(mesh.vertices.size()) * std::max(count, 1U);
    }
    jointBuffers_.resize(kMaxFramesInFlight);
    for (auto& buffer : jointBuffers_) buffer = allocator_->createBuffer(
        sizeof(std::array<float,16>) * static_cast<VkDeviceSize>(jointCapacity_), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);

}

void CharacterSceneRenderer::createOitIndexBuffers() {
    std::size_t totalIndices = 0;
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode
            == AssetAlphaMode::Blend) {
            totalIndices += primitive.indexCount;
        }
    }
    oitIndexBufferSize_ = totalIndices * sizeof(std::uint32_t);
    if (oitIndexBufferSize_ == 0) {
        return;
    }
    oitIndexBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        oitIndexBuffers_[index] = allocator_->createBuffer(
            oitIndexBufferSize_,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            true);
    }
}

void CharacterSceneRenderer::createOverlayDepth(const RenderContext& context) {
    rasterizationSamples_=context.rasterizationSamples;
    outlineMaskViewExtent_=context.renderExtent;
    outlineMaskMaxDimension_=context.descriptorLimits.maxImageDimension2D;
    rhi::RenderPassDesc maskPass;
    maskPass.attachments={{VK_FORMAT_R8_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false}};
    maskPass.attachments[0].samples=context.rasterizationSamples;
    maskPass.externalReadDependency=true;
    outlineMaskPass_=rhi_->createRenderPass(maskPass);
    rhi::RenderPassDesc pass;pass.attachments={{context.sceneColorFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
        {VK_FORMAT_R32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
        {context.depthFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,false,true}};
    pass.depthAttachment=2;pass.externalReadDependency=true;overlayDepthPass_=rhi_->createRenderPass(pass);
    for(std::size_t frame=0;frame<kMaxFramesInFlight;++frame){
        auto& color=opaqueSceneColor_[frame];
        color.image=allocator_->createImage2D(context.renderExtent.width,context.renderExtent.height,context.sceneColorFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        color.view=rhi_->createImageView(color.image.image,context.sceneColorFormat,VK_IMAGE_ASPECT_COLOR_BIT,1);
        auto& target=overlaySceneDepth_[frame];
        target.image=allocator_->createImage2D(context.renderExtent.width,context.renderExtent.height,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        target.view=rhi_->createImageView(target.image.image,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        target.sampler=rhi_->createSampler(sampler);
        auto& depth=overlayDepthAttachments_[frame];depth=allocator_->createImage2D(context.renderExtent.width,context.renderExtent.height,context.depthFormat,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        overlayDepthViews_[frame]=rhi_->createImageView(depth.image,context.depthFormat,VK_IMAGE_ASPECT_DEPTH_BIT,1);
        overlayDepthFramebuffers_[frame]=rhi_->createFramebuffer({overlayDepthPass_,{color.view,target.view,overlayDepthViews_[frame]},context.renderExtent.width,context.renderExtent.height});
    }
    if(context.rasterizationSamples==VK_SAMPLE_COUNT_4_BIT){
        std::vector<SceneMultisampleDestination> destinations;
        for(unsigned frame=0;frame<kMaxFramesInFlight;++frame)
            destinations.push_back({{opaqueSceneColor_[frame].image.image,overlayDepthAttachments_[frame].image,overlaySceneDepth_[frame].image.image},
                {opaqueSceneColor_[frame].view,overlayDepthViews_[frame],overlaySceneDepth_[frame].view}});
        snapshotMultisampling_=std::make_unique<SceneMultisampling>(*rhi_,*allocator_,context.renderExtent,context.depthFormat,VK_FORMAT_R32_SFLOAT,
            SceneSecondaryResolve::Minimum,static_cast<std::uint32_t>(kMaxFramesInFlight),std::move(destinations),shaderDirectory_);
    }
}
void CharacterSceneRenderer::destroyOverlayDepth(){
    snapshotMultisampling_.reset();
    for(std::size_t i=0;i<kMaxFramesInFlight;++i){
        if(outlineMaskFramebuffers_[i])rhi_->destroyFramebuffer(outlineMaskFramebuffers_[i]);outlineMaskFramebuffers_[i]=VK_NULL_HANDLE;
        auto& mask=outlineMasks_[i];if(mask.view)rhi_->destroyImageView(mask.view);allocator_->destroyImage(mask.image);mask={};
        if(overlayDepthFramebuffers_[i])rhi_->destroyFramebuffer(overlayDepthFramebuffers_[i]);overlayDepthFramebuffers_[i]=VK_NULL_HANDLE;
        if(overlayDepthViews_[i])rhi_->destroyImageView(overlayDepthViews_[i]);overlayDepthViews_[i]=VK_NULL_HANDLE;
        allocator_->destroyImage(overlayDepthAttachments_[i]);
        auto& color=opaqueSceneColor_[i];if(color.view)rhi_->destroyImageView(color.view);allocator_->destroyImage(color.image);color={};
        auto& t=overlaySceneDepth_[i];if(t.view)rhi_->destroyImageView(t.view);if(t.sampler)rhi_->destroySampler(t.sampler);allocator_->destroyImage(t.image);t={};
    }
    if(overlayDepthPass_)rhi_->destroyRenderPass(overlayDepthPass_);overlayDepthPass_=VK_NULL_HANDLE;
    if(outlineMaskPass_)rhi_->destroyRenderPass(outlineMaskPass_);outlineMaskPass_=VK_NULL_HANDLE;
}
void CharacterSceneRenderer::prepareOutlineMaskResources(const std::vector<std::vector<float>>& opacities,RecordingBufferSet& buffers) {
    std::vector<std::uint32_t> masked;
    std::vector<bool> visible(sceneInstances_.size(),false);
    for(const auto& span:visibleSpansByMeshKey_)
        for(std::uint32_t index=span[1];index<span[1]+span[2];++index)visible[index]=true;
    for(const auto& instance:sceneInstances_){
        if(!visible[instance.sourceIndex])continue;
        const auto& mesh=instance.meshKey==0?asset_:additionalResources_[instance.meshKey-1]->asset;
        bool needed=false;
        for(const auto& primitive:mesh.primitives){
            const auto& material=mesh.materials[primitive.materialIndex];
            if(material.alphaMode==AssetAlphaMode::Blend||material.materialClass==AssetMaterialClass::Overlay||material.showcasePlatform>.5F)continue;
            const bool explicitOutline=(material.materialFeatures&MaterialFeatureSilhouetteOutline)!=0;
            if((renderSettings_->sceneLightingEnabled||!renderSettings_->silhouetteOutlineEnabled)
                &&!explicitOutline)continue;
            forEachPrimitiveOpacityRun(opacities,primitive.nodeIndex,instance.sourceIndex,1,[&](const PrimitiveOpacityRun& run){
                needed|=run.opacity>0&&(explicitOutline||run.opacity<1);
            });
        }
        if(needed)masked.push_back(instance.sourceIndex);
    }
    auto layout=makeOutlineMaskLayout(sceneInstances_.size(),masked,
        outlineMaskViewExtent_.width,outlineMaskViewExtent_.height,outlineMaskMaxDimension_,128ULL*1024*1024/rasterizationSamples_);
    // Multisample shader descriptors always receive a matching image type.
    if(!layout.width&&rasterizationSamples_==VK_SAMPLE_COUNT_4_BIT)layout.width=layout.height=1;
    auto& mask=outlineMasks_[currentFrame_];
    if(mask.image.width!=layout.width||mask.image.height!=layout.height){
        // updateFrame runs only after this frame slot's fence has retired.
        if(outlineMaskFramebuffers_[currentFrame_])rhi_->destroyFramebuffer(outlineMaskFramebuffers_[currentFrame_]);
        outlineMaskFramebuffers_[currentFrame_]=VK_NULL_HANDLE;
        if(mask.view)rhi_->destroyImageView(mask.view);
        allocator_->destroyImage(mask.image);mask={};
        if(layout.width){
            VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;
            image.extent={layout.width,layout.height,1};image.format=VK_FORMAT_R8_UNORM;image.mipLevels=image.arrayLayers=1;
            image.samples=rasterizationSamples_;image.tiling=VK_IMAGE_TILING_OPTIMAL;
            image.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
            mask.image=allocator_->createImage(image,false);
            mask.view=rhi_->createImageView(mask.image.image,VK_FORMAT_R8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);
            outlineMaskFramebuffers_[currentFrame_]=rhi_->createFramebuffer({outlineMaskPass_,{mask.view},layout.width,layout.height});
            RuntimeDiagnostics::instance().info("render","Outline mask allocated: frame="+std::to_string(currentFrame_)
                +", instances="+std::to_string(masked.size())+", bytes="+std::to_string(std::uint64_t{layout.width}*layout.height*rasterizationSamples_));
        }
    }
    const auto& fallback=gpuMaterials_[0].baseColor;
    const auto view=mask.view?mask.view:fallback.view;
    const auto sampler=mask.view?overlaySceneDepth_[currentFrame_].sampler:fallback.sampler;
    const auto first=bindlessTextures_?currentFrame_:currentFrame_*totalMaterialCount();
    const auto count=bindlessTextures_?1:totalMaterialCount();
    for(std::size_t index=0;index<count;++index)
        rhi_->writeDescriptorImage({descriptorSets_[first+index],26,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,view,sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    buffers.outlineMaskImage=mask.image.image;
    buffers.outlineMaskFramebuffer=outlineMaskFramebuffers_[currentFrame_];
    buffers.outlineMaskExtent={layout.width,layout.height};
    buffers.outlineMaskTiles=layout.instanceTiles;
}

void CharacterSceneRenderer::recordOutlineMask(const RenderContext& context,const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands) {
    if(bindlessTextures_)commands.bindDescriptorSet(pipelineLayout_,descriptorSets_[context.currentFrame]);
    const auto columns=snapshot.buffers.outlineMaskExtent.width/context.renderExtent.width;
    for(const auto& span:snapshot.visibleSpans){
        const auto key=span[0];
        const auto& mesh=key==0?asset_:additionalResources_[key-1]->asset;
        const auto textureBase=key==0?kSharedTextureSlots:additionalResources_[key-1]->textureBase;
        const auto materialBase=key==0?0:additionalResources_[key-1]->globalMaterialBase;
        commands.bindVertexBuffer(snapshot.buffers.vertices.at(key),0);commands.bindIndexBuffer(snapshot.buffers.indices.at(key),0);
        for(std::uint32_t instance=span[1];instance<span[1]+span[2];++instance){
            const auto tile=snapshot.buffers.outlineMaskTiles.at(instance);
            if(tile==std::numeric_limits<std::uint32_t>::max())continue;
            const VkOffset2D offset{static_cast<std::int32_t>((tile%columns)*context.renderExtent.width),static_cast<std::int32_t>((tile/columns)*context.renderExtent.height)};
            commands.setViewport(static_cast<float>(context.renderExtent.width),static_cast<float>(context.renderExtent.height),static_cast<float>(offset.x),static_cast<float>(offset.y));
            commands.setScissor(context.renderExtent,offset);
            for(const auto& primitive:mesh.primitives){
                const auto& m=mesh.materials[primitive.materialIndex];
                if(m.alphaMode==AssetAlphaMode::Blend||m.materialClass==AssetMaterialClass::Overlay||m.showcasePlatform>.5F)continue;
                forEachPrimitiveOpacityRun(snapshot.nodeWorldOpacities,primitive.nodeIndex,instance,1,[&](const PrimitiveOpacityRun& run){
                    if(run.opacity==0)return;
                    const bool mirrored=primitiveMirroredTransform(snapshot.instances.at(instance).model,
                        snapshot.gizmo,&primitive-mesh.primitives.data());
                    commands.bindPipeline(m.doubleSided?outlineMaskDoubleSidedPipeline_
                        :mirrored?outlineMaskMirroredPipeline_:outlineMaskPipeline_);
                    const auto set=bindlessTextures_?context.currentFrame:context.currentFrame*totalMaterialCount()+materialBase+primitive.materialIndex;
                    if(!bindlessTextures_)commands.bindDescriptorSet(pipelineLayout_,descriptorSets_[set]);
                    MaterialPushConstants material{m.alphaCutoff,static_cast<std::uint32_t>(m.alphaMode),m.emissiveStrength,m.showcasePlatform,
                        m.aoColor,m.lamShadowColor,m.materialClass==AssetMaterialClass::Hair?m.hairViewParameters:m.matcapColor,
                        m.hairParameters,m.styleParameters,m.featureParameters,static_cast<unsigned>(m.materialClass),materialFeaturesForFrame(m),m.materialProfileVersion,
                        static_cast<std::uint32_t>((materialBase+primitive.materialIndex)<<1U)};
                    MorphPushConstants morph{};morph.weights=snapshot.settings.morphWeights;
                    morph.gizmoTransform=primitiveGizmoTransform(snapshot.gizmo,&primitive-mesh.primitives.data());
                    morph.textureBaseIndex=textureBase+primitive.materialIndex*kMaterialTextureSlots;
                    commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&material,sizeof(material));
                    commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(material),&morph,sizeof(morph));
                    commands.drawIndexed(primitive.indexCount,primitive.firstIndex,1,instance);
                });
            }
        }
    }
}

void CharacterSceneRenderer::recordOverlayDepth(const RenderContext& context,const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands){
    bool depthShell=false;
    bool colorRequired=screenSpaceReflectionActive(snapshot.settings.screenSpaceReflection,
        snapshot.settings.sceneLightingEnabled,qaIsolationMode_,!(qaEffectMode_==5&&!qaEffectEnabled_));
    bool sampled=colorRequired;
    if(snapshot.settings.sceneLightingEnabled&&snapshot.settings.nprLighting.enabled&&!(qaEffectMode_==4&&!qaEffectEnabled_)) {
        const auto& lights=frameLights_.lights();
        sampled|=std::any_of(lights.begin(),lights.end(),[](const RenderLight& light){
            const auto& response=light.shape.characterResponse;
            return response.present&&(response.specialMode==LightShape::CharacterResponse::SpecialMode::ScreenRim
                ||response.specialMode==LightShape::CharacterResponse::SpecialMode::MaterialRim)
                &&light.shape.receivers.characters&&light.intensity>0;
        });
    }
    for(const auto& span:snapshot.visibleSpans){
        const auto& mesh=span[0]==0?asset_:additionalResources_[span[0]-1]->asset;
        for(const auto& material:mesh.materials) {
            sampled|=(material.materialFeatures&(MaterialFeatureBrowOverlay|MaterialFeatureSceneTint|MaterialFeatureSceneRefraction))!=0;
            colorRequired|=(material.materialFeatures&(MaterialFeatureSceneRefraction|MaterialFeatureSceneTint))!=0;
            depthShell|=material.outlineSurface.present&&material.outlineSurface.depthPrepass;
        }
    }
    if(!sampled&&!depthShell)return;
    if(colorRequired&&context.depthTestEnabled)
        recordDepthCoverage(context,snapshot,commands,DepthCoverageMode::SnapshotPrefill);
    recordDepthCoverage(context,snapshot,commands,colorRequired?DepthCoverageMode::SnapshotColor:DepthCoverageMode::SnapshotDepth);
    if(depthShell)recordOutlineShells(context,snapshot,commands,true);
}

void CharacterSceneRenderer::recordDepthCoverage(const RenderContext& context,const SceneInstanceSnapshot& snapshot,
    rhi::ICommandRecorder& commands,DepthCoverageMode mode,SceneSubmissionCounters* counters){
    const bool mainPrefill=mode==DepthCoverageMode::MainPrefill;
    const bool prefill=mainPrefill||mode==DepthCoverageMode::SnapshotPrefill;
    const bool colorRequired=mode==DepthCoverageMode::SnapshotColor;
    const auto& prefillPipelines=mainPrefill?mainPrefillPipelines_:snapshotPrefillPipelines_;
    commands.setViewport(static_cast<float>(context.renderExtent.width),static_cast<float>(context.renderExtent.height));commands.setScissor(context.renderExtent);
    if(bindlessTextures_){
        commands.bindDescriptorSet(pipelineLayout_,descriptorSets_[context.currentFrame]);
        if(counters)++counters->descriptorSetBinds;
    }
    for(const auto& span:snapshot.visibleSpans){
        const auto key=span[0],first=span[1],count=span[2];const auto& mesh=key==0?asset_:additionalResources_[key-1]->asset;
        const auto textureBase=key==0?kSharedTextureSlots:additionalResources_[key-1]->textureBase;
        const auto materialBase=key==0?0:additionalResources_[key-1]->globalMaterialBase;
        commands.bindVertexBuffer(snapshot.buffers.vertices.at(key),0);commands.bindIndexBuffer(snapshot.buffers.indices.at(key),0);
        for(const auto& primitive:mesh.primitives){
            const auto& m=mesh.materials[primitive.materialIndex];if(m.alphaMode==AssetAlphaMode::Blend||m.materialClass==AssetMaterialClass::Overlay)continue;
            if(prefill&&m.alphaMode!=AssetAlphaMode::Opaque)continue;
            if(mainPrefill&&(m.materialFeatures&(MaterialFeatureSceneTint|MaterialFeatureMultiplyComposite))!=0)continue;
            if(m.showcasePlatform>.5F&&!snapshot.settings.characterPresentation.platformEnabled)continue;
            const bool mirrored=primitiveMirroredTransform(snapshot.instances.at(first).model,
                snapshot.gizmo,&primitive-mesh.primitives.data());
            // Radial eyes can transform the alpha-test UV before coverage.
            const bool simpleDepth=!colorRequired&&!(m.alphaMode==AssetAlphaMode::Mask
                &&(m.materialFeatures&MaterialFeatureEyeRadialSurface)!=0);
            commands.bindPipeline(prefill?prefillPipelines[m.doubleSided?2:mirrored?1:0]
                :simpleDepth?snapshotDepthOnlyPipelines_[m.doubleSided?2:mirrored?1:0]
                :(m.materialFeatures&MaterialFeatureHairPixelIntegral)?hairIntegralPipeline(m,mirrored,true)
                :m.doubleSided?overlayDepthDoubleSidedPipeline_:mirrored?overlayDepthMirroredPipeline_:overlayDepthPipeline_);
            const auto set=bindlessTextures_?context.currentFrame:context.currentFrame*totalMaterialCount()+materialBase+primitive.materialIndex;
            if(!bindlessTextures_){commands.bindDescriptorSet(pipelineLayout_,descriptorSets_[set]);if(counters)++counters->descriptorSetBinds;}
            if(counters)++counters->pipelineBinds;
            MaterialPushConstants material{m.alphaCutoff,static_cast<std::uint32_t>(m.alphaMode),m.emissiveStrength,m.showcasePlatform,
                m.aoColor,m.lamShadowColor,m.materialClass==AssetMaterialClass::Hair?m.hairViewParameters:m.matcapColor,
                m.hairParameters,m.styleParameters,m.featureParameters,static_cast<unsigned>(m.materialClass),materialFeaturesForFrame(m),m.materialProfileVersion,
                static_cast<std::uint32_t>((materialBase+primitive.materialIndex)<<1U)};
            commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&material,sizeof(material));
            if(counters)++counters->pushConstantUpdates;
            MorphPushConstants morph{};morph.weights=snapshot.settings.morphWeights;morph.textureBaseIndex=textureBase+primitive.materialIndex*kMaterialTextureSlots;
            morph.gizmoTransform=primitiveGizmoTransform(snapshot.gizmo,&primitive-mesh.primitives.data());
            forEachPrimitiveOpacityRun(snapshot.nodeWorldOpacities,primitive.nodeIndex,first,count,[&](const PrimitiveOpacityRun& run) {
                if (run.opacity == 0) return;
                morph.nodeOpacity=run.opacity;
                commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(material),&morph,sizeof(morph));
                commands.drawIndexed(primitive.indexCount,primitive.firstIndex,run.instanceCount,
                    materialOriginFirstInstance(m,&snapshot,run.firstInstance,run.instanceCount));
                if(counters){++counters->pushConstantUpdates;++counters->drawCalls;}
            });
        }
    }
}

#include "CharacterOutlinePass.inc"
#include "CharacterMotionPass.inc"
#include "CharacterMaterialSampling.inc"

void CharacterSceneRenderer::createDescriptorPool() {
    rhi::DescriptorPoolDesc poolDesc{};
    if (bindlessTextures_) {
        const std::uint32_t frameCount =
            static_cast<std::uint32_t>(kMaxFramesInFlight);
        const std::uint32_t textureSlots =
            kSharedTextureSlots
            + kMaterialTextureSlots
                * static_cast<std::uint32_t>(totalMaterialCount());
        poolDesc.sizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             frameCount * (textureSlots + 4 + (irradianceClipmapActive_?6U:0U) + (outlineProjectionEnabled_?static_cast<std::uint32_t>(totalMaterialCount()):0))},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, frameCount*(objectShadowPrepared_?2U:1U)},
            {VK_DESCRIPTOR_TYPE_SAMPLER, frameCount},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * (outlineProjectionEnabled_?12U:11U)},
        };
        poolDesc.maxSets = frameCount;
        descriptorPool_ = rhi_->createDescriptorPool(poolDesc);
        return;
    }
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight * totalMaterialCount());
    poolDesc.sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * ((outlineProjectionEnabled_?16U:15U)+(irradianceClipmapActive_?6U:0U))},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descriptorCount*(objectShadowPrepared_?2U:1U)},
        {VK_DESCRIPTOR_TYPE_SAMPLER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount * (outlineProjectionEnabled_?12U:11U)},
    };
    poolDesc.maxSets = descriptorCount;
    descriptorPool_ = rhi_->createDescriptorPool(poolDesc);
}

void CharacterSceneRenderer::createDescriptorSetLayout() {
    if (bindlessTextures_) {
        const std::uint32_t textureSlots =
            kSharedTextureSlots
            + kMaterialTextureSlots
                * static_cast<std::uint32_t>(totalMaterialCount());
        std::vector<rhi::DescriptorBindingDesc> bindings={
            {0,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             1,
             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
            {1,
             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             textureSlots,
             VK_SHADER_STAGE_FRAGMENT_BIT},
            {9,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {28,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {10,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             1,
             VK_SHADER_STAGE_VERTEX_BIT},
            {13,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             1,
             VK_SHADER_STAGE_VERTEX_BIT},
            {14,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             1,
             VK_SHADER_STAGE_FRAGMENT_BIT},
            {15,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             1,
             VK_SHADER_STAGE_FRAGMENT_BIT},
            {16,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             1,
             VK_SHADER_STAGE_FRAGMENT_BIT},
            {17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
            {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
            {19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
            {20,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {21,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT},
            {22,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT},
            {23,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {24,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {26,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {27,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
        };
        if(irradianceClipmapActive_)bindings.push_back({32,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6,VK_SHADER_STAGE_FRAGMENT_BIT});
        if(objectShadowPrepared_)bindings.push_back({29,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_FRAGMENT_BIT});
        if(outlineProjectionEnabled_) {
            bindings.push_back({30,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT});
            bindings.push_back({31,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,static_cast<std::uint32_t>(totalMaterialCount()),VK_SHADER_STAGE_VERTEX_BIT});
        }
        descriptorSetLayout_=rhi_->createDescriptorSetLayout(bindings);
        return;
    }
    constexpr VkShaderStageFlags kFragment = VK_SHADER_STAGE_FRAGMENT_BIT;
    std::vector<rhi::DescriptorBindingDesc> bindings={
        {0,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, kFragment},
        {28, VK_DESCRIPTOR_TYPE_SAMPLER, 1, kFragment},
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kFragment},
        {15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kFragment},
        {16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kFragment},
        {17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {20,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,kFragment},
        {21,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT},
        {22,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT},
        {23,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,kFragment},
        {24,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,kFragment},
        {25,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,kFragment},
        {26,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,kFragment},
        {27,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,kFragment},
    };
    if(irradianceClipmapActive_)bindings.push_back({32,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6,kFragment});
    if(objectShadowPrepared_)bindings.push_back({29,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,kFragment});
    if(outlineProjectionEnabled_) {
        bindings.push_back({30,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT});
        bindings.push_back({31,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_VERTEX_BIT});
    }
    descriptorSetLayout_=rhi_->createDescriptorSetLayout(bindings);
}

void CharacterSceneRenderer::createDescriptorSets() {
    objectShadowBoundViews_.fill(objectShadowPrepared_&&objectShadowAtlas_->view()?objectShadowAtlas_->view():shadowImageView_);
    const auto writeProjection=[this](VkDescriptorSet set,std::size_t material) {
        if(!outlineProjectionEnabled_)return;
        const auto& buffer=outlineProjection_->buffer();
        rhi_->writeDescriptorBuffer({set,30,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffer.buffer,buffer.size});
        const auto& masks=outlineProjection_->masks();
        if(bindlessTextures_) {
            rhi::DescriptorImageArrayWrite write{};write.set=set;write.binding=31;
            for(const auto& mask:masks)write.elements.push_back({mask.sampler,mask.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorImageArray(write);
        }else {
            const auto& mask=masks.at(material);
            rhi_->writeDescriptorImage({set,31,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,mask.view,mask.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }
    };
    const auto& irradianceData=irradianceClipmap_?irradianceClipmap_->parameters():irradianceBuffer_;
    const auto writeSceneImages=[this](VkDescriptorSet set){
        if(irradianceClipmap_)irradianceClipmap_->writeTextures(set,32);
        rhi_->writeDescriptorImage({set,9,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,shadowImageView_,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
        rhi_->writeDescriptorImage({set,28,VK_DESCRIPTOR_TYPE_SAMPLER,VK_NULL_HANDLE,shadowSampler_,VK_IMAGE_LAYOUT_UNDEFINED});
        if(objectShadowPrepared_)rhi_->writeDescriptorImage({set,29,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            objectShadowAtlas_->view()?objectShadowAtlas_->view():shadowImageView_,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
    };
    if (bindlessTextures_) {
        descriptorSets_ = rhi_->allocateDescriptorSets(
            descriptorPool_,
            descriptorSetLayout_,
            static_cast<std::uint32_t>(kMaxFramesInFlight));

        // One array write per frame set: shared slots first, then the
        // per-material blocks in shader-visible order.
        std::vector<VkDescriptorImageInfo> images;
        const auto slots=static_cast<std::uint32_t>(kSharedTextureSlots + kMaterialTextureSlots * totalMaterialCount());
        images.reserve(slots);
        // Own descriptor values; the renderer retains native texture owners
        // until its host has retired all frame fences.
        ResourceIndexTable<VkDescriptorImageInfo> bindings(slots);
        const auto appendInfo = [&images,&bindings](VkDescriptorImageInfo info) {
            const auto handle=bindings.insert(std::make_shared<const VkDescriptorImageInfo>(info));
            if(handle.slot!=images.size()) throw std::logic_error("Texture block slot order changed");
            images.push_back(*bindings.acquire(handle,0).resource);
        };
        const auto appendTexture = [&appendInfo](const auto& texture) {
            VkDescriptorImageInfo info{};
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.imageView = texture.view;
            info.sampler = texture.sampler;
            appendInfo(info);
        };
        appendTexture(environmentTexture_);
        appendTexture(toonRampTexture_);
        for (const GpuMaterial& material : gpuMaterials_) {
            appendTexture(material.baseColor);
            appendTexture(material.normal);
            appendTexture(material.metallicRoughness);
            appendTexture(material.specularEmissive);
            appendTexture(material.styleMask);
            appendTexture(material.matcap);
            appendTexture(material.hairData);
            appendTexture(material.faceSdf.view!=VK_NULL_HANDLE?material.faceSdf:material.normal);
            appendTexture(material.shadowColorLut);
        }
        for (const auto& resource : additionalResources_) {
            for (const GpuMaterial& material : resource->gpuMaterials) {
                appendTexture(material.baseColor);
                appendTexture(material.normal);
                appendTexture(material.metallicRoughness);
                appendTexture(material.specularEmissive);
                appendTexture(material.styleMask);
                appendTexture(material.matcap);
                appendTexture(material.hairData);
                appendTexture(material.faceSdf.view!=VK_NULL_HANDLE?material.faceSdf:material.normal);
                appendTexture(material.shadowColorLut);
            }
        }

        for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
            rhi::DescriptorBufferWrite uniformWrite{};
            uniformWrite.set = descriptorSets_[frame];
            writeSceneImages(uniformWrite.set);
            writeProjection(uniformWrite.set,0);
            rhi_->writeDescriptorBuffer({descriptorSets_[frame],27,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,materialSurfaceProfiles_.buffer,materialSurfaceProfiles_.size});
            rhi_->writeDescriptorImage({descriptorSets_[frame],26,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,gpuMaterials_[0].baseColor.view,gpuMaterials_[0].baseColor.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorBuffer({descriptorSets_[frame],24,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,irradianceData.buffer,irradianceData.size});
            const auto& probe=*reflectionProbeTexture_;
            rhi_->writeDescriptorImage({descriptorSets_[frame],23,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,probe.view(),probe.sampler(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorImage({descriptorSets_[frame],18,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,overlaySceneDepth_[frame].view,overlaySceneDepth_[frame].sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorImage({descriptorSets_[frame],19,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,opaqueSceneColor_[frame].view,overlaySceneDepth_[frame].sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorBuffer({descriptorSets_[frame],20,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,shadowBounds_[frame]->buffer().buffer,shadowBounds_[frame]->buffer().size});
            uniformWrite.binding = 0;
            uniformWrite.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformWrite.buffer = uniformBuffers_[frame].buffer;
            uniformWrite.range = sizeof(UniformBufferObject);
            rhi_->writeDescriptorBuffer(uniformWrite);

            rhi::DescriptorImageArrayWrite arrayWrite{};
            arrayWrite.set = descriptorSets_[frame];
            arrayWrite.binding = 1;
            arrayWrite.elements = images;
            rhi_->writeDescriptorImageArray(arrayWrite);

            rhi::DescriptorBufferWrite jointWrite{};
            jointWrite.set = descriptorSets_[frame];
            jointWrite.binding = 10;
            jointWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            jointWrite.buffer = jointBuffers_[frame].buffer;
            jointWrite.range = jointBuffers_[frame].size;
            rhi_->writeDescriptorBuffer(jointWrite);
            const auto& output = computeSkinningEnabled_ ? skinnedVertexBuffers_[frame] : vertexBuffer_;
            rhi_->writeDescriptorBuffer({jointWrite.set, 17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, output.buffer, output.size});
            rhi_->writeDescriptorBuffer({jointWrite.set,21,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphTargetBuffer_.buffer,morphTargetBuffer_.size});
            rhi_->writeDescriptorBuffer({jointWrite.set,22,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphWeightBuffers_[frame].buffer,morphWeightBuffers_[frame].size});

            rhi::DescriptorBufferWrite instanceWrite{};
            instanceWrite.set = descriptorSets_[frame];
            instanceWrite.binding = 13;
            instanceWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            instanceWrite.buffer = instanceBuffers_[frame].buffer;
            instanceWrite.range = static_cast<VkDeviceSize>(instanceCapacity_)
                * sizeof(InstanceGpuData);
            rhi_->writeDescriptorBuffer(instanceWrite);

            const std::array<const rhi::GpuBuffer*, 3> frameBuffers = {
                &lightBuffers_[frame],
                &clusterHeaderBuffers_[frame],
                &clusterIndexBuffers_[frame],
            };
            for (std::size_t bufferIndex = 0;
                 bufferIndex < frameBuffers.size();
                 ++bufferIndex) {
                rhi::DescriptorBufferWrite write{};
                write.set = descriptorSets_[frame];
                write.binding = 14 + static_cast<std::uint32_t>(bufferIndex);
                write.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.buffer = frameBuffers[bufferIndex]->buffer;
                write.range = frameBuffers[bufferIndex]->size;
                rhi_->writeDescriptorBuffer(write);
            }
        }
        return;
    }
    const std::size_t descriptorCount =
        kMaxFramesInFlight * totalMaterialCount();
    descriptorSets_ = rhi_->allocateDescriptorSets(
        descriptorPool_,
        descriptorSetLayout_,
        static_cast<std::uint32_t>(descriptorCount));

    const auto writeImage = [this](
        const std::size_t descriptorIndex,
        const std::uint32_t binding,
        const VkImageView view,
        const VkSampler sampler,
        const VkImageLayout layout) {
        rhi::DescriptorImageWrite write{};
        write.set = descriptorSets_[descriptorIndex];
        write.binding = binding;
        write.view = view;
        write.sampler = sampler;
        write.layout = layout;
        rhi_->writeDescriptorImage(write);
    };
    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        std::size_t globalMaterial = 0;
        const auto writeMaterialSet = [&](const GpuMaterial& gpuMaterial) {
            const std::size_t descriptorIndex =
                frame * totalMaterialCount() + globalMaterial;
            ++globalMaterial;

            rhi::DescriptorBufferWrite uniformWrite{};
            uniformWrite.set = descriptorSets_[descriptorIndex];
            writeSceneImages(uniformWrite.set);
            writeProjection(uniformWrite.set,globalMaterial-1);
            rhi_->writeDescriptorBuffer({descriptorSets_[descriptorIndex],27,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,materialSurfaceProfiles_.buffer,materialSurfaceProfiles_.size});
            rhi_->writeDescriptorImage({descriptorSets_[descriptorIndex],26,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,gpuMaterials_[0].baseColor.view,gpuMaterials_[0].baseColor.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorBuffer({descriptorSets_[descriptorIndex],24,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,irradianceData.buffer,irradianceData.size});
            const auto& probe=*reflectionProbeTexture_;
            rhi_->writeDescriptorImage({descriptorSets_[descriptorIndex],23,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,probe.view(),probe.sampler(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorImage({descriptorSets_[descriptorIndex],18,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,overlaySceneDepth_[frame].view,overlaySceneDepth_[frame].sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorImage({descriptorSets_[descriptorIndex],19,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,opaqueSceneColor_[frame].view,overlaySceneDepth_[frame].sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            rhi_->writeDescriptorBuffer({descriptorSets_[descriptorIndex],20,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,shadowBounds_[frame]->buffer().buffer,shadowBounds_[frame]->buffer().size});
            uniformWrite.binding = 0;
            uniformWrite.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformWrite.buffer = uniformBuffers_[frame].buffer;
            uniformWrite.range = sizeof(UniformBufferObject);
            rhi_->writeDescriptorBuffer(uniformWrite);

            writeImage(
                descriptorIndex,
                1,
                gpuMaterial.baseColor.view,
                gpuMaterial.baseColor.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                2,
                gpuMaterial.normal.view,
                gpuMaterial.normal.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                3,
                gpuMaterial.metallicRoughness.view,
                gpuMaterial.metallicRoughness.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                4,
                environmentTexture_.view,
                environmentTexture_.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                5,
                gpuMaterial.specularEmissive.view,
                gpuMaterial.specularEmissive.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                6,
                gpuMaterial.styleMask.view,
                gpuMaterial.styleMask.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                7,
                gpuMaterial.matcap.view,
                gpuMaterial.matcap.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                8,
                gpuMaterial.hairData.view,
                gpuMaterial.hairData.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                11,
                toonRampTexture_.view,
                toonRampTexture_.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                12,
                gpuMaterial.faceSdf.view!=VK_NULL_HANDLE?gpuMaterial.faceSdf.view:gpuMaterial.normal.view,
                gpuMaterial.faceSdf.view!=VK_NULL_HANDLE?gpuMaterial.faceSdf.sampler:gpuMaterial.normal.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            writeImage(descriptorIndex,25,gpuMaterial.shadowColorLut.view,gpuMaterial.shadowColorLut.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            rhi::DescriptorBufferWrite jointWrite{};
            jointWrite.set = descriptorSets_[descriptorIndex];
            jointWrite.binding = 10;
            jointWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            jointWrite.buffer = jointBuffers_[frame].buffer;
            jointWrite.range = jointBuffers_[frame].size;
            rhi_->writeDescriptorBuffer(jointWrite);
            const auto& output = computeSkinningEnabled_ ? skinnedVertexBuffers_[frame] : vertexBuffer_;
            rhi_->writeDescriptorBuffer({jointWrite.set, 17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, output.buffer, output.size});
            rhi_->writeDescriptorBuffer({jointWrite.set,21,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphTargetBuffer_.buffer,morphTargetBuffer_.size});
            rhi_->writeDescriptorBuffer({jointWrite.set,22,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,morphWeightBuffers_[frame].buffer,morphWeightBuffers_[frame].size});

            rhi::DescriptorBufferWrite instanceWrite{};
            instanceWrite.set = descriptorSets_[descriptorIndex];
            instanceWrite.binding = 13;
            instanceWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            instanceWrite.buffer = instanceBuffers_[frame].buffer;
            instanceWrite.range = static_cast<VkDeviceSize>(instanceCapacity_)
                * sizeof(InstanceGpuData);
            rhi_->writeDescriptorBuffer(instanceWrite);

            const std::array<const rhi::GpuBuffer*, 3> frameBuffers = {
                &lightBuffers_[frame],
                &clusterHeaderBuffers_[frame],
                &clusterIndexBuffers_[frame],
            };
            for (std::size_t bufferIndex = 0;
                 bufferIndex < frameBuffers.size();
                 ++bufferIndex) {
                rhi::DescriptorBufferWrite write{};
                write.set = descriptorSets_[descriptorIndex];
                write.binding = 14 + static_cast<std::uint32_t>(bufferIndex);
                write.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.buffer = frameBuffers[bufferIndex]->buffer;
                write.range = frameBuffers[bufferIndex]->size;
                rhi_->writeDescriptorBuffer(write);
            }
        };
        for (const GpuMaterial& gpuMaterial : gpuMaterials_) {
            writeMaterialSet(gpuMaterial);
        }
        for (const auto& resource : additionalResources_) {
            for (const GpuMaterial& gpuMaterial : resource->gpuMaterials) {
                writeMaterialSet(gpuMaterial);
            }
        }
    }
}


void CharacterSceneRenderer::createGraphicsPipeline(
    const RenderContext& context) {
    const std::string shaderDirectory = context.shaderDirectory;
    rasterizationSamples_=context.rasterizationSamples;
    const auto mainFragment=[&](std::string file,bool material=false){
        return azurerender::readBinaryFile(shaderDirectory+sceneFragmentVariant(std::move(file),
            rasterizationSamples_==VK_SAMPLE_COUNT_4_BIT,material&&irradianceClipmapActive_));
    };
    const auto vertexCode =
        azurerender::readBinaryFile(shaderDirectory + (objectShadowPrepared_
            ? (computeSkinningEnabled_?"/mesh_object_compute.vert.spv":"/mesh_object.vert.spv")
            : (computeSkinningEnabled_?"/mesh_compute.vert.spv":"/mesh.vert.spv")));
    const auto fragmentCode =
        mainFragment((objectShadowPrepared_
            ? (bindlessTextures_?"/mesh_object_bindless.frag.spv":"/mesh_object.frag.spv")
            : (bindlessTextures_?"/mesh_bindless.frag.spv":"/mesh.frag.spv")),true);
    const auto outlineVertexCode =
        azurerender::readBinaryFile(shaderDirectory + (outlineProjectionEnabled_
            ? (computeSkinningEnabled_?(bindlessTextures_?"/outline_view_scaled_compute_bindless.vert.spv":"/outline_view_scaled_compute.vert.spv")
                :(bindlessTextures_?"/outline_view_scaled_bindless.vert.spv":"/outline_view_scaled.vert.spv"))
            : (computeSkinningEnabled_?"/outline_compute.vert.spv":"/outline.vert.spv")));
    const auto outlineFragmentCode =
        mainFragment((outlineProjectionEnabled_
            ?(bindlessTextures_?"/outline_view_scaled_bindless.frag.spv":"/outline_view_scaled.frag.spv"):"/outline.frag.spv"));
    const auto backgroundVertexCode =
        azurerender::readBinaryFile(shaderDirectory + "/background.vert.spv");
    const auto backgroundFragmentCode =
        mainFragment((bindlessTextures_
            ? "/background_bindless.frag.spv"
            : "/background.frag.spv"));
    const auto shadowVertexCode =
        azurerender::readBinaryFile(shaderDirectory + (computeSkinningEnabled_
            ? "/shadow_compute.vert.spv"
            : "/shadow.vert.spv"));
    const auto shadowFragmentCode =
        azurerender::readBinaryFile(shaderDirectory + (bindlessTextures_
            ? "/shadow_bindless.frag.spv"
            : "/shadow.frag.spv"));
    const VkShaderModule vertexModule = rhi_->createShaderModule(vertexCode);
    const VkShaderModule fragmentModule =
        rhi_->createShaderModule(fragmentCode);
    const VkShaderModule outlineVertexModule =
        rhi_->createShaderModule(outlineVertexCode);
    const VkShaderModule outlineFragmentModule =
        rhi_->createShaderModule(outlineFragmentCode);
    const VkShaderModule backgroundVertexModule =
        rhi_->createShaderModule(backgroundVertexCode);
    const VkShaderModule backgroundFragmentModule =
        rhi_->createShaderModule(backgroundFragmentCode);
    const VkShaderModule shadowVertexModule =
        rhi_->createShaderModule(shadowVertexCode);
    const VkShaderModule shadowFragmentModule =
        rhi_->createShaderModule(shadowFragmentCode);

    try {
        const rhi::PushConstantRangeDesc pushConstants{
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(MaterialPushConstants) + sizeof(MorphPushConstants)
                + sizeof(std::uint32_t),
        };
        pipelineLayout_ =
            rhi_->createPipelineLayout(descriptorSetLayout_, &pushConstants);

        // Full material vertex layout shared by the main-pass pipelines.
        const std::vector<rhi::VertexAttributeDesc> materialAttributes = {
            {0,
             VK_FORMAT_R32G32B32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, position))},
            {1,
             VK_FORMAT_R32G32B32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, normal))},
            {2,
             VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, tangent))},
            {3,
             VK_FORMAT_R32G32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, texcoord))},
            {4,
             VK_FORMAT_R32G32B32A32_UINT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, joints))},
            {5,
             VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, weights))},
            {6,
             VK_FORMAT_R32G32B32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, morph0))},
            {7,
             VK_FORMAT_R32G32B32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, morph1))},
            {8, VK_FORMAT_R32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(AssetVertex, browMask))},
        };
        const std::vector<rhi::VertexAttributeDesc> shadowAttributes = {
            materialAttributes[0],
            materialAttributes[3],
            materialAttributes[4],
            materialAttributes[5],
            materialAttributes[6],
            materialAttributes[7],
        };
        std::vector<rhi::VertexAttributeDesc> outlineAttributes = {
            materialAttributes[0],
            materialAttributes[1],
            materialAttributes[4],
            materialAttributes[5],
            materialAttributes[6],
            materialAttributes[7],
        };

        if(outlineProjectionEnabled_) {
            outlineAttributes.push_back(materialAttributes[2]);outlineAttributes.push_back(materialAttributes[3]);
        }
        rhi::GraphicsPipelineDesc materialDesc{};
        materialDesc.vertexShader = vertexModule;
        materialDesc.fragmentShader = fragmentModule;
        materialDesc.vertexStride =
            static_cast<std::uint32_t>(sizeof(AssetVertex));
        materialDesc.vertexAttributes = materialAttributes;
        materialDesc.colorAttachmentCount = kMaterialColorAttachmentCount;
        materialDesc.renderPass = context.sceneRenderPass;
        materialDesc.rasterizationSamples=context.rasterizationSamples;materialDesc.minSampleShading=context.rasterizationSamples==VK_SAMPLE_COUNT_4_BIT?1.F:0.F;
        materialDesc.layout = pipelineLayout_;
        // The Vulkan lens flips Y and uses a positive-height viewport.
        // glTF outward CCW faces remain CCW in framebuffer coordinates.
        materialDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        materialDesc.depthTest = context.depthTestEnabled;

        materialDesc.cullMode = context.faceCullingEnabled ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        materialDesc.alphaBlend = false;
        materialDesc.depthWrite = true;
        const auto integralBase=materialDesc;
        opaquePipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.frontFace = VK_FRONT_FACE_CLOCKWISE;
        opaqueMirroredPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        auto depthModule=rhi_->createShaderModule(mainFragment((objectShadowPrepared_?(bindlessTextures_?"/character_snapshot_object_bindless.frag.spv":"/character_snapshot_object.frag.spv"):(bindlessTextures_?"/character_snapshot_bindless.frag.spv":"/character_snapshot.frag.spv")),true));
        auto depthDesc=materialDesc;depthDesc.fragmentShader=depthModule;depthDesc.renderPass=snapshotPass();
        depthDesc.colorAttachmentCount=2;
        overlayDepthPipeline_=rhi_->createGraphicsPipeline(depthDesc);
        depthDesc.frontFace=VK_FRONT_FACE_CLOCKWISE;
        overlayDepthMirroredPipeline_=rhi_->createGraphicsPipeline(depthDesc);
        depthDesc.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;depthDesc.cullMode=VK_CULL_MODE_NONE;
        overlayDepthDoubleSidedPipeline_=rhi_->createGraphicsPipeline(depthDesc);
        try{initializeOrdinaryOpaquePipelines(context,integralBase);}
        catch(...){rhi_->destroyShaderModule(depthModule);throw;}
        rhi_->destroyShaderModule(depthModule);
        const auto simpleDepthModule=rhi_->createShaderModule(mainFragment(bindlessTextures_
            ?"/character_snapshot_depth_bindless.frag.spv":"/character_snapshot_depth.frag.spv"));
        VkShaderModule simpleDepthVertex=VK_NULL_HANDLE;
        try{
            simpleDepthVertex=rhi_->createShaderModule(azurerender::readBinaryFile(shaderDirectory+(computeSkinningEnabled_
                ?"/character_snapshot_depth_compute.vert.spv":"/character_snapshot_depth.vert.spv")));
            auto desc=depthDesc;desc.vertexShader=simpleDepthVertex;desc.fragmentShader=simpleDepthModule;desc.colorWriteMasks={0,VK_COLOR_COMPONENT_R_BIT};
            for(unsigned facing=0;facing<3;++facing){
                desc.frontFace=facing==1?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing==2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                snapshotDepthOnlyPipelines_[facing]=rhi_->createGraphicsPipeline(desc);
                // Only prefill is biased; strict LESS color keeps coplanar draw order.
                auto prefill=desc;prefill.colorWriteMasks={0,0};prefill.depthBias=true;
                prefill.depthBiasConstant=1.F;prefill.depthBiasSlope=0.F;
                snapshotPrefillPipelines_[facing]=rhi_->createGraphicsPipeline(prefill);
                prefill.renderPass=context.sceneRenderPass;
                mainPrefillPipelines_[facing]=rhi_->createGraphicsPipeline(prefill);
            }
        }catch(...){if(simpleDepthVertex)rhi_->destroyShaderModule(simpleDepthVertex);rhi_->destroyShaderModule(simpleDepthModule);throw;}
        rhi_->destroyShaderModule(simpleDepthVertex);rhi_->destroyShaderModule(simpleDepthModule);
        materialDesc.cullMode = VK_CULL_MODE_NONE;
        opaqueDoubleSidedPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.cullMode = context.faceCullingEnabled ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        materialDesc.alphaBlend = true;
        materialDesc.depthWrite = false;
        blendPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.frontFace = VK_FRONT_FACE_CLOCKWISE;
        blendMirroredPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        materialDesc.cullMode = VK_CULL_MODE_NONE;
        blendDoubleSidedPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        initializeHairIntegralPipelines(context,integralBase);
        auto multiplyDesc=materialDesc;
        multiplyDesc.primaryColorBlend=rhi::PrimaryColorBlend::Multiply;
        multiplyDesc.depthCompare=VK_COMPARE_OP_LESS_OR_EQUAL;
        multiplyDesc.cullMode=context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
        multiplyPipeline_=rhi_->createGraphicsPipeline(multiplyDesc);
        multiplyDesc.frontFace=VK_FRONT_FACE_CLOCKWISE;
        multiplyMirroredPipeline_=rhi_->createGraphicsPipeline(multiplyDesc);
        multiplyDesc.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
        multiplyDesc.cullMode=VK_CULL_MODE_NONE;
        multiplyDoubleSidedPipeline_=rhi_->createGraphicsPipeline(multiplyDesc);
        auto tintDesc=materialDesc;
        sceneTintPipeline_=rhi_->createGraphicsPipeline(tintDesc);

        const auto maskModule=rhi_->createShaderModule(mainFragment((bindlessTextures_?"/outline_mask_bindless.frag.spv":"/outline_mask.frag.spv")));
        const auto maskVertex=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+(computeSkinningEnabled_?"/outline_mask_compute.vert.spv":"/outline_mask.vert.spv")));
        auto maskDesc=materialDesc;maskDesc.fragmentShader=maskModule;maskDesc.renderPass=outlineMaskPass_;
        maskDesc.vertexShader=maskVertex;
        maskDesc.colorAttachmentCount=1;maskDesc.depthTest=false;maskDesc.depthWrite=false;maskDesc.alphaBlend=false;
        maskDesc.cullMode=context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
        maskDesc.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
        outlineMaskPipeline_=rhi_->createGraphicsPipeline(maskDesc);
        maskDesc.frontFace=VK_FRONT_FACE_CLOCKWISE;
        outlineMaskMirroredPipeline_=rhi_->createGraphicsPipeline(maskDesc);
        maskDesc.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;maskDesc.cullMode=VK_CULL_MODE_NONE;
        outlineMaskDoubleSidedPipeline_=rhi_->createGraphicsPipeline(maskDesc);
        rhi_->destroyShaderModule(maskModule);rhi_->destroyShaderModule(maskVertex);

        rhi::GraphicsPipelineDesc outlineDesc = materialDesc;
        outlineDesc.vertexShader = outlineVertexModule;
        outlineDesc.fragmentShader = outlineFragmentModule;
        outlineDesc.vertexAttributes = outlineAttributes;
        outlineDesc.cullMode = VK_CULL_MODE_FRONT_BIT;
        outlineDesc.alphaBlend = false;
        outlineDesc.depthWrite = false;
        outlinePipeline_ = rhi_->createGraphicsPipeline(outlineDesc);
        outlineDesc.frontFace = VK_FRONT_FACE_CLOCKWISE;
        outlineMirroredPipeline_ = rhi_->createGraphicsPipeline(outlineDesc);
        if(outlineProjectionEnabled_) {
            const auto depthModule=rhi_->createShaderModule(mainFragment(
                (bindlessTextures_?"/outline_depth_bindless.frag.spv":"/outline_depth.frag.spv")));
            try {
                auto shell=outlineDesc;shell.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
                shell.fragmentShader=depthModule;shell.renderPass=snapshotPass();
                shell.depthTest=true;shell.depthWrite=true;shell.depthCompare=VK_COMPARE_OP_LESS;
                shell.colorWriteMasks={0,VK_COLOR_COMPONENT_R_BIT};
                outlineDepthPipeline_=rhi_->createGraphicsPipeline(shell);
                shell.frontFace=VK_FRONT_FACE_CLOCKWISE;
                outlineDepthMirroredPipeline_=rhi_->createGraphicsPipeline(shell);
            }catch(...){rhi_->destroyShaderModule(depthModule);throw;}
            rhi_->destroyShaderModule(depthModule);
        }

        rhi::GraphicsPipelineDesc backgroundDesc = materialDesc;
        backgroundDesc.vertexShader = backgroundVertexModule;
        backgroundDesc.fragmentShader = backgroundFragmentModule;
        backgroundDesc.vertexStride = 0;
        backgroundDesc.vertexAttributes.clear();
        backgroundDesc.cullMode = VK_CULL_MODE_NONE;
        backgroundDesc.depthTest = false;
        backgroundDesc.depthWrite = false;
        backgroundPipeline_ = rhi_->createGraphicsPipeline(backgroundDesc);

        rhi::GraphicsPipelineDesc shadowDesc = materialDesc;
        shadowDesc.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;shadowDesc.minSampleShading=0;
        shadowDesc.vertexShader = shadowVertexModule;
        shadowDesc.fragmentShader = shadowFragmentModule;
        shadowDesc.vertexAttributes = shadowAttributes;
        shadowDesc.cullMode = VK_CULL_MODE_NONE;
        shadowDesc.depthBias = true;
        shadowDesc.depthBiasConstant = 1.25F;
        shadowDesc.depthBiasSlope = 1.75F;
        shadowDesc.depthTest = true;
        shadowDesc.depthWrite = true;
        shadowDesc.alphaBlend = false;
        shadowDesc.colorAttachmentCount = 0;
        shadowDesc.renderPass = context.shadowRenderPass;
        shadowPipeline_ = rhi_->createGraphicsPipeline(shadowDesc);
        if(objectShadowPrepared_){
            shadowDesc.renderPass=objectShadowAtlas_->renderPass();
            shadowDesc.depthBiasConstant=8.F;shadowDesc.depthBiasSlope=0.F;
            shadowDesc.depthBiasExact=rhi_->supportsExactDepthBias();
            objectShadowPipeline_=rhi_->createGraphicsPipeline(shadowDesc);
        }
    } catch (...) {
        rhi_->destroyShaderModule(shadowFragmentModule);
        rhi_->destroyShaderModule(shadowVertexModule);
        rhi_->destroyShaderModule(backgroundFragmentModule);
        rhi_->destroyShaderModule(backgroundVertexModule);
        rhi_->destroyShaderModule(outlineFragmentModule);
        rhi_->destroyShaderModule(outlineVertexModule);
        rhi_->destroyShaderModule(fragmentModule);
        rhi_->destroyShaderModule(vertexModule);
        throw;
    }
    rhi_->destroyShaderModule(shadowFragmentModule);
    rhi_->destroyShaderModule(shadowVertexModule);
    rhi_->destroyShaderModule(backgroundFragmentModule);
    rhi_->destroyShaderModule(backgroundVertexModule);
    rhi_->destroyShaderModule(outlineFragmentModule);
    rhi_->destroyShaderModule(outlineVertexModule);
    rhi_->destroyShaderModule(fragmentModule);
    rhi_->destroyShaderModule(vertexModule);
}


void CharacterSceneRenderer::destroyResources() {
    destroyOrdinaryOpaquePipelines();
    destroyHairIntegralPipelines();
    motionPass_.reset();motionTarget_=nullptr;motionDraws_.clear();motionPreviousValidObjects_=0;
    destroyOverlayDepth();
    for (auto& resources : gpuCullingFrames_) resources.reset();
    gpuCullingCapacities_.fill(0);
    if (rhi_ == nullptr) {
        return;
    }
    for (VkPipeline* pipeline : {
             &opaquePipeline_,
             &opaqueMirroredPipeline_,
             &opaqueDoubleSidedPipeline_,
             &blendPipeline_,
             &blendMirroredPipeline_,
             &blendDoubleSidedPipeline_,
             &multiplyPipeline_,
             &multiplyMirroredPipeline_,
             &multiplyDoubleSidedPipeline_,
             &sceneTintPipeline_,
             &outlinePipeline_,&outlineDepthPipeline_,&outlineDepthMirroredPipeline_,
             &outlineMirroredPipeline_,
             &backgroundPipeline_,
             &shadowPipeline_,
             &objectShadowPipeline_,
             &overlayDepthPipeline_,&overlayDepthMirroredPipeline_,&overlayDepthDoubleSidedPipeline_,&snapshotDepthOnlyPipelines_[0],&snapshotDepthOnlyPipelines_[1],&snapshotDepthOnlyPipelines_[2],&snapshotPrefillPipelines_[0],&snapshotPrefillPipelines_[1],&snapshotPrefillPipelines_[2],&mainPrefillPipelines_[0],&mainPrefillPipelines_[1],&mainPrefillPipelines_[2],&outlineMaskPipeline_,
             &outlineMaskMirroredPipeline_,&outlineMaskDoubleSidedPipeline_}) {
        if (*pipeline != VK_NULL_HANDLE) {
            rhi_->destroyPipeline(*pipeline);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(pipelineLayout_);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    for(auto& bounds:shadowBounds_)bounds.reset();
    objectShadowAtlas_.reset();objectShadowPages_.clear();objectShadowPrepared_=false;objectShadowBoundViews_.fill(VK_NULL_HANDLE);
    if (descriptorPool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(descriptorPool_);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(descriptorSetLayout_);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (skinningPipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(skinningPipeline_);
        skinningPipeline_ = VK_NULL_HANDLE;
    }
    if (skinningPipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(skinningPipelineLayout_);
        skinningPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (skinningDescriptorPool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(skinningDescriptorPool_);
        skinningDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (skinningDescriptorSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(skinningDescriptorSetLayout_);
        skinningDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    skinningDescriptorSets_.clear();
    for (auto& buffer : uniformBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    uniformBuffers_.clear();
    allocator_->destroyBuffer(materialSurfaceProfiles_);
    for (auto& buffer : jointBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    jointBuffers_.clear();
    allocator_->destroyBuffer(morphTargetBuffer_);
    for(auto& buffer:morphWeightBuffers_)allocator_->destroyBuffer(buffer);
    morphGpuMeshes_.clear();
    for (auto& buffer : oitIndexBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    oitIndexBuffers_.clear();
    for (auto& buffer : instanceBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    instanceBuffers_.clear();
    materialOriginInstances_.reset();materialOriginNodes_.clear();
    for (auto& buffer : lightBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    lightBuffers_.clear();
    for (auto& buffer : clusterHeaderBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    clusterHeaderBuffers_.clear();
    for (auto& buffer : clusterIndexBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    clusterIndexBuffers_.clear();
    for (auto& buffer : skinnedVertexBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    skinnedVertexBuffers_.clear();
    allocator_->destroyBuffer(indexBuffer_);
    allocator_->destroyBuffer(vertexBuffer_);
    gpuMaterials_.clear();
    for (auto& resource : additionalResources_) {
        allocator_->destroyBuffer(resource->indexBuffer);
        allocator_->destroyBuffer(resource->vertexBuffer);
        for (auto& buffer : resource->skinnedVertexBuffers) {
            allocator_->destroyBuffer(buffer);
        }
        resource->skinnedVertexBuffers.clear();
        resource->gpuMaterials.clear();
    }
    additionalResources_.clear();
    outlineProjection_.reset();outlineProjectionEnabled_=false;
    materialTextureCache_.reset();
    if (iblPrefilterPipeline_ != VK_NULL_HANDLE) {
        rhi_->destroyPipeline(iblPrefilterPipeline_);
        iblPrefilterPipeline_ = VK_NULL_HANDLE;
    }
    if (iblPrefilterPipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(iblPrefilterPipelineLayout_);
        iblPrefilterPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (iblPrefilterPool_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorPool(iblPrefilterPool_);
        iblPrefilterPool_ = VK_NULL_HANDLE;
    }
    if (iblPrefilterSetLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyDescriptorSetLayout(iblPrefilterSetLayout_);
        iblPrefilterSetLayout_ = VK_NULL_HANDLE;
    }
    for (VkImageView view : environmentPrefilterViews_) {
        rhi_->destroyImageView(view);
    }
    environmentPrefilterViews_.clear();
    if (environmentBaseMipView_ != VK_NULL_HANDLE) {
        rhi_->destroyImageView(environmentBaseMipView_);
        environmentBaseMipView_ = VK_NULL_HANDLE;
    }
    rhi_->destroySampler(environmentTexture_.sampler);
    reflectionProbeTexture_.reset();
    irradianceClipmap_.reset();irradianceClipmapActive_=false;irradianceClipmapDimensions_={1,1,1};
    allocator_->destroyBuffer(irradianceBuffer_);irradianceBuffer_={};irradianceVolume_={};
    rhi_->destroyImageView(environmentTexture_.view);
    allocator_->destroyImage(environmentTexture_.image);
    rhi_->destroySampler(toonRampTexture_.sampler);
    rhi_->destroyImageView(toonRampTexture_.view);
    allocator_->destroyImage(toonRampTexture_.image);
}

void CharacterSceneRenderer::destroyGraphicsPipelinesForRecreate() {
    destroyOrdinaryOpaquePipelines();
    destroyHairIntegralPipelines();
    for (VkPipeline* pipeline : {
             &opaquePipeline_,
             &opaqueMirroredPipeline_,
             &opaqueDoubleSidedPipeline_,
             &blendPipeline_,
             &blendMirroredPipeline_,
             &blendDoubleSidedPipeline_,
             &multiplyPipeline_,
             &multiplyMirroredPipeline_,
             &multiplyDoubleSidedPipeline_,
             &sceneTintPipeline_,
             &outlinePipeline_,&outlineDepthPipeline_,&outlineDepthMirroredPipeline_,
             &outlineMirroredPipeline_,
             &backgroundPipeline_,
             &shadowPipeline_,
             &objectShadowPipeline_,
             &overlayDepthPipeline_,&overlayDepthMirroredPipeline_,&overlayDepthDoubleSidedPipeline_,&snapshotDepthOnlyPipelines_[0],&snapshotDepthOnlyPipelines_[1],&snapshotDepthOnlyPipelines_[2],&snapshotPrefillPipelines_[0],&snapshotPrefillPipelines_[1],&snapshotPrefillPipelines_[2],&mainPrefillPipelines_[0],&mainPrefillPipelines_[1],&mainPrefillPipelines_[2],&outlineMaskPipeline_,
             &outlineMaskMirroredPipeline_,&outlineMaskDoubleSidedPipeline_}) {
        if (*pipeline != VK_NULL_HANDLE) {
            rhi_->destroyPipeline(*pipeline);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(pipelineLayout_);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// Frame recording
// ---------------------------------------------------------------------------

void CharacterSceneRenderer::updateUniformBuffer(
    const SceneFrameData& frame) {
    const float deltaSeconds = frame.deltaSeconds;
    animationFrames_ = frame.animations;
    if (animationPlaying_) animationTime_ += deltaSeconds;

    const Vector3 center = {
        footPivot_[0],
        (asset_.boundsMin[1] + asset_.boundsMax[1]) * 0.5F,
        footPivot_[2],
    };
    const float largestExtent = std::max({
        asset_.boundsMax[0] - asset_.boundsMin[0],
        asset_.boundsMax[1] - asset_.boundsMin[1],
        asset_.boundsMax[2] - asset_.boundsMin[2],
    });
    const float fitScale = largestExtent > 0.0F ? 2.5F / largestExtent : 1.0F;
    const Matrix4 model = multiply(
        rotationY(rotationAngle_),
        multiply(
            uniformScale(fitScale),
            translation(-center[0], -center[1], -center[2])));
    currentModel_ = sceneWorldCoordinates_ ? scene::identityMatrix() : model;
    const Matrix4 view = characterView(*renderSettings_,cameraPosition_,cameraTarget_);
    viewMatrix_ = view;
    const float aspect =
        static_cast<float>(frame.swapchainWidth)
        / static_cast<float>(std::max(frame.swapchainHeight, 1U));
    constexpr float kPi = 3.14159265358979323846F;
    Matrix4 projection = characterProjection(*renderSettings_,aspect);
    projectionMatrix_ = projection;
    prepareTemporalFrame(frame);
    updateClusteredLighting(
        view,
        projection,
        std::max(frame.swapchainWidth, 1U),
        std::max(frame.swapchainHeight, 1U));
    viewFrustum_ =
        scene::extractFrustumPlanes(multiply(projection, view));
    const RenderSettings& settings = *renderSettings_;
    Vector3 lightDirection = settings.showcasePreset == 1
        ? normalize({0.62F, 0.68F, 0.38F})
        : normalize({0.48F, 0.82F, 0.32F});
    const auto* mainSceneLight=settings.sceneLightingEnabled?scene::primaryDirectionalLight(scene_.lights):nullptr;
    if(mainSceneLight)lightDirection=scaleVector(mainSceneLight->shape.direction,-1.F);
    if (frame.qaLightScan&&!mainSceneLight) {
        const float angle = 2.0F * kPi * static_cast<float>(frame.capturedFrames % 120) / 119.0F;
        const auto initial = lightDirection;
        lightDirection = {std::cos(angle) * initial[0] - std::sin(angle) * initial[2],
            initial[1], std::sin(angle) * initial[0] + std::cos(angle) * initial[2]};
    }
    cascadeSplits_ = {};
    const std::vector<float> splits = computeCascadeSplits(
        settings.cameraNear, settings.shadowDistance, kShadowCascadeCount, 0.65F);
    std::copy(splits.begin(), splits.end(), cascadeSplits_.begin());
    const Vector3 cameraForward = normalize(
        subtract(cameraTarget_, cameraPosition_));
    const Vector3 worldUp = settings.cameraView?settings.cameraView->up:Vector3{0.0F, 1.0F, 0.0F};
    const Vector3 cameraRight = normalize(cross(cameraForward, worldUp));
    const Vector3 cameraUp = normalize(cross(cameraRight, cameraForward));
    // Keep conservative shadow coverage around the shared geometry projection.
    const float halfFovTangent = 1.0F / std::abs(projection[5]);
    std::array<float,4> cascadeDepthRanges{};
    float cascadeNear = settings.cameraNear;
    for (std::size_t cascade = 0;
         cascade < kShadowCascadeCount;
         ++cascade) {
        const float cascadeFar = cascadeSplits_[cascade];
        std::array<Vector3, 8> corners{};
        std::size_t cornerIndex = 0;
        Vector3 center{};
        for (const float depth : {cascadeNear, cascadeFar}) {
            const float halfHeight = settings.cameraView&&settings.cameraView->projection==CameraProjectionKind::Orthographic?settings.cameraView->orthoHeight*.5F:depth * halfFovTangent;
            const float halfWidth = halfHeight * aspect;
            const Vector3 sliceCenter = addVectors(
                cameraPosition_, scaleVector(cameraForward, depth));
            const auto projectionShift=settings.cameraView?cameraProjectionRayOffset(*settings.cameraView,cameraRight,cameraUp,halfWidth,halfHeight):Vector3{};
            for (const float vertical : {-1.0F, 1.0F}) {
                for (const float horizontal : {-1.0F, 1.0F}) {
                    const Vector3 corner = addVectors(
                        addVectors(sliceCenter,projectionShift),
                        addVectors(
                            scaleVector(cameraRight, horizontal * halfWidth),
                            scaleVector(cameraUp, vertical * halfHeight)));
                    corners[cornerIndex++] = corner;
                    center = addVectors(center, corner);
                }
            }
        }
        center = scaleVector(
            center, 1.0F / static_cast<float>(corners.size()));
        float radius = 0.0F;
        for (const Vector3& corner : corners) {
            radius = std::max(
                radius, vectorLength(subtract(corner, center)));
        }
        radius = std::max(std::ceil(radius * 16.0F) / 16.0F, 0.25F);
        cascadeDepthRanges[cascade]=radius*4.1F-.1F;
        const Vector3 snapUp=std::abs(dot(lightDirection,worldUp))>.97F?Vector3{0,0,1}:worldUp;
        const Vector3 snapRight=normalize(cross(snapUp,lightDirection));
        const Vector3 snapVertical=normalize(cross(lightDirection,snapRight));
        const float worldTexel=2*radius/static_cast<float>(shadowCascadeResolution_);
        const float dx=std::round(dot(center,snapRight)/worldTexel)*worldTexel-dot(center,snapRight);
        const float dy=std::round(dot(center,snapVertical)/worldTexel)*worldTexel-dot(center,snapVertical);
        center=addVectors(center,addVectors(scaleVector(snapRight,dx),scaleVector(snapVertical,dy)));
        const Vector3 lightEye = addVectors(
            center, scaleVector(lightDirection, radius * 2.0F + 0.1F));
        const Vector3 lightUp = std::abs(dot(lightDirection, worldUp)) > 0.97F
            ? Vector3{0.0F, 0.0F, 1.0F}
            : worldUp;
        const Matrix4 lightView = lookAt(lightEye, center, lightUp);
        const Matrix4 lightProjection = orthographic(
            -radius, radius, -radius, radius, 0.1F, radius * 4.1F);
        cascadeLightViewProjections_[cascade] =
            multiply(lightProjection, lightView);
        shadowCascadeFrusta_[cascade] = scene::extractFrustumPlanes(
            cascadeLightViewProjections_[cascade]);
        cascadeNear = cascadeFar;
    }

    UniformBufferObject uniform{};
    // The actual view rotation is independent of sky projection scale.
    uniform.cameraRight={viewMatrix_[0],viewMatrix_[4],viewMatrix_[8],samplingJitter_[0]};
    uniform.cameraUp={viewMatrix_[1],viewMatrix_[5],viewMatrix_[9],samplingJitter_[1]};
    const auto& reflection=settings.screenSpaceReflection;
    const bool reflectionActive=screenSpaceReflectionActive(reflection,settings.sceneLightingEnabled,
        qaIsolationMode_,!(qaEffectMode_==5&&!qaEffectEnabled_));
    uniform.screenReflectionWorldToClip=multiply(samplingProjectionMatrix_,viewMatrix_);
    uniform.screenReflectionTrace={reflectionActive?1.F:0.F,reflection.maxDistance,reflection.thickness,reflection.normalBias};
    uniform.screenReflectionQuality={float(reflection.maxSteps),reflection.maxRoughness,reflection.edgeFade,reflection.strength};
    uniform.cascadeDepthRanges=cascadeDepthRanges;
    const float skyScale=settings.cameraView&&settings.cameraView->projection==CameraProjectionKind::Orthographic?0.F:halfFovTangent;
    uniform.skyRight={cameraRight[0]*skyScale*aspect,cameraRight[1]*skyScale*aspect,cameraRight[2]*skyScale*aspect,0};
    uniform.skyUp={cameraUp[0]*skyScale,cameraUp[1]*skyScale,cameraUp[2]*skyScale,0};
    uniform.skyForward={cameraForward[0],cameraForward[1],cameraForward[2],0};
    if(settings.cameraView){const auto shift=cameraProjectionRayOffset(*settings.cameraView,cameraRight,cameraUp,skyScale*aspect,skyScale);
        for(unsigned axis=0;axis<3;++axis)uniform.skyForward[axis]+=shift[axis];}
    uniform.skyColor={settings.sky.color[0],settings.sky.color[1],settings.sky.color[2],1};
    uniform.skyParameters={static_cast<float>(settings.sky.mode),settings.sky.intensity,settings.sky.rotationDegrees*kPi/180.F,0};
    uniform.clusterLighting={settings.sceneLightingEnabled?1.F:0.F,settings.sky.intensity,settings.sky.rotationDegrees*kPi/180.F,float(settings.environmentSpecularModel)};
    uniform.cameraPosition = {
        cameraPosition_[0], cameraPosition_[1], cameraPosition_[2], 1.0F,
    };
    uniform.cameraForward = {
        cameraForward[0], cameraForward[1], cameraForward[2], 0.0F,
    };
    uniform.clusterGrid = {
        static_cast<float>(kClusterGridX),
        static_cast<float>(kClusterGridY),
        static_cast<float>(kClusterGridZ),
        static_cast<float>(frameLights_.lights().size()),
    };
    uniform.clusterDepth = {
        settings.cameraNear,
        settings.cameraFar,
        static_cast<float>(std::max(frame.swapchainWidth, 1U)),
        static_cast<float>(std::max(frame.swapchainHeight, 1U)),
    };
    uniform.cascadeSplits = {
        cascadeSplits_[0],
        cascadeSplits_[1],
        cascadeSplits_[2],
        cascadeSplits_[3],
    };
    uniform.renderingParameters = {
        settings.outline.silhouetteWidthPixels*(settings.antiAliasing==2?2.F:1.F),
        settings.stylizedLightingEnabled
            ? settings.styleMaskStrength
            : 0.0F,
        settings.stylizedLightingEnabled
            ? settings.diffuseBandThreshold
            : -1.0F,
        settings.shadow.maximumFilterRadiusTexels,
    };
    constexpr std::array<std::array<float, 4>, 5> kShowcasePresets = {{
        {0.0F, 1.08F, 0.24F, 0.18F},
        {1.0F, 1.08F, 0.26F, 0.10F},
        {2.0F, 0.95F, 0.08F, 0.05F},
        {3.0F, 0.48F, 0.04F, 0.85F},
        {4.0F, 0.18F, 0.02F, 0.08F},
    }};
    uniform.showcaseParameters =
        kShowcasePresets[std::min<std::size_t>(
            settings.showcasePreset,
            kShowcasePresets.size() - 1)];
    uniform.qaParameters = {
        static_cast<float>(qaIsolationMode_),
        static_cast<float>(qaEffectMode_),
        qaEffectEnabled_ ? 1.0F : 0.0F,
        qaHarnessEnabled_ ? 1.0F : 0.0F,
    };
    frameMainLight_ = lightDirection;
    uniform.faceLightDirection = {0, 0, -1, 0};
    uniform.faceSdfParameters = {
        faceSdfPrepared_&&settings.faceSdf.enabled ? 1.0F : 0.0F,
        settings.faceSdf.threshold,
        settings.faceSdf.softness,
        settings.faceSdf.mirrorHorizontal ? 1.0F : 0.0F,
    };
    uniform.faceSdfShadowColor = settings.faceSdf.shadowColor;
    uniform.mainLightDirection = {lightDirection[0], lightDirection[1], lightDirection[2], (frame.qaLightScan||mainSceneLight) ? 1.0F : 0.0F};
    const auto& npr=settings.nprLighting;
    uniform.nprDirectionBias={npr.ambientDirection[0],npr.ambientDirection[1],npr.ambientDirection[2],npr.directionBias};
    uniform.nprGainFloor={npr.directionGain,npr.directionFloor,npr.shadowOverride,npr.useSceneIrradiance?1.F:0.F};
    uniform.nprAmbientColor={npr.ambientColor[0],npr.ambientColor[1],npr.ambientColor[2],npr.indoorMix};
    uniform.nprComposition={npr.outdoorGain,npr.shadowAmbientGain,npr.litAmbientMix,npr.mainColorMix};
    uniform.nprMainRadiance={1,1,1,0};uniform.nprLightingParameters={npr.enabled&&settings.sceneLightingEnabled?1.F:0.F,-1,npr.ambientWeight,npr.specularFloor};
    if(mainSceneLight){
        const auto id=stableSceneLightId(mainSceneLight->id);const auto& lights=frameLights_.lights();
        for(std::size_t index=0;index<lights.size();++index)if(lights[index].stableId==id){
            uniform.nprLightingParameters[1]=static_cast<float>(index);
            if(lights[index].shape.receivers.characters)
                uniform.nprMainRadiance={lights[index].color[0],lights[index].color[1],lights[index].color[2],lights[index].intensity};
            break;
        }
    }
    const auto& probe=scene_.reflectionProbe;
    uniform.probeCenter={probe.center[0],probe.center[1],probe.center[2],0};
    uniform.probeHalfExtents={probe.halfExtents[0],probe.halfExtents[1],probe.halfExtents[2],0};
    const auto projectionOrigin=probe.projectionOrigin.value_or(probe.capturePosition);
    uniform.probeCapture={projectionOrigin[0],projectionOrigin[1],projectionOrigin[2],0};
    uniform.probeParameters={!probe.texture.empty()?1.F:0.F,probe.intensity,probe.blendDistance,probe.boxProjection?1.F:0.F};
    const auto& gi=scene_.irradianceVolume;
    uniform.irradianceOriginSpacing={irradianceVolume_.origin[0],irradianceVolume_.origin[1],irradianceVolume_.origin[2],irradianceVolume_.spacing};
    const auto& irradianceDimensions=irradianceClipmapActive_?irradianceClipmapDimensions_:irradianceVolume_.dimensions;
    uniform.irradianceDimensionsEnabled={float(irradianceDimensions[0]),float(irradianceDimensions[1]),float(irradianceDimensions[2]),gi.enabled&&(irradianceClipmap_||!irradianceVolume_.cells.empty())?1.F:0.F};
    uniform.irradianceParameters={gi.intensity,gi.sampling.normalOffset,gi.sampling.jitterAmplitude,gi.sampling.phase};
    std::memcpy(
        uniformBuffers_[currentFrame_].mapped,
        &uniform,
        sizeof(uniform));
}

void CharacterSceneRenderer::updateClusteredLighting(
    const Matrix4& view,
    const Matrix4& projection,
    const std::uint32_t width,
    const std::uint32_t height) {
    (void)width;
    (void)height;
    std::vector<RenderLight> lights;
    lights.reserve(scene_.lights.size());
    const auto* mainSceneLight=renderSettings_->sceneLightingEnabled?scene::primaryDirectionalLight(scene_.lights):nullptr;
    for (const scene::SceneLightDesc& source : scene_.lights) {
        if (!source.enabled) {
            continue;
        }
        const Vector3 viewPosition = transformPosition(view, source.position);
        const float depth = -viewPosition[2];
        if (source.shape.type!=LightKind::Directional&&!source.shape.characterResponse.isProjectedColorBlend()
            && (depth + source.radius < renderSettings_->cameraNear || source.radius <= 0.0F)) {
            continue;
        }
        const float clipX = projection[0] * viewPosition[0]
            + projection[4] * viewPosition[1]
            + projection[8] * viewPosition[2] + projection[12];
        const float clipY = projection[1] * viewPosition[0]
            + projection[5] * viewPosition[1]
            + projection[9] * viewPosition[2] + projection[13];
        const float clipW = projection[3] * viewPosition[0]
            + projection[7] * viewPosition[1]
            + projection[11] * viewPosition[2] + projection[15];
        RenderLight light{};
        light.stableId = stableSceneLightId(source.id);
        light.position[0] = source.position[0];
        light.position[1] = source.position[1];
        light.position[2] = source.position[2];
        light.clusterPosition[0] = std::abs(clipW)>1e-6F ? clipX / clipW : 0;
        light.clusterPosition[1] = std::abs(clipW)>1e-6F ? clipY / clipW : 0;
        light.clusterPosition[2] = depth;
        light.viewSpaceBounds = true;
        light.viewPosition = {viewPosition[0],viewPosition[1],depth};
        for (std::size_t channel = 0; channel < 3; ++channel) {
            light.color[channel] = source.color[channel];
        }
        light.intensity = source.intensity;
        light.radius = source.radius;
        light.shape = source.shape;
        light.rasterShadowCaster=mainSceneLight==&source;
        lights.push_back(light);
    }
    frameLights_.setLights(std::move(lights), kMaxSceneLights);
    ClusteredLightGrid grid({
        kClusterGridX,
        kClusterGridY,
        kClusterGridZ,
        renderSettings_->cameraNear,
        renderSettings_->cameraFar,
        projection[0],
        projection[5],
        renderSettings_->cameraView && renderSettings_->cameraView->projection
            == CameraProjectionKind::Orthographic,
        renderSettings_->cameraView&&renderSettings_->cameraView->projection==CameraProjectionKind::Orthographic?projection[12]:-projection[8],
        renderSettings_->cameraView&&renderSettings_->cameraView->projection==CameraProjectionKind::Orthographic?projection[13]:-projection[9],
    });
    grid.assign(frameLights_.lights());
    packedLights_ = frameLights_.gpuData();
    packedClusterHeaders_ = grid.gpuHeaderData();
    packedClusterIndices_ = grid.gpuIndexData();
    if (!clusteredLightingReported_) {
        azurerender::RuntimeDiagnostics::instance().print(
            "render",
            "Clustered lighting: "
                + std::to_string(frameLights_.lights().size()) + " lights, "
                + std::to_string(packedClusterIndices_.size())
                + " cluster references");
        clusteredLightingReported_ = true;
    }

    rhi::GpuBuffer& lightBuffer = lightBuffers_[currentFrame_];
    std::memset(
        lightBuffer.mapped, 0, static_cast<std::size_t>(lightBuffer.size));
    if (!packedLights_.empty()) {
        std::memcpy(
            lightBuffer.mapped,
            packedLights_.data(),
            packedLights_.size() * sizeof(RenderLightGpu));
    }
    rhi::GpuBuffer& headerBuffer = clusterHeaderBuffers_[currentFrame_];
    std::memcpy(
        headerBuffer.mapped,
        packedClusterHeaders_.data(),
        packedClusterHeaders_.size()
            * sizeof(std::array<std::uint32_t, 2>));
    rhi::GpuBuffer& indexBuffer = clusterIndexBuffers_[currentFrame_];
    std::memset(
        indexBuffer.mapped, 0, static_cast<std::size_t>(indexBuffer.size));
    if (!packedClusterIndices_.empty()) {
        if (packedClusterIndices_.size()
            > indexBuffer.size / sizeof(std::uint32_t)) {
            throw std::runtime_error(
                "Cluster light index buffer capacity exceeded");
        }
        std::memcpy(
            indexBuffer.mapped,
            packedClusterIndices_.data(),
            packedClusterIndices_.size() * sizeof(std::uint32_t));
    }
}

void CharacterSceneRenderer::recordObjectShadows(const RenderContext& context,const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands){
    for(const auto& page:snapshot.objectShadowPages)recordShadowDraws(context,4,1,&snapshot,&commands,&page);
}

void CharacterSceneRenderer::recordShadowPass(const RenderContext& context) {
    rhi::ICommandRecorder& commands = *context.commands;
    rhi::RenderPassBeginDesc shadowPass{};
    shadowPass.renderPass = context.shadowRenderPass;
    shadowPass.framebuffer = context.shadowFramebuffer;
    shadowPass.extent = {context.shadowMapSize, context.shadowMapSize};
    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0F, 0};
    shadowPass.clearValues = {shadowClear};
    commands.beginRenderPass(shadowPass);
    recordShadowDraws(context, 0, kShadowCascadeCount, instanceSnapshot_.get());
    commands.endRenderPass();
    if (context.gpuTimingEnabled) {
        commands.writeTimestamp(context.timestampQueryPool, 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
}

void CharacterSceneRenderer::recordShadowDraws(const RenderContext& context,
    std::uint32_t firstCascade, std::uint32_t cascadeCount,
    const SceneInstanceSnapshot* snapshot, rhi::ICommandRecorder* recordingCommands,const ObjectShadowPage* objectPage) {
    if (snapshot == nullptr) throw std::logic_error("Shadow recording requires an instance snapshot");
    rhi::ICommandRecorder& commands = recordingCommands != nullptr ? *recordingCommands : *context.commands;

    const std::uint32_t cascadeResolution = objectPage?kObjectShadowTileSize:context.shadowMapSize/2;
    commands.bindPipeline(objectPage?objectShadowPipeline_:shadowPipeline_);
    if (context.submissionCounters != nullptr) {
        ++context.submissionCounters->pipelineBinds;
    }
    if (bindlessTextures_) {
        commands.bindDescriptorSet(
            pipelineLayout_, descriptorSets_[context.currentFrame]);
        if (context.submissionCounters != nullptr) {
            ++context.submissionCounters->descriptorSetBinds;
        }
    }
    for (std::uint32_t cascade = firstCascade;
         cascade < firstCascade + cascadeCount;
         ++cascade) {
        const VkOffset2D cascadeOffset{
            static_cast<std::int32_t>((objectPage?objectPage->page%kObjectShadowColumns:cascade%2)*cascadeResolution),
            static_cast<std::int32_t>((objectPage?objectPage->page/kObjectShadowColumns:cascade/2)*cascadeResolution),
        };
        const float resolution = static_cast<float>(cascadeResolution);
        commands.setViewport(
            resolution, resolution,
            static_cast<float>(cascadeOffset.x),
            static_cast<float>(cascadeOffset.y));
        if(objectPage)commands.setScissor({cascadeResolution-2,cascadeResolution-2},{cascadeOffset.x+1,cascadeOffset.y+1});
        else commands.setScissor({cascadeResolution,cascadeResolution},cascadeOffset);
        commands.pushConstants(
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(MaterialPushConstants) + sizeof(MorphPushConstants),
            &cascade,
            sizeof(cascade));
        if (context.submissionCounters != nullptr) {
            ++context.submissionCounters->pushConstantUpdates;
        }
        std::vector<std::array<std::uint32_t,3>> objectSpans;
        if(objectPage)objectSpans.push_back({snapshot->instances.at(objectPage->instanceIndex).meshKey,objectPage->instanceIndex,1});
        const auto& cascadeSpans=objectPage?objectSpans:snapshot->shadowViewSpans.empty()
            ? snapshot->shadowSpans : snapshot->shadowViewSpans.at(cascade);
        for (const std::array<std::uint32_t, 3>& span : cascadeSpans) {
        const std::uint32_t meshKey = span[0];
        const std::uint32_t firstInstance = span[1];
        const std::uint32_t instanceCount = span[2];
        const auto recordShadowPrimitives = [&](
            const LoadedAsset& mesh,
            const std::uint32_t textureBase,
            const std::size_t globalMaterialBase) {
            for (const AssetPrimitive& primitive : mesh.primitives) {
                const AssetMaterial& material =
                    mesh.materials[primitive.materialIndex];
                const bool explicitCoverage=(material.materialFeatures&MaterialFeatureShadowCoverageMask)!=0;
                if (material.showcasePlatform > 0.5F
                    || (!explicitCoverage&&(material.materialClass == AssetMaterialClass::Overlay
                        || (material.materialFeatures&MaterialFeatureMultiplyComposite)))) {
                    continue;
                }
                if (!bindlessTextures_) {
                    const std::size_t descriptorIndex =
                        context.currentFrame * totalMaterialCount()
                        + globalMaterialBase + primitive.materialIndex;
                    commands.bindDescriptorSet(
                        pipelineLayout_, descriptorSets_[descriptorIndex]);
                    if (context.submissionCounters != nullptr) {
                        ++context.submissionCounters->descriptorSetBinds;
                    }
                }
                const MaterialPushConstants materialConstants{
                    material.alphaCutoff,
                    static_cast<std::uint32_t>(material.alphaMode),
                    material.emissiveStrength,
                    material.showcasePlatform,
                    material.aoColor,
                    material.lamShadowColor,
                    material.materialClass==AssetMaterialClass::Hair?material.hairViewParameters:material.matcapColor,
                    material.hairParameters,
                    material.styleParameters,
                    material.featureParameters,
                    static_cast<std::uint32_t>(material.materialClass),
                    material.materialFeatures,
                    material.materialProfileVersion,
                    static_cast<std::uint32_t>((globalMaterialBase+primitive.materialIndex)<<1U),
                };
                commands.pushConstants(
                    pipelineLayout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    &materialConstants,
                    sizeof(materialConstants));
                MorphPushConstants morphConstants{};
                morphConstants.weights = snapshot->settings.morphWeights;
                morphConstants.gizmoTransform=primitiveGizmoTransform(snapshot->gizmo,&primitive-mesh.primitives.data());
                if(objectPage)morphConstants.gizmoTransform=internal::multiply(objectPage->worldToClip,
                    internal::multiply(snapshot->instances.at(objectPage->instanceIndex).model,morphConstants.gizmoTransform));
                if (bindlessTextures_) {
                    morphConstants.textureBaseIndex =
                        textureBase
                        + primitive.materialIndex * kMaterialTextureSlots;
                }
                forEachPrimitiveOpacityRun(snapshot->nodeWorldOpacities,primitive.nodeIndex,firstInstance,instanceCount,[&](const PrimitiveOpacityRun& run) {
                if (run.opacity == 0) return;
                morphConstants.nodeOpacity=run.opacity;
                commands.pushConstants(
                    pipelineLayout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    sizeof(MaterialPushConstants),
                    &morphConstants,
                    sizeof(morphConstants));
                commands.drawIndexed(
                    primitive.indexCount,
                    primitive.firstIndex,
                    run.instanceCount,
                    run.firstInstance);
                if (context.submissionCounters != nullptr) {
                    ++context.submissionCounters->drawCalls;
                    context.submissionCounters->pushConstantUpdates += 2;
                }
                });
            }
        };
        if (meshKey == 0) {
            commands.bindVertexBuffer(
                snapshot->buffers.vertices.at(meshKey), 0);
            commands.bindIndexBuffer(snapshot->buffers.indices.at(meshKey), 0);
            recordShadowPrimitives(asset_, kSharedTextureSlots, 0);
        } else {
            const AdditionalResource& resource =
                *additionalResources_[meshKey - 1];
            commands.bindVertexBuffer(
                snapshot->buffers.vertices.at(meshKey), 0);
            commands.bindIndexBuffer(snapshot->buffers.indices.at(meshKey), 0);
            recordShadowPrimitives(
                resource.asset,
                resource.textureBase,
                resource.globalMaterialBase);
        }
        }
    }
}

void CharacterSceneRenderer::recordMainPass(const RenderContext& context) {
    rhi::ICommandRecorder& commands = *context.commands;
    commands.beginRenderPass(mainPassDescription(context));
    recordMainDraws(context, 0, instanceSnapshot_.get());
    commands.endRenderPass();
    if (context.gpuTimingEnabled) {
        commands.writeTimestamp(context.timestampQueryPool, 2, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
}

rhi::RenderPassBeginDesc CharacterSceneRenderer::mainPassDescription(const RenderContext& context) const {
    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color.float32[0] = 0.035F;
    clearValues[0].color.float32[1] = 0.055F;
    clearValues[0].color.float32[2] = 0.075F;
    clearValues[0].color.float32[3] = 1.0F;
    clearValues[1].depthStencil = {1.0F, 0};
    clearValues[2].color.float32[0] = 0.5F;
    clearValues[2].color.float32[1] = 0.5F;
    clearValues[2].color.float32[2] = 1.0F;
    clearValues[2].color.float32[3] = 0.0F;
    rhi::RenderPassBeginDesc renderPassInfo{};
    renderPassInfo.renderPass = context.sceneRenderPass;
    renderPassInfo.framebuffer = context.sceneFramebuffer;
    renderPassInfo.extent = context.renderExtent;
    renderPassInfo.clearValues.assign(
        clearValues.begin(), clearValues.end());
    return renderPassInfo;
}

void CharacterSceneRenderer::prepareTransparentIndices() {
    transparentPrimitivesByMesh_.assign(additionalResources_.size() + 1, {});
    for (std::size_t key = 0; key < transparentPrimitivesByMesh_.size(); ++key) {
        const auto& mesh = key == 0 ? asset_ : additionalResources_[key - 1]->asset;
        for (const auto& primitive : mesh.primitives)
            if (mesh.materials[primitive.materialIndex].alphaMode == AssetAlphaMode::Blend)
                transparentPrimitivesByMesh_[key].push_back(&primitive);
    }
    transparentIndexOffsets_.assign(sceneInstances_.size(), 0);
    std::size_t total = 0;
    for (const auto& instance : sceneInstances_) {
        transparentIndexOffsets_[instance.sourceIndex] = total;
        const auto& mesh = instance.meshKey == 0 ? asset_ : additionalResources_[instance.meshKey - 1]->asset;
        for (const auto& primitive : mesh.primitives)
            if (mesh.materials[primitive.materialIndex].alphaMode == AssetAlphaMode::Blend) total += primitive.indexCount;
    }
    transparentDrawsPrepared_ = total != 0;
    if (total == 0) return;
    oitIndexBuffers_.resize(kMaxFramesInFlight);
    auto& buffer = oitIndexBuffers_[currentFrame_];
    if (buffer.size < total * sizeof(std::uint32_t)) {
        allocator_->destroyBuffer(buffer);
        buffer = allocator_->createBuffer(total * sizeof(std::uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true);
    }
    const Vector3 cameraForward = normalize(subtract(cameraTarget_, cameraPosition_));
    for (const auto& instance : sceneInstances_) {
        const auto& mesh = instance.meshKey == 0 ? asset_ : additionalResources_[instance.meshKey - 1]->asset;
        const auto& transparentPrimitives = transparentPrimitivesByMesh_[instance.meshKey];
        const auto& pose = instancePoses_.at(instance.sourceIndex);
        std::vector<Vector3> positions(mesh.vertices.size());
        std::vector<bool> prepared(mesh.vertices.size(),false);
        const auto vertexPosition = [&](std::uint32_t index) {
            if(prepared.at(index))return positions[index];
            positions[index]=deformedVertexPosition(mesh,index,&pose.pose,pose.morph);
            prepared[index]=true;return positions[index];
        };
        std::size_t oitWriteIndex = transparentIndexOffsets_[instance.sourceIndex];
        if (!transparentPrimitives.empty() && !oitIndexBuffers_.empty()) {
            std::uint32_t* oitMapped = static_cast<std::uint32_t*>(
                oitIndexBuffers_[currentFrame_].mapped);
            for (const AssetPrimitive* primitive : transparentPrimitives) {
                const std::uint32_t triangleCount = primitive->indexCount / 3;
                std::vector<std::uint32_t> triangleOrder(triangleCount);
                std::vector<float> triangleDepth(triangleCount);
                for (std::uint32_t triangle = 0; triangle < triangleCount;
                     ++triangle) {
                    triangleOrder[triangle] = triangle;
                    const std::uint32_t base =
                        primitive->firstIndex + triangle * 3;
                    const std::uint32_t i0 = mesh.indices[base];
                    const std::uint32_t i1 = mesh.indices[base + 1];
                    const std::uint32_t i2 = mesh.indices[base + 2];
                    const Vector3 v0 = transformPosition(
                        instance.model, vertexPosition(i0));
                    const Vector3 v1 = transformPosition(
                        instance.model, vertexPosition(i1));
                    const Vector3 v2 = transformPosition(
                        instance.model, vertexPosition(i2));
                    const Vector3 centroid = {
                        (v0[0] + v1[0] + v2[0]) * (1.0F / 3.0F),
                        (v0[1] + v1[1] + v2[1]) * (1.0F / 3.0F),
                        (v0[2] + v1[2] + v2[2]) * (1.0F / 3.0F),
                    };
                    const Vector3 cameraOffset =
                        subtract(centroid, cameraPosition_);
                    triangleDepth[triangle] =
                        dot(cameraOffset, cameraForward);
                }
                std::stable_sort(
                    triangleOrder.begin(),
                    triangleOrder.end(),
                    [&](const std::uint32_t left, const std::uint32_t right) {
                        return triangleDepth[left] > triangleDepth[right];
                    });
                for (std::uint32_t triangle = 0; triangle < triangleCount;
                     ++triangle) {
                    const std::uint32_t ordered = triangleOrder[triangle];
                    const std::uint32_t base =
                        primitive->firstIndex + ordered * 3;
                    oitMapped[oitWriteIndex + triangle * 3] =
                        mesh.indices[base];
                    oitMapped[oitWriteIndex + triangle * 3 + 1] =
                        mesh.indices[base + 1];
                    oitMapped[oitWriteIndex + triangle * 3 + 2] =
                        mesh.indices[base + 2];
                }
                oitWriteIndex += primitive->indexCount;
            }
        }
    }
}

void CharacterSceneRenderer::recordMainDraws(const RenderContext& context, std::uint32_t stage,
    const SceneInstanceSnapshot* snapshot, std::size_t firstTransparent, std::size_t transparentCount, rhi::ICommandRecorder* recordingCommands, SceneSubmissionCounters* recordingCounters) {
    if (snapshot == nullptr) throw std::logic_error("Main recording requires an instance snapshot");
    if (firstTransparent > snapshot->visibleIndices.size())
        throw std::out_of_range("Transparent recording range");
    const auto transparentEnd = firstTransparent
        + std::min(transparentCount, snapshot->visibleIndices.size() - firstTransparent);
    rhi::ICommandRecorder& commands = recordingCommands != nullptr ? *recordingCommands : *context.commands;
    auto* counters = recordingCommands != nullptr ? recordingCounters : context.submissionCounters;

    commands.setViewport(
        static_cast<float>(context.renderExtent.width),
        static_cast<float>(context.renderExtent.height));
    commands.setScissor(context.renderExtent);

    // The global texture array serves every pipeline in this pass, so one
    // bind at the top replaces the per-primitive table switches.
    if (bindlessTextures_) {
        commands.bindDescriptorSet(
            pipelineLayout_, descriptorSets_[context.currentFrame]);
        if (counters != nullptr) {
            ++counters->descriptorSetBinds;
        }
    }

    const RenderSettings& settings = snapshot->settings;
    if (stage == 0 || stage == 1 || stage == 4) {
    if (settings.characterPresentation.backgroundEnabled) {
        commands.bindPipeline(backgroundPipeline_);
        if(counters)++counters->pipelineBinds;
        if (!bindlessTextures_) {
            const std::size_t backgroundDescriptorIndex =
                context.currentFrame * totalMaterialCount();
            commands.bindDescriptorSet(
                pipelineLayout_, descriptorSets_[backgroundDescriptorIndex]);
            if (counters != nullptr) {
                ++counters->descriptorSetBinds;
            }
        }
        commands.draw(3);
        if(counters)++counters->drawCalls;
    }

    }
    if (stage == 0 || stage == 2 || stage == 4) {
    if(settings.sceneLightingEnabled&&context.depthTestEnabled)
        recordDepthCoverage(context,*snapshot,commands,DepthCoverageMode::MainPrefill,counters);
    for (const std::array<std::uint32_t, 3>& span : snapshot->opaqueSpans) {
        const std::uint32_t meshKey = span[0];
        const std::uint32_t firstInstance = span[1];
        const std::uint32_t instanceCount = span[2];
        const auto recordOpaquePrimitives = [&](
            const LoadedAsset& mesh,
            const std::uint32_t textureBase,
            const std::size_t globalMaterialBase) {
            for (const AssetPrimitive& primitive : mesh.primitives) {
                if (mesh.materials[primitive.materialIndex].alphaMode
                    != AssetAlphaMode::Blend) {
                    drawPrimitive(
                        commands,
                        mesh,
                        primitive,
                        primitive.firstIndex,
                        instanceCount,
                        firstInstance,
                        textureBase,
                        globalMaterialBase, counters, snapshot);
                }
            }
        };
        if (meshKey == 0) {
            commands.bindVertexBuffer(
                (snapshot != nullptr ? snapshot->buffers.vertices.at(meshKey) : renderVertexBuffer(meshKey, context.currentFrame).buffer), 0);
            commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(meshKey) : renderIndexBuffer(meshKey).buffer), 0);
            recordOpaquePrimitives(asset_, kSharedTextureSlots, 0);
        } else {
            const AdditionalResource& resource =
                *additionalResources_[meshKey - 1];
            commands.bindVertexBuffer(
                (snapshot != nullptr ? snapshot->buffers.vertices.at(meshKey) : renderVertexBuffer(meshKey, context.currentFrame).buffer), 0);
            commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(meshKey) : renderIndexBuffer(meshKey).buffer), 0);
            recordOpaquePrimitives(
                resource.asset,
                resource.textureBase,
                resource.globalMaterialBase);
        }
    }
    }
    if(stage==0||stage==1||stage==4)recordOutlineShells(context,*snapshot,commands,false,counters);
    if (stage == 0 || stage == 3) {
    for (std::size_t slot = firstTransparent; slot < transparentEnd; ++slot) {
        const scene::SceneInstance* instance = &snapshot->instances.at(snapshot->visibleIndices[slot]);
        const std::uint32_t firstInstance = instance->sourceIndex;
        const std::uint32_t meshKey = instance->meshKey;
        const LoadedAsset& mesh = meshKey == 0
            ? asset_
            : additionalResources_[meshKey - 1]->asset;
        const std::uint32_t textureBase = meshKey == 0
            ? kSharedTextureSlots
            : additionalResources_[meshKey - 1]->textureBase;
        const std::size_t globalMaterialBase = meshKey == 0
            ? 0
            : additionalResources_[meshKey - 1]->globalMaterialBase;
        if (meshKey == 0) {
            commands.bindVertexBuffer(
                (snapshot != nullptr ? snapshot->buffers.vertices.at(meshKey) : renderVertexBuffer(meshKey, context.currentFrame).buffer), 0);
            commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(meshKey) : renderIndexBuffer(meshKey).buffer), 0);
        } else {
            commands.bindVertexBuffer(
                (snapshot != nullptr ? snapshot->buffers.vertices.at(meshKey) : renderVertexBuffer(meshKey, context.currentFrame).buffer), 0);
            commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(meshKey) : renderIndexBuffer(meshKey).buffer), 0);
        }
        std::size_t oitReadIndex = snapshot->transparentOffsets.at(instance->sourceIndex);
        const auto primitiveCount = snapshot != nullptr
            ? snapshot->buffers.transparentPrimitives.at(meshKey).size() : transparentPrimitivesByMesh_[meshKey].size();
        for (std::size_t primitiveSlot = 0; primitiveSlot < primitiveCount; ++primitiveSlot) {
            const auto* primitive = snapshot != nullptr
                ? &mesh.primitives.at(snapshot->buffers.transparentPrimitives[meshKey][primitiveSlot])
                : transparentPrimitivesByMesh_[meshKey][primitiveSlot];
            if (!oitIndexBuffers_.empty()) {
                const VkDeviceSize offsetBytes =
                    static_cast<VkDeviceSize>(oitReadIndex)
                    * sizeof(std::uint32_t);
                commands.bindIndexBuffer(
                    (snapshot != nullptr ? snapshot->buffers.transparentIndices : oitIndexBuffers_[context.currentFrame].buffer),
                    offsetBytes);
            }
            drawPrimitive(
                commands,
                mesh,
                *primitive,
                0,
                1,
                firstInstance,
                textureBase,
                globalMaterialBase, counters, snapshot);
            oitReadIndex += primitive->indexCount;
        }
    }
    }
}

void CharacterSceneRenderer::drawPrimitive(
    rhi::ICommandRecorder& commands,
    const LoadedAsset& mesh,
    const AssetPrimitive& primitive,
    const std::uint32_t firstIndexOffset,
    const std::uint32_t instanceCount,
    const std::uint32_t firstInstance,
    const std::uint32_t textureBase,
    const std::size_t globalMaterialBase,
    SceneSubmissionCounters* counters, const SceneInstanceSnapshot* snapshot) {
    const auto recordRun = [&](const PrimitiveOpacityRun& run) {
    if (run.opacity == 0) return;
    const auto firstInstance=run.firstInstance;
    const auto instanceCount=run.instanceCount;
    const auto frameIndex = snapshot != nullptr ? snapshot->frameIndex : currentFrame_;
    const auto indirectBuffer = snapshot != nullptr ? snapshot->indirectBuffer
        : (gpuCullingFrames_[currentFrame_] ? gpuCullingFrames_[currentFrame_]->output().buffer : VK_NULL_HANDLE);
    const AssetMaterial& material = mesh.materials[primitive.materialIndex];
    if (material.showcasePlatform > 0.5F
        && !(snapshot != nullptr ? snapshot->settings : *renderSettings_).characterPresentation.platformEnabled) {
        return;
    }
    const bool blend = material.alphaMode == AssetAlphaMode::Blend;
    const auto& model = snapshot ? snapshot->instances.at(firstInstance).model : sceneInstances_.at(firstInstance).model;
    const RecordingGizmoState gizmo=snapshot? snapshot->gizmo
        :RecordingGizmoState{selectedPrimitiveIndex_,gizmoActive_,gizmoTranslation_,gizmoRotation_,gizmoScale_};
    const bool mirrored=primitiveMirroredTransform(model,gizmo,&primitive-mesh.primitives.data());
    const bool multiplyComposite=(material.materialFeatures&MaterialFeatureMultiplyComposite)!=0;
    const VkPipeline pipeline = (material.materialFeatures&MaterialFeatureHairPixelIntegral)
        ?hairIntegralPipeline(material,mirrored,false):multiplyComposite
        ? (material.doubleSided?multiplyDoubleSidedPipeline_:mirrored?multiplyMirroredPipeline_:multiplyPipeline_)
        : (material.materialFeatures & MaterialFeatureSceneTint) ? sceneTintPipeline_ : blend
        ? (material.doubleSided ? blendDoubleSidedPipeline_ : mirrored ? blendMirroredPipeline_ : blendPipeline_)
        : ordinaryOpaqueMaterial(material)?ordinaryOpaquePipelines_[material.doubleSided?2:mirrored?1:0]
        : (material.doubleSided ? opaqueDoubleSidedPipeline_ : mirrored ? opaqueMirroredPipeline_ : opaquePipeline_);
    commands.bindPipeline(pipeline);
    if (!bindlessTextures_) {
        const std::size_t descriptorIndex =
            frameIndex * totalMaterialCount()
            + globalMaterialBase + primitive.materialIndex;
        commands.bindDescriptorSet(
            pipelineLayout_, descriptorSets_[descriptorIndex]);
        if (counters != nullptr) {
            ++counters->descriptorSetBinds;
        }
    }
    const MaterialPushConstants materialConstants{
        material.alphaCutoff,
        static_cast<std::uint32_t>(material.alphaMode),
        material.emissiveStrength,
        material.showcasePlatform,
        material.aoColor,
        material.lamShadowColor,
        material.materialClass==AssetMaterialClass::Hair?material.hairViewParameters:material.matcapColor,
        material.hairParameters,
        material.styleParameters,
        material.featureParameters,
            static_cast<std::uint32_t>(material.materialClass),
            materialFeaturesForFrame(material),
            material.materialProfileVersion,
            static_cast<std::uint32_t>((globalMaterialBase+primitive.materialIndex)<<1U) |
            ((snapshot != nullptr ? snapshot->gizmo.selectedPrimitive : selectedPrimitiveIndex_)
                == static_cast<std::int32_t>(
                       &primitive - mesh.primitives.data())
            ? 1U
            : 0U),
    };
    commands.pushConstants(
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        &materialConstants,
        sizeof(materialConstants));
    MorphPushConstants morphConstants{};
    morphConstants.nodeOpacity=run.opacity;
    morphConstants.weights = (snapshot != nullptr ? snapshot->settings : *renderSettings_).morphWeights;
    if (bindlessTextures_) {
        morphConstants.textureBaseIndex =
            textureBase
            + primitive.materialIndex * kMaterialTextureSlots;
    }
    morphConstants.gizmoTransform=primitiveGizmoTransform(gizmo,&primitive-mesh.primitives.data());
    commands.pushConstants(
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(MaterialPushConstants),
        &morphConstants,
        sizeof(morphConstants));
    std::uint32_t recordedDrawCalls = 0;
    const bool useIndirect=!blend&&indirectBuffer!=VK_NULL_HANDLE
        &&firstIndexOffset==primitive.firstIndex&&!material.nprDiffuse.rimOrigin.present;
    if (useIndirect) {
        const auto primitiveIndex = static_cast<std::size_t>(&primitive - mesh.primitives.data());
        if (snapshot != nullptr ? snapshot->buffers.multiDrawIndirect : multiDrawIndirect_) {
            for (std::uint32_t i = 0; i < instanceCount;) {
                const auto count = std::min(snapshot != nullptr ? snapshot->buffers.maxDrawIndirectCount : maxDrawIndirectCount_, instanceCount - i);
                const auto slot = (snapshot != nullptr ? snapshot->indirectOffsets : indirectInstanceOffsets_).at(firstInstance + i) + primitiveIndex;
                commands.drawIndexedIndirect(indirectBuffer,
                    slot * sizeof(VkDrawIndexedIndirectCommand), count,
                    static_cast<std::uint32_t>(mesh.primitives.size() * sizeof(VkDrawIndexedIndirectCommand)));
                i += count;
                ++recordedDrawCalls;
            }
        } else for (std::uint32_t i = 0; i < instanceCount; ++i) {
            const auto slot = (snapshot != nullptr ? snapshot->indirectOffsets : indirectInstanceOffsets_).at(firstInstance + i) + primitiveIndex;
            commands.drawIndexedIndirect(indirectBuffer,
                slot * sizeof(VkDrawIndexedIndirectCommand), 1,
                sizeof(VkDrawIndexedIndirectCommand));
            ++recordedDrawCalls;
        }
    } else {
        commands.drawIndexed(
            primitive.indexCount, firstIndexOffset, instanceCount,
            materialOriginFirstInstance(material,snapshot,firstInstance,instanceCount));
        ++recordedDrawCalls;
    }
    if (counters != nullptr) {
        counters->drawCalls += recordedDrawCalls;
        if (useIndirect)
            counters->indirectDrawCalls += recordedDrawCalls;
        ++counters->pipelineBinds;
        counters->pushConstantUpdates += 2;
    }
    };
    if (snapshot != nullptr)
        forEachPrimitiveOpacityRun(snapshot->nodeWorldOpacities,primitive.nodeIndex,firstInstance,instanceCount,recordRun);
    else recordRun(PrimitiveOpacityRun{firstInstance,instanceCount,1.0F});
}

void CharacterSceneRenderer::buildSceneState() {
    state_.pickables.clear();
    for(const auto& instance:sceneInstances_) {
        const auto* mesh=instance.meshKey==0?&asset_:&additionalResources_[instance.meshKey-1]->asset;
        const auto* pose=instance.sourceIndex<instancePoses_.size()?&instancePoses_[instance.sourceIndex]:nullptr;
        state_.pickables.push_back({instance.nodeId,mesh,instance.model,pose?&pose->pose:nullptr,pose?pose->morph:std::array<float,2>{}});
    }
    state_.asset = &asset_;
    state_.modelMatrix = currentModel_.data();
    state_.selectedPrimitiveIndex = selectedPrimitiveIndex_;
    state_.primitiveCount = asset_.primitives.size();
}

}  // namespace azurerender
