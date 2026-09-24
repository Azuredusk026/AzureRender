#include "scenes/CharacterSceneRenderer.hpp"

#include "app/AzureRenderInternal.hpp"
#include "platform/BinaryFile.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "render/RenderSettings.hpp"
#include "render/EnvironmentAsset.hpp"

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
    qaInstanceCount_ = std::max(context.qaInstanceCount, 1U);
    renderSettings_ = context.renderSettings;
    rampAtlasPath_ = context.rampAtlasPath;
    environmentSource_ = context.environment;
    shadowImageView_ = context.shadowImageView;
    shadowSampler_ = context.shadowSampler;

    // Load and validate the glTF asset (mirrors the former application init).
    const std::string resolvedAssetPath = context.assetPath;
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
             << ", " << asset_.jointMatrices.size() << " joint matrices";
    azurerender::RuntimeDiagnostics::instance().print("asset", skinning.str());
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
    createUniformBuffers();
    createJointBuffers();
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
    buildSceneState();
}

void CharacterSceneRenderer::rebuildSceneInstances() {
    // The default asset scene contributes one instance; the QA stress knob
    // clones it onto a grid to prove draw counts stay flat as instances grow.
    sceneInstances_.clear();
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
            {asset_.boundsMin, asset_.boundsMax}, instance.model);
        instance.sourceIndex = index;
        instance.meshKey = 0;
        sceneInstances_.push_back(instance);
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

    if (visibleInstances_.empty() || instanceBuffers_.empty()) {
        return;
    }
    auto* destination = static_cast<InstanceGpuData*>(
        instanceBuffers_[currentFrame_].mapped);
    const std::size_t visibleCount = visibleInstances_.size();
    for (std::size_t slot = 0; slot < visibleCount; ++slot) {
        const Matrix4& model = visibleInstances_[slot]->model;
        destination[slot].model = model;
        // The same association as the per-frame uniform path so a single
        // instance reproduces the original values exactly.
        destination[slot].modelViewProjection =
            multiply(projectionMatrix_, multiply(viewMatrix_, model));
        destination[slot].lightModelViewProjection = multiply(
            lightProjectionMatrix_, multiply(lightViewMatrix_, model));
    }
}

void CharacterSceneRenderer::recordScene(const RenderContext& context) {
    submissionCounters_ = context.submissionCounters;
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

void CharacterSceneRenderer::createVertexBuffer() {
    const VkDeviceSize size = sizeof(AssetVertex) * asset_.vertices.size();
    rhi::GpuBuffer staging = allocator_->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
    std::memcpy(
        staging.mapped, asset_.vertices.data(), static_cast<std::size_t>(size));
    vertexBuffer_ = allocator_->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        false);
    rhi_->copyBuffer(staging, vertexBuffer_, size);
    allocator_->destroyBuffer(staging);
}

void CharacterSceneRenderer::createIndexBuffer() {
    const VkDeviceSize size = sizeof(std::uint32_t) * asset_.indices.size();
    rhi::GpuBuffer staging = allocator_->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
    std::memcpy(
        staging.mapped, asset_.indices.data(), static_cast<std::size_t>(size));
    indexBuffer_ = allocator_->createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        false);
    rhi_->copyBuffer(staging, indexBuffer_, size);
    allocator_->destroyBuffer(staging);
}

void CharacterSceneRenderer::createTexture() {
    const auto uploadTexture = [this](
        const auto& pixels,
        const std::uint32_t width,
        const std::uint32_t height,
        const VkFormat format,
        const bool clampVertical,
        GpuTexture& texture,
        const std::uint32_t mipLevels = 1) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(pixels.size())
            * sizeof(typename std::decay_t<decltype(pixels)>::value_type);
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
                | (mipLevels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0),
            mipLevels);
        rhi_->transitionImageLayout(
            texture.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels);
        rhi_->copyBufferToImage(staging, texture.image, width, height);
        if (mipLevels > 1) {
            rhi_->generateMipmaps(
                texture.image, format, width, height, mipLevels);
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
    };

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

    gpuMaterials_.resize(asset_.materials.size());
    for (std::size_t index = 0; index < asset_.materials.size(); ++index) {
        const AssetMaterial& material = asset_.materials[index];
        uploadTexture(
            material.baseColorPixels,
            material.baseColorWidth,
            material.baseColorHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterials_[index].baseColor);
        uploadTexture(
            material.normalPixels,
            material.normalWidth,
            material.normalHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].normal);
        uploadTexture(
            material.metallicRoughnessPixels,
            material.metallicRoughnessWidth,
            material.metallicRoughnessHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].metallicRoughness);
        uploadTexture(
            material.specularEmissivePixels,
            material.specularEmissiveWidth,
            material.specularEmissiveHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterials_[index].specularEmissive);
        uploadTexture(
            material.styleMaskPixels,
            material.styleMaskWidth,
            material.styleMaskHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].styleMask);
        uploadTexture(
            material.matcapPixels,
            material.matcapWidth,
            material.matcapHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterials_[index].matcap);
        uploadTexture(
            material.hairDataPixels,
            material.hairDataWidth,
            material.hairDataHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].hairData);
        const std::vector<std::uint8_t> faceSdfPixels =
            material.faceSdf.present
            ? material.faceSdf.pixels
            : std::vector<std::uint8_t>{
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
            };
        uploadTexture(
            faceSdfPixels,
            material.faceSdf.present ? material.faceSdf.width : 2,
            material.faceSdf.present ? material.faceSdf.height : 2,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].faceSdf);
    }

    constexpr std::uint32_t kEnvironmentWidth = 512;
    constexpr std::uint32_t kEnvironmentHeight = 256;
    constexpr std::uint32_t kEnvironmentMipLevels = 7;
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
    uploadTexture(
        environmentPixels,
        environmentWidth,
        environmentHeight,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        true,
        environmentTexture_,
        kEnvironmentMipLevels);

    const auto toonRamp = loadPpmTexture(rampAtlasPath_);
    uploadTexture(
        toonRamp.pixels,
        toonRamp.width,
        toonRamp.height,
        VK_FORMAT_R8G8B8A8_UNORM,
        true,
        toonRampTexture_);
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
    instanceCapacity_ = std::max(qaInstanceCount_, 1U);
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
    const VkDeviceSize size =
        sizeof(asset_.jointMatrices.front()) * asset_.jointMatrices.size();
    jointBuffers_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        jointBuffers_[index] = allocator_->createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            true);
        std::memcpy(
            jointBuffers_[index].mapped,
            asset_.jointMatrices.data(),
            static_cast<std::size_t>(size));
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
                * static_cast<std::uint32_t>(asset_.materials.size());
        poolDesc.sizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             frameCount * textureSlots},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2},
        };
        poolDesc.maxSets = frameCount;
        descriptorPool_ = rhi_->createDescriptorPool(poolDesc);
        return;
    }
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight * asset_.materials.size());
    poolDesc.sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * 11},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount * 2},
    };
    poolDesc.maxSets = descriptorCount;
    descriptorPool_ = rhi_->createDescriptorPool(poolDesc);
}

void CharacterSceneRenderer::createDescriptorSetLayout() {
    if (bindlessTextures_) {
        const std::uint32_t textureSlots =
            kSharedTextureSlots
            + kMaterialTextureSlots
                * static_cast<std::uint32_t>(asset_.materials.size());
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
            jointWrite.range = sizeof(asset_.jointMatrices.front())
                * asset_.jointMatrices.size();
            rhi_->writeDescriptorBuffer(jointWrite);

            rhi::DescriptorBufferWrite instanceWrite{};
            instanceWrite.set = descriptorSets_[frame];
            instanceWrite.binding = 13;
            instanceWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            instanceWrite.buffer = instanceBuffers_[frame].buffer;
            instanceWrite.range = static_cast<VkDeviceSize>(instanceCapacity_)
                * sizeof(InstanceGpuData);
            rhi_->writeDescriptorBuffer(instanceWrite);
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
        for (std::size_t material = 0; material < asset_.materials.size();
             ++material) {
            const std::size_t descriptorIndex =
                frame * asset_.materials.size() + material;
            const GpuMaterial& gpuMaterial = gpuMaterials_[material];

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
            jointWrite.range = sizeof(asset_.jointMatrices.front())
                * asset_.jointMatrices.size();
            rhi_->writeDescriptorBuffer(jointWrite);

            rhi::DescriptorBufferWrite instanceWrite{};
            instanceWrite.set = descriptorSets_[descriptorIndex];
            instanceWrite.binding = 13;
            instanceWrite.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            instanceWrite.buffer = instanceBuffers_[frame].buffer;
            instanceWrite.range = static_cast<VkDeviceSize>(instanceCapacity_)
                * sizeof(InstanceGpuData);
            rhi_->writeDescriptorBuffer(instanceWrite);
        }
    }
}


void CharacterSceneRenderer::createGraphicsPipeline(
    const RenderContext& context) {
    const std::string shaderDirectory = context.shaderDirectory;
    const auto vertexCode =
        azurerender::readBinaryFile(shaderDirectory + "/mesh.vert.spv");
    const auto fragmentCode =
        azurerender::readBinaryFile(shaderDirectory + (bindlessTextures_
            ? "/mesh_bindless.frag.spv"
            : "/mesh.frag.spv"));
    const auto outlineVertexCode =
        azurerender::readBinaryFile(shaderDirectory + "/outline.vert.spv");
    const auto outlineFragmentCode =
        azurerender::readBinaryFile(shaderDirectory + "/outline.frag.spv");
    const auto backgroundVertexCode =
        azurerender::readBinaryFile(shaderDirectory + "/background.vert.spv");
    const auto backgroundFragmentCode =
        azurerender::readBinaryFile(shaderDirectory + (bindlessTextures_
            ? "/background_bindless.frag.spv"
            : "/background.frag.spv"));
    const auto shadowVertexCode =
        azurerender::readBinaryFile(shaderDirectory + "/shadow.vert.spv");
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
            sizeof(MaterialPushConstants) + sizeof(MorphPushConstants),
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
        };
        const std::vector<rhi::VertexAttributeDesc> outlineAttributes = {
            materialAttributes[0],
            materialAttributes[1],
            materialAttributes[4],
            materialAttributes[5],
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
    viewFrustum_ =
        scene::extractFrustumPlanes(multiply(projection, view));
    const RenderSettings& settings = *renderSettings_;
    const Vector3 lightDirection = settings.showcasePreset == 1
        ? normalize({0.62F, 0.68F, 0.38F})
        : normalize({0.48F, 0.82F, 0.32F});
    const Vector3 lightTarget = {0.0F, -0.10F, 0.0F};
    const Vector3 lightPosition = {
        lightTarget[0] + lightDirection[0] * 4.5F,
        lightTarget[1] + lightDirection[1] * 4.5F,
        lightTarget[2] + lightDirection[2] * 4.5F,
    };
    const Matrix4 lightView = lookAt(
        lightPosition,
        lightTarget,
        {0.0F, 1.0F, 0.0F});
    const Matrix4 lightProjection = orthographic(
        -1.90F, 1.90F, -1.90F, 1.90F, 0.10F, 8.0F);
    lightViewMatrix_ = lightView;
    lightProjectionMatrix_ = lightProjection;

    UniformBufferObject uniform{};
    uniform.cameraPosition = {
        cameraPosition_[0], cameraPosition_[1], cameraPosition_[2], 1.0F,
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

    const float shadowSize = static_cast<float>(context.shadowMapSize);
    commands.setViewport(shadowSize, shadowSize);
    commands.setScissor({context.shadowMapSize, context.shadowMapSize});
    commands.bindVertexBuffer(vertexBuffer_.buffer, 0);
    commands.bindIndexBuffer(indexBuffer_.buffer, 0);
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
    const std::uint32_t visibleCount =
        static_cast<std::uint32_t>(visibleInstances_.size());
    if (visibleCount > 0) {
        for (const AssetPrimitive& primitive : asset_.primitives) {
            const AssetMaterial& material =
                asset_.materials[primitive.materialIndex];
            if (material.showcasePlatform > 0.5F
                || material.materialClass == AssetMaterialClass::Overlay) {
                continue;
            }
            if (!bindlessTextures_) {
                const std::size_t descriptorIndex =
                    context.currentFrame * asset_.materials.size()
                    + primitive.materialIndex;
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
            morphConstants.weights = renderSettings_->morphWeights;
            if (bindlessTextures_) {
                morphConstants.textureBaseIndex =
                    kSharedTextureSlots
                    + primitive.materialIndex * kMaterialTextureSlots;
            }
            commands.pushConstants(
                pipelineLayout_,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                sizeof(MaterialPushConstants),
                &morphConstants,
                sizeof(morphConstants));
            commands.drawIndexed(
                primitive.indexCount, primitive.firstIndex, visibleCount);
            if (context.submissionCounters != nullptr) {
                ++context.submissionCounters->drawCalls;
                context.submissionCounters->pushConstantUpdates += 2;
            }
        }
    }
    commands.endRenderPass();
    if (context.gpuTimingEnabled) {
        commands.writeTimestamp(context.timestampQueryPool, 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
}

void CharacterSceneRenderer::recordMainPass(const RenderContext& context) {
    rhi::ICommandRecorder& commands = *context.commands;
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
    commands.beginRenderPass(renderPassInfo);

    commands.setViewport(
        static_cast<float>(context.renderExtent.width),
        static_cast<float>(context.renderExtent.height));
    commands.setScissor(context.renderExtent);

    // The global texture array serves every pipeline in this pass, so one
    // bind at the top replaces the per-primitive table switches.
    if (bindlessTextures_) {
        commands.bindDescriptorSet(
            pipelineLayout_, descriptorSets_[context.currentFrame]);
        if (context.submissionCounters != nullptr) {
            ++context.submissionCounters->descriptorSetBinds;
        }
    }

    const RenderSettings& settings = *renderSettings_;
    if (settings.characterPresentation.backgroundEnabled) {
        commands.bindPipeline(backgroundPipeline_);
        if (!bindlessTextures_) {
            const std::size_t backgroundDescriptorIndex =
                context.currentFrame * asset_.materials.size();
            commands.bindDescriptorSet(
                pipelineLayout_, descriptorSets_[backgroundDescriptorIndex]);
            if (context.submissionCounters != nullptr) {
                ++context.submissionCounters->descriptorSetBinds;
            }
        }
        commands.draw(3);
    }

    commands.bindVertexBuffer(vertexBuffer_.buffer, 0);
    commands.bindIndexBuffer(indexBuffer_.buffer, 0);
    if (settings.silhouetteOutlineEnabled) {
        commands.bindPipeline(outlinePipeline_);
        if (!visibleInstances_.empty()) {
            for (const AssetPrimitive& primitive : asset_.primitives) {
                const AssetMaterial& outlineMaterial =
                    asset_.materials[primitive.materialIndex];
                if (!settings.characterPresentation.platformEnabled
                    && outlineMaterial.showcasePlatform > 0.5F) {
                    continue;
                }
                // The turntable is a receiver, not character silhouette geometry;
                // overlay cards likewise produce oversized black shells.
                if (outlineMaterial.showcasePlatform > 0.5F
                    || outlineMaterial.materialClass == AssetMaterialClass::Overlay
                    || outlineMaterial.alphaMode == AssetAlphaMode::Blend) {
                    continue;
                }
                if (!bindlessTextures_) {
                    const std::size_t descriptorIndex =
                        context.currentFrame * asset_.materials.size()
                        + primitive.materialIndex;
                    commands.bindDescriptorSet(
                        pipelineLayout_, descriptorSets_[descriptorIndex]);
                    if (context.submissionCounters != nullptr) {
                        ++context.submissionCounters->descriptorSetBinds;
                    }
                }
                commands.drawIndexed(
                    primitive.indexCount,
                    primitive.firstIndex,
                    static_cast<std::uint32_t>(visibleInstances_.size()));
                if (context.submissionCounters != nullptr) {
                    ++context.submissionCounters->drawCalls;
                }
            }
        }
    }
    if (!visibleInstances_.empty()) {
        for (const AssetPrimitive& primitive : asset_.primitives) {
            if (asset_.materials[primitive.materialIndex].alphaMode
                != AssetAlphaMode::Blend) {
                drawPrimitive(
                    commands,
                    primitive,
                    primitive.firstIndex,
                    static_cast<std::uint32_t>(visibleInstances_.size()),
                    0);
            }
        }
    }
    std::vector<const AssetPrimitive*> transparentPrimitives;
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode
            == AssetAlphaMode::Blend) {
            transparentPrimitives.push_back(&primitive);
        }
    }
    const Vector3 cameraForward = normalize({
        -cameraPosition_[0],
        -cameraPosition_[1],
        -cameraPosition_[2],
    });
    for (std::size_t slot = 0; slot < visibleInstances_.size(); ++slot) {
        const scene::SceneInstance* instance = visibleInstances_[slot];
        const std::uint32_t firstInstance =
            static_cast<std::uint32_t>(slot);
        std::size_t oitWriteIndex = 0;
        if (!transparentPrimitives.empty() && !oitIndexBuffers_.empty()) {
            std::uint32_t* oitMapped = static_cast<std::uint32_t*>(
                oitIndexBuffers_[context.currentFrame].mapped);
            for (const AssetPrimitive* primitive : transparentPrimitives) {
                const std::uint32_t triangleCount = primitive->indexCount / 3;
                std::vector<std::uint32_t> triangleOrder(triangleCount);
                std::vector<float> triangleDepth(triangleCount);
                for (std::uint32_t triangle = 0; triangle < triangleCount;
                     ++triangle) {
                    triangleOrder[triangle] = triangle;
                    const std::uint32_t base =
                        primitive->firstIndex + triangle * 3;
                    const std::uint32_t i0 = asset_.indices[base];
                    const std::uint32_t i1 = asset_.indices[base + 1];
                    const std::uint32_t i2 = asset_.indices[base + 2];
                    const Vector3 v0 = transformPosition(
                        instance->model, asset_.vertices[i0].position);
                    const Vector3 v1 = transformPosition(
                        instance->model, asset_.vertices[i1].position);
                    const Vector3 v2 = transformPosition(
                        instance->model, asset_.vertices[i2].position);
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
                        asset_.indices[base];
                    oitMapped[oitWriteIndex + triangle * 3 + 1] =
                        asset_.indices[base + 1];
                    oitMapped[oitWriteIndex + triangle * 3 + 2] =
                        asset_.indices[base + 2];
                }
                oitWriteIndex += primitive->indexCount;
            }
        }
        std::size_t oitReadIndex = 0;
        for (const AssetPrimitive* primitive : transparentPrimitives) {
            if (!oitIndexBuffers_.empty()) {
                const VkDeviceSize offsetBytes =
                    static_cast<VkDeviceSize>(oitReadIndex)
                    * sizeof(std::uint32_t);
                commands.bindIndexBuffer(
                    oitIndexBuffers_[context.currentFrame].buffer,
                    offsetBytes);
            }
            drawPrimitive(commands, *primitive, 0, 1, firstInstance);
            oitReadIndex += primitive->indexCount;
        }
    }
    commands.endRenderPass();
    if (context.gpuTimingEnabled) {
        commands.writeTimestamp(context.timestampQueryPool, 2, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
}

void CharacterSceneRenderer::drawPrimitive(
    rhi::ICommandRecorder& commands,
    const AssetPrimitive& primitive,
    const std::uint32_t firstIndexOffset,
    const std::uint32_t instanceCount,
    const std::uint32_t firstInstance) {
    const AssetMaterial& material = asset_.materials[primitive.materialIndex];
    if (material.showcasePlatform > 0.5F
        && !renderSettings_->characterPresentation.platformEnabled) {
        return;
    }
    const bool blend = material.alphaMode == AssetAlphaMode::Blend;
    const VkPipeline pipeline = blend
        ? (material.doubleSided ? blendDoubleSidedPipeline_ : blendPipeline_)
        : (material.doubleSided ? opaqueDoubleSidedPipeline_ : opaquePipeline_);
    commands.bindPipeline(pipeline);
    if (!bindlessTextures_) {
        const std::size_t descriptorIndex =
            currentFrame_ * asset_.materials.size() + primitive.materialIndex;
        commands.bindDescriptorSet(
            pipelineLayout_, descriptorSets_[descriptorIndex]);
        if (submissionCounters_ != nullptr) {
            ++submissionCounters_->descriptorSetBinds;
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
        selectedPrimitiveIndex_
                == static_cast<std::int32_t>(
                       &primitive - asset_.primitives.data())
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
    morphConstants.weights = renderSettings_->morphWeights;
    if (bindlessTextures_) {
        morphConstants.textureBaseIndex =
            kSharedTextureSlots
            + primitive.materialIndex * kMaterialTextureSlots;
    }
    morphConstants.gizmoTransform =
        [&]() -> std::array<float, 16> {
            if (selectedPrimitiveIndex_
                != static_cast<std::int32_t>(
                       &primitive - asset_.primitives.data())) {
                return {1.0F, 0.0F, 0.0F, 0.0F,
                        0.0F, 1.0F, 0.0F, 0.0F,
                        0.0F, 0.0F, 1.0F, 0.0F,
                        0.0F, 0.0F, 0.0F, 1.0F};
            }
            constexpr float kPi = 3.14159265358979323846F;
            std::array<float, 3> gizmoTranslation{0.0F, 0.0F, 0.0F};
            std::array<float, 3> gizmoRotation{0.0F, 0.0F, 0.0F};
            std::array<float, 3> gizmoScale{1.0F, 1.0F, 1.0F};
            if (gizmoActive_) {
                gizmoTranslation = gizmoTranslation_;
                gizmoRotation = gizmoRotation_;
                gizmoScale = gizmoScale_;
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
    commands.drawIndexed(
        primitive.indexCount, firstIndexOffset, instanceCount, firstInstance);
    if (submissionCounters_ != nullptr) {
        ++submissionCounters_->drawCalls;
        ++submissionCounters_->pipelineBinds;
        submissionCounters_->pushConstantUpdates += 2;
    }
}

void CharacterSceneRenderer::buildSceneState() {
    state_.asset = &asset_;
    state_.modelMatrix = currentModel_.data();
    state_.selectedPrimitiveIndex = selectedPrimitiveIndex_;
    state_.primitiveCount = asset_.primitives.size();
}

}  // namespace azurerender
