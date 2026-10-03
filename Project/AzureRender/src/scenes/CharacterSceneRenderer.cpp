#include "scenes/CharacterSceneRenderer.hpp"

#include "render/RenderMath.hpp"
#include "platform/BinaryFile.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "render/RenderSettings.hpp"
#include "render/EnvironmentAsset.hpp"
#include "render/ComputePass.hpp"
#include "render/CascadedShadow.hpp"

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

using namespace azurerender::internal;

namespace azurerender {

namespace {

static_assert(sizeof(AssetVertex) == 26 * sizeof(std::uint32_t));

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
    caps.diagnosticViewNames = {
        "Beauty",
        "World Normal",
        "Internal Outline",
        "Shadow Map",
        "Depth",
    };
    return caps;
}

void CharacterSceneRenderer::onLoad(const RenderContext& context) {
    allocator_ = context.allocator;
    if (allocator_ == nullptr) {
        throw std::runtime_error(
            "RenderContext must carry the engine GPU allocator");
    }
    rhi_ = context.rhi;
    if (rhi_ == nullptr) {
        throw std::runtime_error("RenderContext must carry the engine RHI");
    }
    bindlessTextures_ = context.bindlessTextures;
    cullingEnabled_ = context.cullingEnabled;
    computeIblEnabled_ = context.rgba16fStorageImage;
    computeSkinningEnabled_ = context.computeSkinning;
    shaderDirectory_ = context.shaderDirectory;
    qaInstanceCount_ = std::max(context.qaInstanceCount, 1U);
    renderSettings_ = context.renderSettings;
    gpuCullingEnabled_ = context.gpuCulling;
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
    const std::string resolvedAssetPath = context.scene.resources[0].path;
    asset_ = loadGltfAsset(resolvedAssetPath);
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
        azurerender::RuntimeDiagnostics::instance().print(
            "asset",
            "Face SDF: material=" + material.name
                + ", texture=" + std::to_string(material.faceSdf.width) + 'x'
                + std::to_string(material.faceSdf.height)
                + ", headNode=" + material.faceSdf.headNodeName);
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
            "asset", "Face SDF light frame: bind-relative head rotation");
    }
    footPivot_ = estimateFootPivot(asset_);
    appendShowcasePlatform(asset_, footPivot_);
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

    // GPU resources (mirrors the former application init order).
    createVertexBuffer();
    createIndexBuffer();
    createTexture();
    createAdditionalResources();
    createUniformBuffers();
    createJointBuffers();
    createComputeSkinningResources();
    createOitIndexBuffers();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline(context);

    buildSceneState();

}

void CharacterSceneRenderer::onSwapchainRecreate(
    const RenderContext& context) {
    renderSettings_ = context.renderSettings;
    // The scene render pass is recreated by the engine; the character
    // pipelines reference it and must be rebuilt.
    destroyGraphicsPipelinesForRecreate();
    createGraphicsPipeline(context);
}

void CharacterSceneRenderer::updateFrame(const SceneFrameData& frame) {
    if (frame.sceneSnapshot != nullptr) {
        // GPU resources retain their load-time ordering; runtime nodes and
        // lights are refreshed before uniforms, culling and draw snapshots.
        scene_.nodes = frame.sceneSnapshot->nodes;
        scene_.lights = frame.sceneSnapshot->lights;
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
    opaqueRecordingSpans_.clear();
    if (gpuCullingFrames_[currentFrame_]) {
        for (const auto& instance : sceneInstances_) {
            if (!opaqueRecordingSpans_.empty()
                && opaqueRecordingSpans_.back()[0] == instance.meshKey
                && opaqueRecordingSpans_.back()[1] + opaqueRecordingSpans_.back()[2] == instance.sourceIndex)
                ++opaqueRecordingSpans_.back()[2];
            else opaqueRecordingSpans_.push_back({instance.meshKey, instance.sourceIndex, 1});
        }
    } else {
        opaqueRecordingSpans_ = visibleSpansByMeshKey_;
    }
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
    instanceSnapshot_ = std::make_shared<const SceneInstanceSnapshot>(
        std::move(sceneInstances_), std::move(visibleIndices), std::move(opaqueRecordingSpans_),
        std::move(visibleShadowSpansByMeshKey_), std::move(indirectInstanceOffsets_), std::move(transparentIndexOffsets_), std::move(visibleSpansByMeshKey_), *renderSettings_,
        gpuCullingFrames_[currentFrame_] ? gpuCullingFrames_[currentFrame_]->output().buffer : VK_NULL_HANDLE, static_cast<std::uint32_t>(currentFrame_),
        RecordingGizmoState{selectedPrimitiveIndex_, gizmoActive_, gizmoTranslation_, gizmoRotation_, gizmoScale_}, std::move(recordingBuffers));
    visibleInstances_.clear();
}

void CharacterSceneRenderer::rebuildSceneInstances() {
    const auto primaryBounds = morphBounds(asset_, renderSettings_->morphWeights);
    std::vector<scene::AxisAlignedBounds> meshBounds{primaryBounds};
    for (const auto& resource : additionalResources_)
        meshBounds.push_back(morphBounds(resource->asset, renderSettings_->morphWeights));
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
            instance.meshKey = 0;
            sceneInstances_.push_back(instance);
        }
    } else {
        const std::vector<Matrix4> nodeWorldTransforms =
            scene::resolveNodeWorldTransforms(scene_);
        const auto meshKeyOf = [this](const scene::SceneNodeDesc& node) {
            for (std::size_t resourceIndex = 0;
                 resourceIndex < scene_.resources.size();
                 ++resourceIndex) {
                if (scene_.resources[resourceIndex].id == node.resourceId) {
                    return resourceIndex;
                }
            }
            return scene_.resources.size();
        };
        // Instances are grouped by resource so each draw section gets one
        // contiguous span in the instance buffer.
        for (std::size_t meshKey = 0; meshKey < scene_.resources.size();
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
                sceneInstances_.push_back(instance);
            }
        }
    }

    if (gpuCullingEnabled_ && !sceneInstances_.empty()) {
        std::vector<GpuCullBounds> bounds;
        std::vector<VkDrawIndexedIndirectCommand> commands;
        indirectInstanceOffsets_.clear();
        for (const auto& instance : sceneInstances_) {
            indirectInstanceOffsets_.push_back(commands.size());
            bounds.push_back({
                {instance.worldBounds.minimum[0], instance.worldBounds.minimum[1], instance.worldBounds.minimum[2], 0},
                {instance.worldBounds.maximum[0], instance.worldBounds.maximum[1], instance.worldBounds.maximum[2], 0}});
            const auto& mesh = instance.meshKey == 0 ? asset_ : additionalResources_[instance.meshKey - 1]->asset;
            for (const auto& primitive : mesh.primitives)
                commands.push_back({primitive.indexCount, 1, primitive.firstIndex, 0, instance.sourceIndex});
        }
        const auto capacity = std::max(bounds.size(), commands.size());
        if (gpuCullingCapacities_[currentFrame_] < capacity) {
            auto resources = std::make_unique<GpuCullingResources>(*rhi_);
            resources->initialize(azurerender::readBinaryFile(shaderDirectory_ + "/cull_indirect.comp.spv"),
                                  static_cast<std::uint32_t>(capacity));
            gpuCullingFrames_[currentFrame_] = std::move(resources);
            gpuCullingCapacities_[currentFrame_] = capacity;
        }
        gpuCullingFrames_[currentFrame_]->upload(bounds, commands);
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
        destination[slot].meta = {
            meshKey == 0
                ? 0U
                : additionalResources_[meshKey - 1]->jointBase,
            0U,
            0U,
            0U,
        };
        // The same association as the per-frame uniform path so a single
        // instance reproduces the original values exactly.
        destination[slot].modelViewProjection =
            multiply(projectionMatrix_, multiply(viewMatrix_, model));
        for (std::size_t cascade = 0;
             cascade < kShadowCascadeCount;
             ++cascade) {
            destination[slot].cascadeLightModelViewProjection[cascade] =
                multiply(cascadeLightViewProjections_[cascade], model);
        }
    }
}

const rhi::GpuBuffer& CharacterSceneRenderer::renderVertexBuffer(
    const std::uint32_t meshKey,
    const std::uint32_t frameIndex) const {
    if (!computeSkinningEnabled_) {
        return meshKey == 0
            ? vertexBuffer_
            : additionalResources_[meshKey - 1]->vertexBuffer;
    }
    return meshKey == 0
        ? skinnedVertexBuffers_[frameIndex]
        : additionalResources_[meshKey - 1]->skinnedVertexBuffers[frameIndex];
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
    const auto drawVertexBuffer = [this, &sourceVertexBuffer, &context](
        const std::uint32_t meshKey) -> const rhi::GpuBuffer& {
        return computeSkinningEnabled_
            ? (meshKey == 0
                ? skinnedVertexBuffers_[context.currentFrame]
                : additionalResources_[meshKey - 1]
                      ->skinnedVertexBuffers[context.currentFrame])
            : sourceVertexBuffer(meshKey);
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
    const auto clusterHeaderResource = importHostStorage(
        "character-cluster-headers",
        clusterHeaderBuffers_[context.currentFrame]);
    const auto clusterIndexResource = importHostStorage(
        "character-cluster-indices",
        clusterIndexBuffers_[context.currentFrame]);

    std::optional<RenderGraph::PassId> skinningPass;
    if (computeSkinningEnabled_) {
        skinningPass = graph.addCommandPass("character-skinning",
            [this, frozenContext, weights = instanceSnapshot_->settings.morphWeights,
             meshCount = meshResourceCount_](rhi::ICommandRecorder& commands) {
                for (std::uint32_t meshKey = 0; meshKey < meshCount; ++meshKey)
                    recordComputeSkinningMesh(*frozenContext, meshKey, &commands, &weights);
            }, true);
        graph.use(*skinningPass, jointResource, RenderGraphUsage::Storage, false);
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

        const rhi::GpuBuffer& output = drawVertexBuffer(meshKey);
        rhi::BufferBarrierDesc outputInitial{};
        outputInitial.buffer = output.buffer;
        outputInitial.size = output.size;
        outputInitial.dstStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        outputInitial.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        vertexResources[meshKey] = graph.importBuffer(
            "character-vertex-output-" + std::to_string(meshKey),
            outputInitial);

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
            graph.use(*skinningPass, vertexResources[meshKey], RenderGraphUsage::Storage, true);
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
    graph.attachment(shadow, resources.shadow, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
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
    graph.use(main, lightResource, RenderGraphUsage::FragmentStorage, false);
    graph.use(main, clusterHeaderResource, RenderGraphUsage::FragmentStorage, false);
    graph.use(main, clusterIndexResource, RenderGraphUsage::FragmentStorage, false);
    for (std::uint32_t meshKey = 0; meshKey < meshResourceCount_; ++meshKey) {
        graph.use(main, vertexResources[meshKey], RenderGraphUsage::VertexBuffer, false);
        graph.use(main, indexResources[meshKey], RenderGraphUsage::IndexBuffer, false);
    }
    graph.use(main, jointResource, RenderGraphUsage::VertexStorage, false);
    graph.attachment(main, resources.color, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    graph.attachment(main, resources.depth, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    graph.attachment(main, resources.normal, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CharacterSceneRenderer::recordScene(const RenderContext& context) {
    if (!instanceSnapshot_) throw std::logic_error("Direct recording requires a prepared snapshot");
    instanceSnapshot_->validateForRecording();
    recordComputeSkinning(context);
    recordShadowPass(context);
    recordMainPass(context);
}

void CharacterSceneRenderer::onUnload(const RenderContext& context) {
    (void)context;
    destroyResources();
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
    const std::string animationName = asset_.animations.empty()
        ? std::string()
        : asset_.animations[animationIndex_].name;
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
    const std::uint32_t mipLevels) {
    const VkDeviceSize size = static_cast<VkDeviceSize>(pixels.size())
        * sizeof(typename std::decay_t<Pixels>::value_type);
    rhi::GpuBuffer staging = allocator_->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
    std::memcpy(
        staging.mapped, pixels.data(), static_cast<std::size_t>(size));
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
    rhi_->copyBufferToImage(staging, texture.image, width, height);
    if (mipLevels > 1) {
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
    texture.sampler = rhi_->createSampler(samplerDesc);
}

void CharacterSceneRenderer::uploadMaterialTextures(
    const LoadedAsset& asset,
    std::vector<GpuMaterial>& gpuMaterials) {
    gpuMaterials.resize(asset.materials.size());
    for (std::size_t index = 0; index < asset.materials.size(); ++index) {
        const AssetMaterial& material = asset.materials[index];
        GpuMaterial& gpuMaterial = gpuMaterials[index];
        uploadTextureData(
            material.baseColorPixels,
            material.baseColorWidth,
            material.baseColorHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterial.baseColor);
        uploadTextureData(
            material.normalPixels,
            material.normalWidth,
            material.normalHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterial.normal);
        uploadTextureData(
            material.metallicRoughnessPixels,
            material.metallicRoughnessWidth,
            material.metallicRoughnessHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterial.metallicRoughness);
        uploadTextureData(
            material.specularEmissivePixels,
            material.specularEmissiveWidth,
            material.specularEmissiveHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterial.specularEmissive);
        uploadTextureData(
            material.styleMaskPixels,
            material.styleMaskWidth,
            material.styleMaskHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterial.styleMask);
        uploadTextureData(
            material.matcapPixels,
            material.matcapWidth,
            material.matcapHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterial.matcap);
        uploadTextureData(
            material.hairDataPixels,
            material.hairDataWidth,
            material.hairDataHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterial.hairData);
        const std::vector<std::uint8_t> faceSdfPixels =
            material.faceSdf.present
            ? material.faceSdf.pixels
            : std::vector<std::uint8_t>{
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
            };
        uploadTextureData(
            faceSdfPixels,
            material.faceSdf.present ? material.faceSdf.width : 2,
            material.faceSdf.present ? material.faceSdf.height : 2,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterial.faceSdf);
    }
}

void CharacterSceneRenderer::createTexture() {

    const auto loadPpmTexture = [](const std::string& path) {
        std::ifstream stream(path);
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

    uploadMaterialTextures(asset_, gpuMaterials_);

    constexpr std::uint32_t kEnvironmentWidth = 512;
    constexpr std::uint32_t kEnvironmentHeight = 256;
    constexpr float kPi = 3.14159265358979323846F;
    std::vector<std::uint16_t> environmentPixels;
    std::uint32_t environmentWidth = kEnvironmentWidth;
    std::uint32_t environmentHeight = kEnvironmentHeight;
    if (!environmentSource_.path.empty()) {
        const EnvironmentImage environment =
            loadEnvironmentImage(environmentSource_);
        environmentPixels = environment.rgba16f;
        environmentWidth = environment.width;
        environmentHeight = environment.height;
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
        kEnvironmentMipLevels);
    if (computeIblEnabled_) {
        createEnvironmentPrefilter();
    } else {
        azurerender::RuntimeDiagnostics::instance().info(
            "render", "Character IBL: hardware mip fallback path");
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
    if (scene_.resources.size() <= 1) {
        return;
    }
    std::uint32_t jointBase =
        static_cast<std::uint32_t>(asset_.jointMatrices.size());
    std::uint32_t textureBase =
        kSharedTextureSlots
        + kMaterialTextureSlots
            * static_cast<std::uint32_t>(asset_.materials.size());
    std::size_t globalMaterialBase = asset_.materials.size();
    for (std::size_t index = 1; index < scene_.resources.size(); ++index) {
        auto resource = std::make_unique<AdditionalResource>();
        resource->path = scene_.resources[index].path;
        resource->asset = loadGltfAsset(resource->path);
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
    createOutputBuffers(vertexBuffer_.size, skinnedVertexBuffers_);
    for (auto& resource : additionalResources_) {
        createOutputBuffers(
            resource->vertexBuffer.size, resource->skinnedVertexBuffers);
    }

    skinningDescriptorSetLayout_ = rhi_->createDescriptorSetLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    });
    const std::uint32_t descriptorSetCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight) * meshResourceCount_;
    rhi::DescriptorPoolDesc descriptorPool{};
    descriptorPool.sizes = {{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        descriptorSetCount * 3,
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
        const std::array<rhi::DescriptorBufferWrite, 3> writes = {{
            {set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             source.buffer, source.size},
            {set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             jointBuffers_[frame].buffer, jointBuffers_[frame].size},
            {set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             destination.buffer, destination.size},
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
                resource.skinnedVertexBuffers[frame]);
        }
    }
    azurerender::RuntimeDiagnostics::instance().info(
        "render", "Character skinning and morph: Vulkan compute path");
}

void CharacterSceneRenderer::recordComputeSkinningMesh(
    const RenderContext& context,
    const std::uint32_t meshKey, rhi::ICommandRecorder* recorder,
    const std::array<float, 2>* morphWeights) {
    auto& commands = recorder != nullptr ? *recorder : *context.commands;
    const LoadedAsset& mesh = meshKey == 0
        ? asset_
        : additionalResources_[meshKey - 1]->asset;
    const std::uint32_t jointBase = meshKey == 0
        ? 0
        : additionalResources_[meshKey - 1]->jointBase;
    const std::size_t setIndex =
        context.currentFrame * meshResourceCount_ + meshKey;
    const SkinningPushConstants parameters{
        static_cast<std::uint32_t>(mesh.vertices.size()),
        jointBase,
        morphWeights != nullptr ? *morphWeights : renderSettings_->morphWeights,
    };
    commands.pushConstants(
        skinningPipelineLayout_,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        &parameters,
        sizeof(parameters));
    ComputePass dispatch({
        static_cast<std::uint32_t>(mesh.vertices.size()),
        1,
        64,
        1,
        1,
        true,
    });
    dispatch.record(
        commands,
        skinningPipeline_,
        skinningPipelineLayout_,
        skinningDescriptorSets_[setIndex]);
}

void CharacterSceneRenderer::recordComputeSkinning(
    const RenderContext& context) {
    if (!computeSkinningEnabled_) {
        return;
    }
    for (std::uint32_t meshKey = 0; meshKey < meshResourceCount_; ++meshKey) {
        recordComputeSkinningMesh(context, meshKey);
    }
}

void CharacterSceneRenderer::createUniformBuffers() {
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
    instanceCapacity_ = static_cast<std::uint32_t>(
        std::max<std::size_t>(qaInstanceCount_, scene_.nodes.size()));
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
    std::size_t totalJoints = asset_.jointMatrices.size();
    for (const auto& resource : additionalResources_) {
        totalJoints += resource->asset.jointMatrices.size();
    }
    const VkDeviceSize size = sizeof(asset_.jointMatrices.front()) * totalJoints;
    jointBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        jointBuffers_[index] = allocator_->createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            true);
        std::byte* destination =
            static_cast<std::byte*>(jointBuffers_[index].mapped);
        const VkDeviceSize heroBytes =
            sizeof(asset_.jointMatrices.front()) * asset_.jointMatrices.size();
        std::memcpy(
            destination,
            asset_.jointMatrices.data(),
            static_cast<std::size_t>(heroBytes));
        for (const auto& resource : additionalResources_) {
            const VkDeviceSize resourceBytes =
                sizeof(asset_.jointMatrices.front())
                * resource->asset.jointMatrices.size();
            std::memcpy(
                destination + resource->jointBase
                    * sizeof(asset_.jointMatrices.front()),
                resource->asset.jointMatrices.data(),
                static_cast<std::size_t>(resourceBytes));
        }
    }
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
             frameCount * textureSlots},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 5},
        };
        poolDesc.maxSets = frameCount;
        descriptorPool_ = rhi_->createDescriptorPool(poolDesc);
        return;
    }
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight * totalMaterialCount());
    poolDesc.sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * 11},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount * 5},
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
        descriptorSetLayout_ = rhi_->createDescriptorSetLayout({
            {0,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             1,
             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
            {1,
             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             textureSlots,
             VK_SHADER_STAGE_FRAGMENT_BIT},
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
        });
        return;
    }
    constexpr VkShaderStageFlags kFragment = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout_ = rhi_->createDescriptorSetLayout({
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
        {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kFragment},
        {13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kFragment},
        {15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kFragment},
        {16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kFragment},
    });
}

void CharacterSceneRenderer::createDescriptorSets() {
    if (bindlessTextures_) {
        descriptorSets_ = rhi_->allocateDescriptorSets(
            descriptorPool_,
            descriptorSetLayout_,
            static_cast<std::uint32_t>(kMaxFramesInFlight));

        // One array write per frame set: shared slots first, then the
        // per-material blocks in shader-visible order.
        std::vector<VkDescriptorImageInfo> images;
        images.reserve(
            kSharedTextureSlots
            + kMaterialTextureSlots * asset_.materials.size());
        const auto appendTexture = [&images](const GpuTexture& texture) {
            VkDescriptorImageInfo info{};
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.imageView = texture.view;
            info.sampler = texture.sampler;
            images.push_back(info);
        };
        appendTexture(environmentTexture_);
        {
            VkDescriptorImageInfo shadowInfo{};
            shadowInfo.imageLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowInfo.imageView = shadowImageView_;
            shadowInfo.sampler = shadowSampler_;
            images.push_back(shadowInfo);
        }
        appendTexture(toonRampTexture_);
        for (const GpuMaterial& material : gpuMaterials_) {
            appendTexture(material.baseColor);
            appendTexture(material.normal);
            appendTexture(material.metallicRoughness);
            appendTexture(material.specularEmissive);
            appendTexture(material.styleMask);
            appendTexture(material.matcap);
            appendTexture(material.hairData);
            appendTexture(material.faceSdf);
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
                appendTexture(material.faceSdf);
            }
        }

        for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
            rhi::DescriptorBufferWrite uniformWrite{};
            uniformWrite.set = descriptorSets_[frame];
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
        kMaxFramesInFlight * asset_.materials.size();
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
                9,
                shadowImageView_,
                shadowSampler_,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                11,
                toonRampTexture_.view,
                toonRampTexture_.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            writeImage(
                descriptorIndex,
                12,
                gpuMaterial.faceSdf.view,
                gpuMaterial.faceSdf.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            rhi::DescriptorBufferWrite jointWrite{};
            jointWrite.set = descriptorSets_[descriptorIndex];
            jointWrite.binding = 10;
            jointWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            jointWrite.buffer = jointBuffers_[frame].buffer;
            jointWrite.range = jointBuffers_[frame].size;
            rhi_->writeDescriptorBuffer(jointWrite);

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
    const auto vertexCode =
        azurerender::readBinaryFile(shaderDirectory + (computeSkinningEnabled_
            ? "/mesh_compute.vert.spv"
            : "/mesh.vert.spv"));
    const auto fragmentCode =
        azurerender::readBinaryFile(shaderDirectory + (bindlessTextures_
            ? "/mesh_bindless.frag.spv"
            : "/mesh.frag.spv"));
    const auto outlineVertexCode =
        azurerender::readBinaryFile(shaderDirectory + (computeSkinningEnabled_
            ? "/outline_compute.vert.spv"
            : "/outline.vert.spv"));
    const auto outlineFragmentCode =
        azurerender::readBinaryFile(shaderDirectory + "/outline.frag.spv");
    const auto backgroundVertexCode =
        azurerender::readBinaryFile(shaderDirectory + "/background.vert.spv");
    const auto backgroundFragmentCode =
        azurerender::readBinaryFile(shaderDirectory + (bindlessTextures_
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
        };
        const std::vector<rhi::VertexAttributeDesc> shadowAttributes = {
            materialAttributes[0],
            materialAttributes[3],
            materialAttributes[4],
            materialAttributes[5],
            materialAttributes[6],
            materialAttributes[7],
        };
        const std::vector<rhi::VertexAttributeDesc> outlineAttributes = {
            materialAttributes[0],
            materialAttributes[1],
            materialAttributes[4],
            materialAttributes[5],
            materialAttributes[6],
            materialAttributes[7],
        };

        rhi::GraphicsPipelineDesc materialDesc{};
        materialDesc.vertexShader = vertexModule;
        materialDesc.fragmentShader = fragmentModule;
        materialDesc.vertexStride =
            static_cast<std::uint32_t>(sizeof(AssetVertex));
        materialDesc.vertexAttributes = materialAttributes;
        materialDesc.colorAttachmentCount = 2;
        materialDesc.renderPass = context.sceneRenderPass;
        materialDesc.layout = pipelineLayout_;

        materialDesc.cullMode = VK_CULL_MODE_BACK_BIT;
        materialDesc.alphaBlend = false;
        materialDesc.depthWrite = true;
        opaquePipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.cullMode = VK_CULL_MODE_NONE;
        opaqueDoubleSidedPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.cullMode = VK_CULL_MODE_BACK_BIT;
        materialDesc.alphaBlend = true;
        materialDesc.depthWrite = false;
        blendPipeline_ = rhi_->createGraphicsPipeline(materialDesc);
        materialDesc.cullMode = VK_CULL_MODE_NONE;
        blendDoubleSidedPipeline_ = rhi_->createGraphicsPipeline(materialDesc);

        rhi::GraphicsPipelineDesc outlineDesc = materialDesc;
        outlineDesc.vertexShader = outlineVertexModule;
        outlineDesc.fragmentShader = outlineFragmentModule;
        outlineDesc.vertexAttributes = outlineAttributes;
        outlineDesc.cullMode = VK_CULL_MODE_FRONT_BIT;
        outlineDesc.alphaBlend = false;
        outlineDesc.depthWrite = false;
        outlinePipeline_ = rhi_->createGraphicsPipeline(outlineDesc);

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
    for (auto& resources : gpuCullingFrames_) resources.reset();
    gpuCullingCapacities_.fill(0);
    if (rhi_ == nullptr) {
        return;
    }
    for (VkPipeline* pipeline : {
             &opaquePipeline_,
             &opaqueDoubleSidedPipeline_,
             &blendPipeline_,
             &blendDoubleSidedPipeline_,
             &outlinePipeline_,
             &backgroundPipeline_,
             &shadowPipeline_}) {
        if (*pipeline != VK_NULL_HANDLE) {
            rhi_->destroyPipeline(*pipeline);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        rhi_->destroyPipelineLayout(pipelineLayout_);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
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
    for (auto& buffer : jointBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    jointBuffers_.clear();
    for (auto& buffer : oitIndexBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    oitIndexBuffers_.clear();
    for (auto& buffer : instanceBuffers_) {
        allocator_->destroyBuffer(buffer);
    }
    instanceBuffers_.clear();
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
    for (auto& material : gpuMaterials_) {
        for (GpuTexture* texture : {
                 &material.baseColor,
                 &material.normal,
                 &material.metallicRoughness,
                 &material.specularEmissive,
                 &material.styleMask,
                 &material.matcap,
                 &material.hairData,
            &material.faceSdf}) {
            rhi_->destroySampler(texture->sampler);
            rhi_->destroyImageView(texture->view);
            allocator_->destroyImage(texture->image);
        }
    }
    gpuMaterials_.clear();
    for (auto& resource : additionalResources_) {
        allocator_->destroyBuffer(resource->indexBuffer);
        allocator_->destroyBuffer(resource->vertexBuffer);
        for (auto& buffer : resource->skinnedVertexBuffers) {
            allocator_->destroyBuffer(buffer);
        }
        resource->skinnedVertexBuffers.clear();
        for (auto& material : resource->gpuMaterials) {
            for (GpuTexture* texture : {
                     &material.baseColor,
                     &material.normal,
                     &material.metallicRoughness,
                     &material.specularEmissive,
                     &material.styleMask,
                     &material.matcap,
                     &material.hairData,
                     &material.faceSdf}) {
                rhi_->destroySampler(texture->sampler);
                rhi_->destroyImageView(texture->view);
                allocator_->destroyImage(texture->image);
            }
        }
        resource->gpuMaterials.clear();
    }
    additionalResources_.clear();
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
    rhi_->destroyImageView(environmentTexture_.view);
    allocator_->destroyImage(environmentTexture_.image);
    rhi_->destroySampler(toonRampTexture_.sampler);
    rhi_->destroyImageView(toonRampTexture_.view);
    allocator_->destroyImage(toonRampTexture_.image);
}

void CharacterSceneRenderer::destroyGraphicsPipelinesForRecreate() {
    for (VkPipeline* pipeline : {
             &opaquePipeline_,
             &opaqueDoubleSidedPipeline_,
             &blendPipeline_,
             &blendDoubleSidedPipeline_,
             &outlinePipeline_,
             &backgroundPipeline_,
             &shadowPipeline_}) {
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
    if (!asset_.animations.empty()) {
        if (animationPlaying_) {
            animationTime_ += deltaSeconds;
        }
        sampleAnimation(
            asset_,
            animationIndex_,
            animationTime_,
            asset_.jointMatrices);
        const std::size_t jointBytes =
            sizeof(asset_.jointMatrices.front())
            * asset_.jointMatrices.size();
        std::memcpy(
            jointBuffers_[currentFrame_].mapped,
            asset_.jointMatrices.data(),
            jointBytes);
    }

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
    currentModel_ = model;
    const Matrix4 view = lookAt(
        cameraPosition_,
        cameraTarget_,
        {0.0F, 1.0F, 0.0F});
    viewMatrix_ = view;
    const float aspect =
        static_cast<float>(frame.swapchainWidth)
        / static_cast<float>(std::max(frame.swapchainHeight, 1U));
    constexpr float kPi = 3.14159265358979323846F;
    const Matrix4 projection = perspective(kPi / 3.0F, aspect, 0.1F, 100.0F);
    projectionMatrix_ = projection;
    updateClusteredLighting(
        view,
        projection,
        std::max(frame.swapchainWidth, 1U),
        std::max(frame.swapchainHeight, 1U));
    viewFrustum_ =
        scene::extractFrustumPlanes(multiply(projection, view));
    const RenderSettings& settings = *renderSettings_;
    const Vector3 lightDirection = settings.showcasePreset == 1
        ? normalize({0.62F, 0.68F, 0.38F})
        : normalize({0.48F, 0.82F, 0.32F});
    cascadeSplits_ = {};
    const std::vector<float> splits = computeCascadeSplits(
        0.1F, 100.0F, kShadowCascadeCount, 0.65F);
    std::copy(splits.begin(), splits.end(), cascadeSplits_.begin());
    const Vector3 cameraForward = normalize(
        subtract(cameraTarget_, cameraPosition_));
    const Vector3 worldUp = {0.0F, 1.0F, 0.0F};
    const Vector3 cameraRight = normalize(cross(cameraForward, worldUp));
    const Vector3 cameraUp = normalize(cross(cameraRight, cameraForward));
    const float halfFovTangent = std::tan(kPi / 6.0F);
    float cascadeNear = 0.1F;
    for (std::size_t cascade = 0;
         cascade < kShadowCascadeCount;
         ++cascade) {
        const float cascadeFar = cascadeSplits_[cascade];
        std::array<Vector3, 8> corners{};
        std::size_t cornerIndex = 0;
        Vector3 center{};
        for (const float depth : {cascadeNear, cascadeFar}) {
            const float halfHeight = depth * halfFovTangent;
            const float halfWidth = halfHeight * aspect;
            const Vector3 sliceCenter = addVectors(
                cameraPosition_, scaleVector(cameraForward, depth));
            for (const float vertical : {-1.0F, 1.0F}) {
                for (const float horizontal : {-1.0F, 1.0F}) {
                    const Vector3 corner = addVectors(
                        sliceCenter,
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
        0.1F,
        100.0F,
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
        largestExtent * 0.004F,
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
        {1.0F, 1.52F, 0.06F, 0.24F},
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
    Vector3 faceLight = {0.0F, 0.0F, -1.0F};
    bool hasFaceSdf = false;
    if (faceSdfHeadNode_.has_value()
        && *faceSdfHeadNode_ < asset_.nodeWorldMatrices.size()) {
        const float cosine = std::cos(rotationAngle_);
        const float sine = std::sin(rotationAngle_);
        const Vector3 objectLight = normalize({
            cosine * lightDirection[0] - sine * lightDirection[2],
            lightDirection[1],
            sine * lightDirection[0] + cosine * lightDirection[2],
        });
        const auto& head = asset_.nodeWorldMatrices[*faceSdfHeadNode_];
        const Vector3 currentX = normalize({head[0], head[1], head[2]});
        const Vector3 currentY = normalize({head[4], head[5], head[6]});
        const Vector3 currentZ = normalize({head[8], head[9], head[10]});
        const Vector3 jointLocalLight = {
            dot(objectLight, currentX),
            dot(objectLight, currentY),
            dot(objectLight, currentZ),
        };
        const Vector3 bindX = {
            faceSdfBindBasis_[0],
            faceSdfBindBasis_[1],
            faceSdfBindBasis_[2],
        };
        const Vector3 bindY = {
            faceSdfBindBasis_[3],
            faceSdfBindBasis_[4],
            faceSdfBindBasis_[5],
        };
        const Vector3 bindZ = {
            faceSdfBindBasis_[6],
            faceSdfBindBasis_[7],
            faceSdfBindBasis_[8],
        };
        faceLight = normalize({
            bindX[0] * jointLocalLight[0]
                + bindY[0] * jointLocalLight[1]
                + bindZ[0] * jointLocalLight[2],
            bindX[1] * jointLocalLight[0]
                + bindY[1] * jointLocalLight[1]
                + bindZ[1] * jointLocalLight[2],
            bindX[2] * jointLocalLight[0]
                + bindY[2] * jointLocalLight[1]
                + bindZ[2] * jointLocalLight[2],
        });
        hasFaceSdf = true;
    }
    uniform.faceLightDirection = {
        faceLight[0], faceLight[1], faceLight[2], hasFaceSdf ? 1.0F : 0.0F,
    };
    uniform.faceSdfParameters = {
        settings.faceSdf.enabled ? 1.0F : 0.0F,
        settings.faceSdf.threshold,
        settings.faceSdf.softness,
        settings.faceSdf.mirrorHorizontal ? 1.0F : 0.0F,
    };
    uniform.faceSdfShadowColor = settings.faceSdf.shadowColor;
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
    for (const scene::SceneLightDesc& source : scene_.lights) {
        if (!source.enabled) {
            continue;
        }
        const Vector3 viewPosition = transformPosition(view, source.position);
        const float depth = -viewPosition[2];
        if (!(depth > 0.0F) || source.radius <= 0.0F) {
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
        if (!(std::abs(clipW) > 1.0e-6F)) {
            continue;
        }
        RenderLight light{};
        std::uint64_t stableId = 1469598103934665603ULL;
        for (const unsigned char value : source.id) {
            stableId ^= value;
            stableId *= 1099511628211ULL;
        }
        light.stableId = stableId;
        light.position[0] = source.position[0];
        light.position[1] = source.position[1];
        light.position[2] = source.position[2];
        light.clusterPosition[0] = clipX / clipW;
        light.clusterPosition[1] = clipY / clipW;
        light.clusterPosition[2] = depth;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            light.color[channel] = source.color[channel];
        }
        light.intensity = source.intensity;
        light.radius = source.radius;
        lights.push_back(light);
    }
    frameLights_.setLights(std::move(lights), kMaxSceneLights);
    ClusteredLightGrid grid({
        kClusterGridX,
        kClusterGridY,
        kClusterGridZ,
        0.1F,
        100.0F,
        std::abs(projection[0]),
        std::abs(projection[5]),
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
    const SceneInstanceSnapshot* snapshot, rhi::ICommandRecorder* recordingCommands) {
    if (snapshot == nullptr) throw std::logic_error("Shadow recording requires an instance snapshot");
    rhi::ICommandRecorder& commands = recordingCommands != nullptr ? *recordingCommands : *context.commands;

    const std::uint32_t cascadeResolution = context.shadowMapSize / 2;
    commands.bindPipeline(shadowPipeline_);
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
            static_cast<std::int32_t>((cascade % 2) * cascadeResolution),
            static_cast<std::int32_t>((cascade / 2) * cascadeResolution),
        };
        const float resolution = static_cast<float>(cascadeResolution);
        commands.setViewport(
            resolution, resolution,
            static_cast<float>(cascadeOffset.x),
            static_cast<float>(cascadeOffset.y));
        commands.setScissor(
            {cascadeResolution, cascadeResolution}, cascadeOffset);
        commands.pushConstants(
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(MaterialPushConstants) + sizeof(MorphPushConstants),
            &cascade,
            sizeof(cascade));
        if (context.submissionCounters != nullptr) {
            ++context.submissionCounters->pushConstantUpdates;
        }
        for (const std::array<std::uint32_t, 3>& span :
             snapshot->shadowSpans) {
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
                if (material.showcasePlatform > 0.5F
                    || material.materialClass == AssetMaterialClass::Overlay) {
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
                    material.matcapColor,
                    material.hairParameters,
                    material.styleParameters,
                    material.featureParameters,
                    static_cast<std::uint32_t>(material.materialClass),
                    material.materialFeatures,
                    material.materialProfileVersion,
                    0,
                };
                commands.pushConstants(
                    pipelineLayout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    &materialConstants,
                    sizeof(materialConstants));
                MorphPushConstants morphConstants{};
                morphConstants.weights = snapshot->settings.morphWeights;
                if (bindlessTextures_) {
                    morphConstants.textureBaseIndex =
                        textureBase
                        + primitive.materialIndex * kMaterialTextureSlots;
                }
                commands.pushConstants(
                    pipelineLayout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    sizeof(MaterialPushConstants),
                    &morphConstants,
                    sizeof(morphConstants));
                commands.drawIndexed(
                    primitive.indexCount,
                    primitive.firstIndex,
                    instanceCount,
                    firstInstance);
                if (context.submissionCounters != nullptr) {
                    ++context.submissionCounters->drawCalls;
                    context.submissionCounters->pushConstantUpdates += 2;
                }
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
    const Vector3 cameraForward = normalize({-cameraPosition_[0], -cameraPosition_[1], -cameraPosition_[2]});
    for (const auto& instance : sceneInstances_) {
        const auto& mesh = instance.meshKey == 0 ? asset_ : additionalResources_[instance.meshKey - 1]->asset;
        const auto& transparentPrimitives = transparentPrimitivesByMesh_[instance.meshKey];
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
                        instance.model, mesh.vertices[i0].position);
                    const Vector3 v1 = transformPosition(
                        instance.model, mesh.vertices[i1].position);
                    const Vector3 v2 = transformPosition(
                        instance.model, mesh.vertices[i2].position);
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
        if (!bindlessTextures_) {
            const std::size_t backgroundDescriptorIndex =
                context.currentFrame * asset_.materials.size();
            commands.bindDescriptorSet(
                pipelineLayout_, descriptorSets_[backgroundDescriptorIndex]);
            if (counters != nullptr) {
                ++counters->descriptorSetBinds;
            }
        }
        commands.draw(3);
    }

    commands.bindVertexBuffer(
        (snapshot != nullptr ? snapshot->buffers.vertices.at(0) : renderVertexBuffer(0, context.currentFrame).buffer), 0);
    commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(0) : renderIndexBuffer(0).buffer), 0);
    if (settings.silhouetteOutlineEnabled) {
        commands.bindPipeline(outlinePipeline_);
        MorphPushConstants outlineMorphConstants{};
        outlineMorphConstants.weights = settings.morphWeights;
        commands.pushConstants(
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(MaterialPushConstants),
            &outlineMorphConstants,
            sizeof(outlineMorphConstants));
        if (counters != nullptr) {
            ++counters->pushConstantUpdates;
        }
        for (const std::array<std::uint32_t, 3>& span :
             snapshot->visibleSpans) {
            const std::uint32_t meshKey = span[0];
            const std::uint32_t firstInstance = span[1];
            const std::uint32_t instanceCount = span[2];
            const auto recordOutlinePrimitives = [&](
                const LoadedAsset& mesh,
                const std::size_t globalMaterialBase) {
                for (const AssetPrimitive& primitive : mesh.primitives) {
                    const AssetMaterial& outlineMaterial =
                        mesh.materials[primitive.materialIndex];
                    if (!settings.characterPresentation.platformEnabled
                        && outlineMaterial.showcasePlatform > 0.5F) {
                        continue;
                    }
                    // The turntable is a receiver, not character silhouette
                    // geometry; overlay cards likewise produce oversized
                    // black shells.
                    if (outlineMaterial.showcasePlatform > 0.5F
                        || outlineMaterial.materialClass
                            == AssetMaterialClass::Overlay
                        || outlineMaterial.alphaMode == AssetAlphaMode::Blend) {
                        continue;
                    }
                    if (!bindlessTextures_) {
                        const std::size_t descriptorIndex =
                            context.currentFrame * totalMaterialCount()
                            + globalMaterialBase + primitive.materialIndex;
                        commands.bindDescriptorSet(
                            pipelineLayout_, descriptorSets_[descriptorIndex]);
                        if (counters != nullptr) {
                            ++counters->descriptorSetBinds;
                        }
                    }
                    commands.drawIndexed(
                        primitive.indexCount,
                        primitive.firstIndex,
                        instanceCount,
                        firstInstance);
                    if (counters != nullptr) {
                        ++counters->drawCalls;
                    }
                }
            };
            if (meshKey == 0) {
                commands.bindVertexBuffer(
                    (snapshot != nullptr ? snapshot->buffers.vertices.at(meshKey) : renderVertexBuffer(meshKey, context.currentFrame).buffer), 0);
                commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(meshKey) : renderIndexBuffer(meshKey).buffer), 0);
                recordOutlinePrimitives(asset_, 0);
            } else {
                const AdditionalResource& resource =
                    *additionalResources_[meshKey - 1];
                commands.bindVertexBuffer(
                    (snapshot != nullptr ? snapshot->buffers.vertices.at(meshKey) : renderVertexBuffer(meshKey, context.currentFrame).buffer), 0);
                commands.bindIndexBuffer((snapshot != nullptr ? snapshot->buffers.indices.at(meshKey) : renderIndexBuffer(meshKey).buffer), 0);
                recordOutlinePrimitives(
                    resource.asset, resource.globalMaterialBase);
            }
        }
    }
    }
    if (stage == 0 || stage == 2 || stage == 4) {
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
    const auto frameIndex = snapshot != nullptr ? snapshot->frameIndex : currentFrame_;
    const auto indirectBuffer = snapshot != nullptr ? snapshot->indirectBuffer
        : (gpuCullingFrames_[currentFrame_] ? gpuCullingFrames_[currentFrame_]->output().buffer : VK_NULL_HANDLE);
    const AssetMaterial& material = mesh.materials[primitive.materialIndex];
    if (material.showcasePlatform > 0.5F
        && !(snapshot != nullptr ? snapshot->settings : *renderSettings_).characterPresentation.platformEnabled) {
        return;
    }
    const bool blend = material.alphaMode == AssetAlphaMode::Blend;
    const VkPipeline pipeline = blend
        ? (material.doubleSided ? blendDoubleSidedPipeline_ : blendPipeline_)
        : (material.doubleSided ? opaqueDoubleSidedPipeline_ : opaquePipeline_);
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
        material.matcapColor,
        material.hairParameters,
        material.styleParameters,
        material.featureParameters,
            static_cast<std::uint32_t>(material.materialClass),
            material.materialFeatures,
            material.materialProfileVersion,
            (snapshot != nullptr ? snapshot->gizmo.selectedPrimitive : selectedPrimitiveIndex_)
                == static_cast<std::int32_t>(
                       &primitive - mesh.primitives.data())
            ? 1U
            : 0U,
    };
    commands.pushConstants(
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        &materialConstants,
        sizeof(materialConstants));
    MorphPushConstants morphConstants{};
    morphConstants.weights = (snapshot != nullptr ? snapshot->settings : *renderSettings_).morphWeights;
    if (bindlessTextures_) {
        morphConstants.textureBaseIndex =
            textureBase
            + primitive.materialIndex * kMaterialTextureSlots;
    }
    morphConstants.gizmoTransform =
        [&]() -> std::array<float, 16> {
            if ((snapshot != nullptr ? snapshot->gizmo.selectedPrimitive : selectedPrimitiveIndex_)
                != static_cast<std::int32_t>(
                       &primitive - mesh.primitives.data())) {
                return {1.0F, 0.0F, 0.0F, 0.0F,
                        0.0F, 1.0F, 0.0F, 0.0F,
                        0.0F, 0.0F, 1.0F, 0.0F,
                        0.0F, 0.0F, 0.0F, 1.0F};
            }
            constexpr float kPi = 3.14159265358979323846F;
            std::array<float, 3> gizmoTranslation{0.0F, 0.0F, 0.0F};
            std::array<float, 3> gizmoRotation{0.0F, 0.0F, 0.0F};
            std::array<float, 3> gizmoScale{1.0F, 1.0F, 1.0F};
            if (snapshot != nullptr ? snapshot->gizmo.active : gizmoActive_) {
                gizmoTranslation = snapshot != nullptr ? snapshot->gizmo.translation : gizmoTranslation_;
                gizmoRotation = snapshot != nullptr ? snapshot->gizmo.rotation : gizmoRotation_;
                gizmoScale = snapshot != nullptr ? snapshot->gizmo.scale : gizmoScale_;
            }
            const Matrix4 gizmoTransform = multiply(
                translation(
                    gizmoTranslation[0],
                    gizmoTranslation[1],
                    gizmoTranslation[2]),
                multiply(
                    multiply(
                        rotationX(gizmoRotation[0] * kPi / 180.0F),
                        rotationY(gizmoRotation[1] * kPi / 180.0F)),
                    multiply(
                        rotationZ(gizmoRotation[2] * kPi / 180.0F),
                        scale(
                            gizmoScale[0],
                            gizmoScale[1],
                            gizmoScale[2]))));
            return {
                gizmoTransform[0], gizmoTransform[1],
                gizmoTransform[2], gizmoTransform[3],
                gizmoTransform[4], gizmoTransform[5],
                gizmoTransform[6], gizmoTransform[7],
                gizmoTransform[8], gizmoTransform[9],
                gizmoTransform[10], gizmoTransform[11],
                gizmoTransform[12], gizmoTransform[13],
                gizmoTransform[14], gizmoTransform[15],
            };
        }();
    commands.pushConstants(
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(MaterialPushConstants),
        &morphConstants,
        sizeof(morphConstants));
    std::uint32_t recordedDrawCalls = 0;
    if (!blend && indirectBuffer != VK_NULL_HANDLE
        && firstIndexOffset == primitive.firstIndex) {
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
            primitive.indexCount, firstIndexOffset, instanceCount, firstInstance);
        ++recordedDrawCalls;
    }
    if (counters != nullptr) {
        counters->drawCalls += recordedDrawCalls;
        if (!blend && indirectBuffer != VK_NULL_HANDLE
            && firstIndexOffset == primitive.firstIndex)
            counters->indirectDrawCalls += recordedDrawCalls;
        ++counters->pipelineBinds;
        counters->pushConstantUpdates += 2;
    }
}

void CharacterSceneRenderer::buildSceneState() {
    state_.asset = &asset_;
    state_.modelMatrix = currentModel_.data();
    state_.selectedPrimitiveIndex = selectedPrimitiveIndex_;
    state_.primitiveCount = asset_.primitives.size();
}

}  // namespace azurerender
