#include "scenes/CharacterSceneRenderer.hpp"

#include "app/AzureRenderInternal.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "render/RenderSettings.hpp"
#include "render/VulkanHelpers.hpp"

#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
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

// IEEE 754 half-precision encode for the HDR environment texture.
std::uint16_t floatToHalf(const float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const std::uint32_t sign = (bits >> 16) & 0x8000U;
    const std::int32_t exponent =
        static_cast<std::int32_t>((bits >> 23) & 0xFFU) - 127 + 15;
    const std::uint32_t mantissa = bits & 0x7FFFFFU;
    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<std::uint16_t>(sign);
        }
        const std::uint32_t shifted =
            (mantissa | 0x800000U) >> (1 - exponent);
        return static_cast<std::uint16_t>(
            sign | (shifted + 0x0FFFU + ((shifted >> 13) & 1U)) >> 13);
    }
    if (exponent >= 31) {
        return static_cast<std::uint16_t>(sign | 0x7C00U);
    }
    return static_cast<std::uint16_t>(
        sign | (static_cast<std::uint32_t>(exponent) << 10)
            | (mantissa >> 13));
}

// Decodes an equirectangular environment asset into RGBA float16 pixels.
std::vector<std::uint16_t> loadEnvironmentAsset(
    const std::string& path,
    std::uint32_t& outWidth,
    std::uint32_t& outHeight) {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint16_t> halfPixels;
    if (path.size() > 4
        && (path.compare(path.size() - 4, 4, ".hdr") == 0
            || path.compare(path.size() - 4, 4, ".HDR") == 0)) {
        float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
        if (data == nullptr) {
            throw std::runtime_error(
                "Failed to decode HDR environment: " + path);
        }
        halfPixels.resize(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        const std::size_t count = static_cast<std::size_t>(width) * height * 4;
        for (std::size_t index = 0; index < count; ++index) {
            halfPixels[index] = floatToHalf(std::clamp(data[index], 0.0F, 64.0F));
        }
        stbi_image_free(data);
    } else {
        unsigned char* data =
            stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (data == nullptr) {
            throw std::runtime_error(
                "Failed to decode environment image: " + path);
        }
        halfPixels.resize(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        const std::size_t count = static_cast<std::size_t>(width) * height * 4;
        for (std::size_t index = 0; index < count; ++index) {
            halfPixels[index] = floatToHalf(
                static_cast<float>(data[index]) * (1.0F / 255.0F));
        }
        stbi_image_free(data);
    }
    outWidth = static_cast<std::uint32_t>(width);
    outHeight = static_cast<std::uint32_t>(height);
    return halfPixels;
}

void appendShowcasePlatform(LoadedAsset& asset) {
    constexpr std::uint32_t kSegments = 96;
    const Vector3 boundsCenter = {
        (asset.boundsMin[0] + asset.boundsMax[0]) * 0.5F,
        (asset.boundsMin[1] + asset.boundsMax[1]) * 0.5F,
        (asset.boundsMin[2] + asset.boundsMax[2]) * 0.5F,
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
    device_ = context.device;
    physicalDevice_ = context.physicalDevice;
    graphicsQueue_ = context.graphicsQueue;
    commandPool_ = context.commandPool;
    renderSettings_ = context.renderSettings;
    rampAtlasPath_ = context.rampAtlasPath;
    environmentPath_ = context.environmentPath;
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
    appendShowcasePlatform(asset_);
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
    buildSceneState();
}

void CharacterSceneRenderer::recordScene(const RenderContext& context) {
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
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    vk::createBuffer(
        device_,
        physicalDevice_,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);
    void* mapped = nullptr;
    vkCheck(
        vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped),
        "vkMapMemory(vertex)");
    std::memcpy(mapped, asset_.vertices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, stagingMemory);
    vk::createBuffer(
        device_,
        physicalDevice_,
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer_,
        vertexBufferMemory_);
    vk::copyBuffer(
        device_, graphicsQueue_, commandPool_, stagingBuffer, vertexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void CharacterSceneRenderer::createIndexBuffer() {
    const VkDeviceSize size = sizeof(std::uint32_t) * asset_.indices.size();
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    vk::createBuffer(
        device_,
        physicalDevice_,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);
    void* mapped = nullptr;
    vkCheck(
        vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped),
        "vkMapMemory(index)");
    std::memcpy(mapped, asset_.indices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, stagingMemory);
    vk::createBuffer(
        device_,
        physicalDevice_,
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer_,
        indexBufferMemory_);
    vk::copyBuffer(
        device_, graphicsQueue_, commandPool_, stagingBuffer, indexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
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
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        vk::createBuffer(
            device_,
            physicalDevice_,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory);
        void* mapped = nullptr;
        vkCheck(
            vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped),
            "vkMapMemory(texture)");
        std::memcpy(mapped, pixels.data(), static_cast<std::size_t>(size));
        vkUnmapMemory(device_, stagingMemory);
        vk::createImage(
            device_,
            physicalDevice_,
            width,
            height,
            format,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                | (mipLevels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0),
            texture.image,
            texture.memory,
            mipLevels);
        vk::transitionImageLayout(
            device_, graphicsQueue_, commandPool_,
            texture.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels);
        vk::copyBufferToImage(
            device_, graphicsQueue_, commandPool_,
            stagingBuffer, texture.image, width, height);
        if (mipLevels > 1) {
            vk::generateMipmaps(
                device_, physicalDevice_, graphicsQueue_, commandPool_,
                texture.image, format, width, height, mipLevels);
        } else {
            vk::transitionImageLayout(
                device_, graphicsQueue_, commandPool_,
                texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                mipLevels);
        }
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        texture.view = vk::createImageView(
            device_, texture.image, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = clampVertical
            ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
            : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod = static_cast<float>(mipLevels - 1);
        vkCheck(
            vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler),
            "vkCreateSampler");
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
    if (!environmentPath_.empty()) {
        environmentPixels = loadEnvironmentAsset(
            environmentPath_,
            environmentWidth,
            environmentHeight);
        azurerender::RuntimeDiagnostics::instance().print(
            "asset",
            "Environment: " + environmentPath_ + " ("
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
                        floatToHalf(std::clamp(color, 0.0F, 32.0F));
                }
                environmentPixels[pixel + 3] = floatToHalf(1.0F);
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
    uniformBufferMemories_.resize(kMaxFramesInFlight);
    uniformBufferMapped_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        vk::createBuffer(
            device_,
            physicalDevice_,
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers_[index],
            uniformBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                uniformBufferMemories_[index],
                0,
                size,
                0,
                &uniformBufferMapped_[index]),
            "vkMapMemory(uniform)");
    }
}

void CharacterSceneRenderer::createJointBuffers() {
    if (asset_.jointMatrices.empty()) {
        throw std::runtime_error("Asset has no joint-matrix fallback");
    }
    const VkDeviceSize size =
        sizeof(asset_.jointMatrices.front()) * asset_.jointMatrices.size();
    jointBuffers_.resize(kMaxFramesInFlight);
    jointBufferMemories_.resize(kMaxFramesInFlight);
    jointBufferMapped_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        vk::createBuffer(
            device_,
            physicalDevice_,
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            jointBuffers_[index],
            jointBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                jointBufferMemories_[index],
                0,
                size,
                0,
                &jointBufferMapped_[index]),
            "vkMapMemory(joints)");
        std::memcpy(
            jointBufferMapped_[index],
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
    oitIndexBufferMemories_.resize(kMaxFramesInFlight);
    oitIndexBufferMapped_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        vk::createBuffer(
            device_,
            physicalDevice_,
            oitIndexBufferSize_,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            oitIndexBuffers_[index],
            oitIndexBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                oitIndexBufferMemories_[index],
                0,
                oitIndexBufferSize_,
                0,
                &oitIndexBufferMapped_[index]),
            "vkMapMemory(oit)");
    }
}

void CharacterSceneRenderer::createDescriptorPool() {
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight * asset_.materials.size());
    const std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * 11},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
    }};
    VkDescriptorPoolCreateInfo createInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = descriptorCount;
    vkCheck(
        vkCreateDescriptorPool(device_, &createInfo, nullptr, &descriptorPool_),
        "vkCreateDescriptorPool");
}

void CharacterSceneRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.binding = 0;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 1;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalBinding = textureBinding;
    normalBinding.binding = 2;
    VkDescriptorSetLayoutBinding metallicRoughnessBinding = textureBinding;
    metallicRoughnessBinding.binding = 3;
    VkDescriptorSetLayoutBinding environmentBinding = textureBinding;
    environmentBinding.binding = 4;
    VkDescriptorSetLayoutBinding specularEmissiveBinding = textureBinding;
    specularEmissiveBinding.binding = 5;
    VkDescriptorSetLayoutBinding styleMaskBinding = textureBinding;
    styleMaskBinding.binding = 6;
    VkDescriptorSetLayoutBinding matcapBinding = textureBinding;
    matcapBinding.binding = 7;
    VkDescriptorSetLayoutBinding hairDataBinding = textureBinding;
    hairDataBinding.binding = 8;
    VkDescriptorSetLayoutBinding shadowBinding = textureBinding;
    shadowBinding.binding = 9;
    VkDescriptorSetLayoutBinding jointBinding{};
    jointBinding.binding = 10;
    jointBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    jointBinding.descriptorCount = 1;
    jointBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutBinding toonRampBinding = textureBinding;
    toonRampBinding.binding = 11;
    VkDescriptorSetLayoutBinding faceSdfBinding = textureBinding;
    faceSdfBinding.binding = 12;

    const std::array bindings = {
        uniformBinding,
        textureBinding,
        normalBinding,
        metallicRoughnessBinding,
        environmentBinding,
        specularEmissiveBinding,
        styleMaskBinding,
        matcapBinding,
        hairDataBinding,
        shadowBinding,
        jointBinding,
        toonRampBinding,
        faceSdfBinding,
    };
    VkDescriptorSetLayoutCreateInfo createInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    vkCheck(
        vkCreateDescriptorSetLayout(
            device_, &createInfo, nullptr, &descriptorSetLayout_),
        "vkCreateDescriptorSetLayout");
}

void CharacterSceneRenderer::createDescriptorSets() {
    const std::size_t descriptorCount = kMaxFramesInFlight * asset_.materials.size();
    const std::vector<VkDescriptorSetLayout> layouts(
        descriptorCount, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = descriptorPool_;
    allocateInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    allocateInfo.pSetLayouts = layouts.data();
    descriptorSets_.resize(descriptorCount);
    vkCheck(
        vkAllocateDescriptorSets(device_, &allocateInfo, descriptorSets_.data()),
        "vkAllocateDescriptorSets");

    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[frame];
        bufferInfo.range = sizeof(UniformBufferObject);
        VkDescriptorBufferInfo jointBufferInfo{};
        jointBufferInfo.buffer = jointBuffers_[frame];
        jointBufferInfo.range =
            sizeof(asset_.jointMatrices.front())
            * asset_.jointMatrices.size();

        for (std::size_t material = 0; material < asset_.materials.size();
             ++material) {
            const std::size_t descriptorIndex =
                frame * asset_.materials.size() + material;
            VkDescriptorImageInfo baseColorInfo{};
            baseColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            baseColorInfo.imageView = gpuMaterials_[material].baseColor.view;
            baseColorInfo.sampler = gpuMaterials_[material].baseColor.sampler;
            VkDescriptorImageInfo normalInfo{};
            normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalInfo.imageView = gpuMaterials_[material].normal.view;
            normalInfo.sampler = gpuMaterials_[material].normal.sampler;
            VkDescriptorImageInfo metallicRoughnessInfo{};
            metallicRoughnessInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            metallicRoughnessInfo.imageView =
                gpuMaterials_[material].metallicRoughness.view;
            metallicRoughnessInfo.sampler =
                gpuMaterials_[material].metallicRoughness.sampler;
            VkDescriptorImageInfo environmentInfo{};
            environmentInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            environmentInfo.imageView = environmentTexture_.view;
            environmentInfo.sampler = environmentTexture_.sampler;
            VkDescriptorImageInfo specularEmissiveInfo{};
            specularEmissiveInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            specularEmissiveInfo.imageView =
                gpuMaterials_[material].specularEmissive.view;
            specularEmissiveInfo.sampler =
                gpuMaterials_[material].specularEmissive.sampler;
            VkDescriptorImageInfo styleMaskInfo{};
            styleMaskInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            styleMaskInfo.imageView =
                gpuMaterials_[material].styleMask.view;
            styleMaskInfo.sampler =
                gpuMaterials_[material].styleMask.sampler;
            VkDescriptorImageInfo matcapInfo{};
            matcapInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            matcapInfo.imageView =
                gpuMaterials_[material].matcap.view;
            matcapInfo.sampler =
                gpuMaterials_[material].matcap.sampler;
            VkDescriptorImageInfo hairDataInfo{};
            hairDataInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            hairDataInfo.imageView =
                gpuMaterials_[material].hairData.view;
            hairDataInfo.sampler =
                gpuMaterials_[material].hairData.sampler;
            VkDescriptorImageInfo shadowInfo{};
            shadowInfo.imageLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowInfo.imageView = shadowImageView_;
            shadowInfo.sampler = shadowSampler_;
            VkDescriptorImageInfo toonRampInfo{};
            toonRampInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toonRampInfo.imageView = toonRampTexture_.view;
            toonRampInfo.sampler = toonRampTexture_.sampler;
            VkDescriptorImageInfo faceSdfInfo{};
            faceSdfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            faceSdfInfo.imageView = gpuMaterials_[material].faceSdf.view;
            faceSdfInfo.sampler = gpuMaterials_[material].faceSdf.sampler;

            std::array<VkWriteDescriptorSet, 13> writes{};
            writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[0].dstSet = descriptorSets_[descriptorIndex];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &bufferInfo;
            writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[1].dstSet = descriptorSets_[descriptorIndex];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo = &baseColorInfo;
            writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[2].dstSet = descriptorSets_[descriptorIndex];
            writes[2].dstBinding = 2;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].pImageInfo = &normalInfo;
            writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[3].dstSet = descriptorSets_[descriptorIndex];
            writes[3].dstBinding = 3;
            writes[3].descriptorCount = 1;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].pImageInfo = &metallicRoughnessInfo;
            writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[4].dstSet = descriptorSets_[descriptorIndex];
            writes[4].dstBinding = 4;
            writes[4].descriptorCount = 1;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[4].pImageInfo = &environmentInfo;
            writes[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[5].dstSet = descriptorSets_[descriptorIndex];
            writes[5].dstBinding = 5;
            writes[5].descriptorCount = 1;
            writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[5].pImageInfo = &specularEmissiveInfo;
            writes[6] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[6].dstSet = descriptorSets_[descriptorIndex];
            writes[6].dstBinding = 6;
            writes[6].descriptorCount = 1;
            writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[6].pImageInfo = &styleMaskInfo;
            writes[7] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[7].dstSet = descriptorSets_[descriptorIndex];
            writes[7].dstBinding = 7;
            writes[7].descriptorCount = 1;
            writes[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[7].pImageInfo = &matcapInfo;
            writes[8] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[8].dstSet = descriptorSets_[descriptorIndex];
            writes[8].dstBinding = 8;
            writes[8].descriptorCount = 1;
            writes[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[8].pImageInfo = &hairDataInfo;
            writes[9] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[9].dstSet = descriptorSets_[descriptorIndex];
            writes[9].dstBinding = 9;
            writes[9].descriptorCount = 1;
            writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[9].pImageInfo = &shadowInfo;
            writes[10] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[10].dstSet = descriptorSets_[descriptorIndex];
            writes[10].dstBinding = 10;
            writes[10].descriptorCount = 1;
            writes[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[10].pBufferInfo = &jointBufferInfo;
            writes[11] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[11].dstSet = descriptorSets_[descriptorIndex];
            writes[11].dstBinding = 11;
            writes[11].descriptorCount = 1;
            writes[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[11].pImageInfo = &toonRampInfo;
            writes[12] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[12].dstSet = descriptorSets_[descriptorIndex];
            writes[12].dstBinding = 12;
            writes[12].descriptorCount = 1;
            writes[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[12].pImageInfo = &faceSdfInfo;
            vkUpdateDescriptorSets(
                device_,
                static_cast<std::uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr);
        }
    }
}

void CharacterSceneRenderer::createGraphicsPipeline(
    const RenderContext& context) {
    const std::string shaderDirectory = context.shaderDirectory;
    const auto vertexCode =
        vk::readBinaryFile(shaderDirectory + "/mesh.vert.spv");
    const auto fragmentCode =
        vk::readBinaryFile(shaderDirectory + "/mesh.frag.spv");
    const auto outlineVertexCode =
        vk::readBinaryFile(shaderDirectory + "/outline.vert.spv");
    const auto outlineFragmentCode =
        vk::readBinaryFile(shaderDirectory + "/outline.frag.spv");
    const auto backgroundVertexCode =
        vk::readBinaryFile(shaderDirectory + "/background.vert.spv");
    const auto backgroundFragmentCode =
        vk::readBinaryFile(shaderDirectory + "/background.frag.spv");
    const auto shadowVertexCode =
        vk::readBinaryFile(shaderDirectory + "/shadow.vert.spv");
    const auto shadowFragmentCode =
        vk::readBinaryFile(shaderDirectory + "/shadow.frag.spv");
    const VkShaderModule vertexModule = vk::createShaderModule(device_, vertexCode);
    const VkShaderModule fragmentModule = vk::createShaderModule(device_, fragmentCode);
    const VkShaderModule outlineVertexModule = vk::createShaderModule(device_, outlineVertexCode);
    const VkShaderModule outlineFragmentModule = vk::createShaderModule(device_, outlineFragmentCode);
    const VkShaderModule backgroundVertexModule = vk::createShaderModule(device_, backgroundVertexCode);
    const VkShaderModule backgroundFragmentModule = vk::createShaderModule(device_, backgroundFragmentCode);
    const VkShaderModule shadowVertexModule = vk::createShaderModule(device_, shadowVertexCode);
    const VkShaderModule shadowFragmentModule = vk::createShaderModule(device_, shadowFragmentCode);

    try {
        VkPipelineShaderStageCreateInfo vertexStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexModule;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentModule;
        fragmentStage.pName = "main";
        const std::array shaderStages = {vertexStage, fragmentStage};
        VkPipelineShaderStageCreateInfo outlineVertexStage = vertexStage;
        outlineVertexStage.module = outlineVertexModule;
        VkPipelineShaderStageCreateInfo outlineFragmentStage = fragmentStage;
        outlineFragmentStage.module = outlineFragmentModule;
        const std::array outlineShaderStages = {
            outlineVertexStage,
            outlineFragmentStage,
        };
        VkPipelineShaderStageCreateInfo backgroundVertexStage = vertexStage;
        backgroundVertexStage.module = backgroundVertexModule;
        VkPipelineShaderStageCreateInfo backgroundFragmentStage = fragmentStage;
        backgroundFragmentStage.module = backgroundFragmentModule;
        const std::array backgroundShaderStages = {
            backgroundVertexStage,
            backgroundFragmentStage,
        };
        VkPipelineShaderStageCreateInfo shadowVertexStage = vertexStage;
        shadowVertexStage.module = shadowVertexModule;
        VkPipelineShaderStageCreateInfo shadowFragmentStage = fragmentStage;
        shadowFragmentStage.module = shadowFragmentModule;
        const std::array shadowShaderStages = {
            shadowVertexStage,
            shadowFragmentStage,
        };

        VkPipelineVertexInputStateCreateInfo vertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(AssetVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        std::array<VkVertexInputAttributeDescription, 8> attributeDescriptions{};
        attributeDescriptions[0] = {
            0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AssetVertex, position)};
        attributeDescriptions[1] = {
            1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AssetVertex, normal)};
        attributeDescriptions[2] = {
            2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(AssetVertex, tangent)};
        attributeDescriptions[3] = {
            3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(AssetVertex, texcoord)};
        attributeDescriptions[4] = {
            4, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(AssetVertex, joints)};
        attributeDescriptions[5] = {
            5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(AssetVertex, weights)};
        attributeDescriptions[6] = {
            6, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AssetVertex, morph0)};
        attributeDescriptions[7] = {
            7, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AssetVertex, morph1)};
        const std::array shadowAttributeDescriptions = {
            attributeDescriptions[0],
            attributeDescriptions[3],
            attributeDescriptions[4],
            attributeDescriptions[5],
        };
        const std::array outlineAttributeDescriptions = {
            attributeDescriptions[0],
            attributeDescriptions[1],
            attributeDescriptions[4],
            attributeDescriptions[5],
        };
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(attributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewportState{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterizer{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth = 1.0F;
        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthStencil{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        std::array<VkPipelineColorBlendAttachmentState, 2>
            colorBlendAttachments{};
        for (auto& attachment : colorBlendAttachments) {
            attachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount =
            static_cast<std::uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();
        const std::array dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount =
            static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo layoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorSetLayout_;
        VkPushConstantRange materialPushRange{};
        materialPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        materialPushRange.size = sizeof(MaterialPushConstants);
        VkPushConstantRange morphPushRange{};
        morphPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        morphPushRange.offset = sizeof(MaterialPushConstants);
        morphPushRange.size = sizeof(MorphPushConstants);
        const VkPushConstantRange pushConstantRanges[] = {
            materialPushRange, morphPushRange};
        layoutInfo.pushConstantRangeCount = 2;
        layoutInfo.pPushConstantRanges = pushConstantRanges;
        vkCheck(
            vkCreatePipelineLayout(
                device_, &layoutInfo, nullptr, &pipelineLayout_),
            "vkCreatePipelineLayout");

        VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount =
            static_cast<std::uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = context.sceneRenderPass;
        pipelineInfo.subpass = 0;

        const auto createVariant = [&](
            const VkCullModeFlags cullMode,
            const bool blend,
            VkPipeline& pipeline) {
            rasterizer.cullMode = cullMode;
            depthStencil.depthWriteEnable = blend ? VK_FALSE : VK_TRUE;
            auto& colorAttachment = colorBlendAttachments[0];
            colorAttachment.blendEnable = blend ? VK_TRUE : VK_FALSE;
            colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorAttachment.dstColorBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorAttachment.dstAlphaBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachments[1] = colorAttachment;
            vkCheck(
                vkCreateGraphicsPipelines(
                    device_,
                    VK_NULL_HANDLE,
                    1,
                    &pipelineInfo,
                    nullptr,
                    &pipeline),
                "vkCreateGraphicsPipelines(material variant)");
        };
        createVariant(VK_CULL_MODE_BACK_BIT, false, opaquePipeline_);
        createVariant(VK_CULL_MODE_NONE, false, opaqueDoubleSidedPipeline_);
        createVariant(VK_CULL_MODE_BACK_BIT, true, blendPipeline_);
        createVariant(VK_CULL_MODE_NONE, true, blendDoubleSidedPipeline_);
        pipelineInfo.pStages = outlineShaderStages.data();
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(
                outlineAttributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions =
            outlineAttributeDescriptions.data();
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        depthStencil.depthWriteEnable = VK_FALSE;
        colorBlendAttachments[0].blendEnable = VK_FALSE;
        colorBlendAttachments[1].blendEnable = VK_FALSE;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &outlinePipeline_),
            "vkCreateGraphicsPipelines(outline)");

        VkPipelineVertexInputStateCreateInfo emptyVertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        pipelineInfo.pStages = backgroundShaderStages.data();
        pipelineInfo.pVertexInputState = &emptyVertexInput;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &backgroundPipeline_),
            "vkCreateGraphicsPipelines(background)");

        pipelineInfo.pStages = shadowShaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(shadowAttributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions =
            shadowAttributeDescriptions.data();
        pipelineInfo.renderPass = context.shadowRenderPass;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 1.25F;
        rasterizer.depthBiasSlopeFactor = 1.75F;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &shadowPipeline_),
            "vkCreateGraphicsPipelines(shadow)");
    } catch (...) {
        vkDestroyShaderModule(device_, shadowFragmentModule, nullptr);
        vkDestroyShaderModule(device_, shadowVertexModule, nullptr);
        vkDestroyShaderModule(device_, backgroundFragmentModule, nullptr);
        vkDestroyShaderModule(device_, backgroundVertexModule, nullptr);
        vkDestroyShaderModule(device_, outlineFragmentModule, nullptr);
        vkDestroyShaderModule(device_, outlineVertexModule, nullptr);
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, shadowFragmentModule, nullptr);
    vkDestroyShaderModule(device_, shadowVertexModule, nullptr);
    vkDestroyShaderModule(device_, backgroundFragmentModule, nullptr);
    vkDestroyShaderModule(device_, backgroundVertexModule, nullptr);
    vkDestroyShaderModule(device_, outlineFragmentModule, nullptr);
    vkDestroyShaderModule(device_, outlineVertexModule, nullptr);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
}

void CharacterSceneRenderer::destroyResources() {
    if (device_ == VK_NULL_HANDLE) {
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
            vkDestroyPipeline(device_, *pipeline, nullptr);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    for (std::size_t index = 0; index < uniformBuffers_.size(); ++index) {
        if (uniformBufferMapped_[index] != nullptr) {
            vkUnmapMemory(device_, uniformBufferMemories_[index]);
        }
        vkDestroyBuffer(device_, uniformBuffers_[index], nullptr);
        vkFreeMemory(device_, uniformBufferMemories_[index], nullptr);
    }
    uniformBuffers_.clear();
    uniformBufferMemories_.clear();
    uniformBufferMapped_.clear();
    for (std::size_t index = 0; index < jointBuffers_.size(); ++index) {
        if (jointBufferMapped_[index] != nullptr) {
            vkUnmapMemory(device_, jointBufferMemories_[index]);
        }
        vkDestroyBuffer(device_, jointBuffers_[index], nullptr);
        vkFreeMemory(device_, jointBufferMemories_[index], nullptr);
    }
    jointBuffers_.clear();
    jointBufferMemories_.clear();
    jointBufferMapped_.clear();
    for (std::size_t index = 0; index < oitIndexBuffers_.size(); ++index) {
        if (oitIndexBufferMapped_[index] != nullptr) {
            vkUnmapMemory(device_, oitIndexBufferMemories_[index]);
        }
        vkDestroyBuffer(device_, oitIndexBuffers_[index], nullptr);
        vkFreeMemory(device_, oitIndexBufferMemories_[index], nullptr);
    }
    oitIndexBuffers_.clear();
    oitIndexBufferMemories_.clear();
    oitIndexBufferMapped_.clear();
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, indexBuffer_, nullptr);
        vkFreeMemory(device_, indexBufferMemory_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    for (const auto& material : gpuMaterials_) {
        for (const GpuTexture* texture : {
                 &material.baseColor,
                 &material.normal,
                 &material.metallicRoughness,
                 &material.specularEmissive,
                 &material.styleMask,
                 &material.matcap,
                 &material.hairData,
                 &material.faceSdf}) {
            vkDestroySampler(device_, texture->sampler, nullptr);
            vkDestroyImageView(device_, texture->view, nullptr);
            vkDestroyImage(device_, texture->image, nullptr);
            vkFreeMemory(device_, texture->memory, nullptr);
        }
    }
    gpuMaterials_.clear();
    vkDestroySampler(device_, environmentTexture_.sampler, nullptr);
    vkDestroyImageView(device_, environmentTexture_.view, nullptr);
    vkDestroyImage(device_, environmentTexture_.image, nullptr);
    vkFreeMemory(device_, environmentTexture_.memory, nullptr);
    vkDestroySampler(device_, toonRampTexture_.sampler, nullptr);
    vkDestroyImageView(device_, toonRampTexture_.view, nullptr);
    vkDestroyImage(device_, toonRampTexture_.image, nullptr);
    vkFreeMemory(device_, toonRampTexture_.memory, nullptr);
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
            vkDestroyPipeline(device_, *pipeline, nullptr);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
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
            jointBufferMapped_[currentFrame_],
            asset_.jointMatrices.data(),
            jointBytes);
    }

    const Vector3 center = {
        (asset_.boundsMin[0] + asset_.boundsMax[0]) * 0.5F,
        (asset_.boundsMin[1] + asset_.boundsMax[1]) * 0.5F,
        (asset_.boundsMin[2] + asset_.boundsMax[2]) * 0.5F,
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
    const float aspect =
        static_cast<float>(frame.swapchainWidth)
        / static_cast<float>(std::max(frame.swapchainHeight, 1U));
    constexpr float kPi = 3.14159265358979323846F;
    const Matrix4 projection = perspective(kPi / 3.0F, aspect, 0.1F, 100.0F);
    const RenderSettings& settings = *renderSettings_;
    const Vector3 lightDirection = settings.showcasePreset == 1
        ? normalize({0.36F, 0.86F, 0.36F})
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

    UniformBufferObject uniform{};
    uniform.model = model;
    uniform.modelViewProjection = multiply(projection, multiply(view, model));
    uniform.lightModelViewProjection =
        multiply(lightProjection, multiply(lightView, model));
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
        0.12F,
    };
    constexpr std::array<std::array<float, 4>, 5> kShowcasePresets = {{
        {0.0F, 1.00F, 0.13F, 0.12F},
        {1.0F, 0.98F, 0.12F, 0.28F},
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
        const Vector3 headX = normalize({head[0], head[1], head[2]});
        const Vector3 headY = normalize({head[4], head[5], head[6]});
        const Vector3 headZ = normalize({head[8], head[9], head[10]});
        faceLight = normalize({
            dot(objectLight, headX),
            dot(objectLight, headY),
            dot(objectLight, headZ),
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
        uniformBufferMapped_[currentFrame_],
        &uniform,
        sizeof(uniform));
}

void CharacterSceneRenderer::recordShadowPass(const RenderContext& context) {
    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0F, 0};
    VkRenderPassBeginInfo shadowPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    shadowPassInfo.renderPass = context.shadowRenderPass;
    shadowPassInfo.framebuffer = context.shadowFramebuffer;
    shadowPassInfo.renderArea.extent = {context.shadowMapSize, context.shadowMapSize};
    shadowPassInfo.clearValueCount = 1;
    shadowPassInfo.pClearValues = &shadowClear;
    vkCmdBeginRenderPass(
        context.commandBuffer,
        &shadowPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    VkViewport shadowViewport{};
    shadowViewport.width = static_cast<float>(context.shadowMapSize);
    shadowViewport.height = static_cast<float>(context.shadowMapSize);
    shadowViewport.maxDepth = 1.0F;
    vkCmdSetViewport(context.commandBuffer, 0, 1, &shadowViewport);
    VkRect2D shadowScissor{};
    shadowScissor.extent = {context.shadowMapSize, context.shadowMapSize};
    vkCmdSetScissor(context.commandBuffer, 0, 1, &shadowScissor);
    const VkDeviceSize shadowOffsets[] = {0};
    vkCmdBindVertexBuffers(
        context.commandBuffer, 0, 1, &vertexBuffer_, shadowOffsets);
    vkCmdBindIndexBuffer(
        context.commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shadowPipeline_);
    for (const AssetPrimitive& primitive : asset_.primitives) {
        const AssetMaterial& material =
            asset_.materials[primitive.materialIndex];
        if (material.showcasePlatform > 0.5F
            || material.materialClass == AssetMaterialClass::Overlay) {
            continue;
        }
        const std::size_t descriptorIndex =
            context.currentFrame * asset_.materials.size()
            + primitive.materialIndex;
        vkCmdBindDescriptorSets(
            context.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[descriptorIndex],
            0,
            nullptr);
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
        vkCmdPushConstants(
            context.commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(materialConstants),
            &materialConstants);
        const MorphPushConstants morphConstants{
            renderSettings_->morphWeights,
        };
        vkCmdPushConstants(
            context.commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(MaterialPushConstants),
            sizeof(morphConstants),
            &morphConstants);
        vkCmdDrawIndexed(
            context.commandBuffer,
            primitive.indexCount,
            1,
            primitive.firstIndex,
            0,
            0);
    }
    vkCmdEndRenderPass(context.commandBuffer);
    if (context.gpuTimingEnabled) {
        vkCmdWriteTimestamp(
            context.commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            context.timestampQueryPool,
            1);
    }
}

void CharacterSceneRenderer::recordMainPass(const RenderContext& context) {
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
    VkRenderPassBeginInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = context.sceneRenderPass;
    renderPassInfo.framebuffer = context.sceneFramebuffer;
    renderPassInfo.renderArea.extent = context.renderExtent;
    renderPassInfo.clearValueCount =
        static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(
        context.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(context.renderExtent.width);
    viewport.height = static_cast<float>(context.renderExtent.height);
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = context.renderExtent;
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        backgroundPipeline_);
    const std::size_t backgroundDescriptorIndex =
        context.currentFrame * asset_.materials.size();
    vkCmdBindDescriptorSets(
        context.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[backgroundDescriptorIndex],
        0,
        nullptr);
    vkCmdDraw(context.commandBuffer, 3, 1, 0, 0);

    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(context.commandBuffer, 0, 1, &vertexBuffer_, offsets);
    vkCmdBindIndexBuffer(context.commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    const RenderSettings& settings = *renderSettings_;
    if (settings.silhouetteOutlineEnabled) {
        vkCmdBindPipeline(
            context.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            outlinePipeline_);
        for (const AssetPrimitive& primitive : asset_.primitives) {
            if (asset_.materials[primitive.materialIndex].alphaMode
                == AssetAlphaMode::Blend) {
                continue;
            }
            const std::size_t descriptorIndex =
                context.currentFrame * asset_.materials.size()
                + primitive.materialIndex;
            vkCmdBindDescriptorSets(
                context.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout_,
                0,
                1,
                &descriptorSets_[descriptorIndex],
                0,
                nullptr);
            vkCmdDrawIndexed(
                context.commandBuffer,
                primitive.indexCount,
                1,
                primitive.firstIndex,
                0,
                0);
        }
    }
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode
            != AssetAlphaMode::Blend) {
            drawPrimitive(context.commandBuffer, primitive, primitive.firstIndex);
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
    std::size_t oitWriteIndex = 0;
    if (!transparentPrimitives.empty() && !oitIndexBufferMapped_.empty()) {
        std::uint32_t* oitMapped = static_cast<std::uint32_t*>(
            oitIndexBufferMapped_[context.currentFrame]);
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
                    currentModel_, asset_.vertices[i0].position);
                const Vector3 v1 = transformPosition(
                    currentModel_, asset_.vertices[i1].position);
                const Vector3 v2 = transformPosition(
                    currentModel_, asset_.vertices[i2].position);
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
                    asset_.indices[base] - primitive->firstIndex;
                oitMapped[oitWriteIndex + triangle * 3 + 1] =
                    asset_.indices[base + 1] - primitive->firstIndex;
                oitMapped[oitWriteIndex + triangle * 3 + 2] =
                    asset_.indices[base + 2] - primitive->firstIndex;
            }
            oitWriteIndex += primitive->indexCount;
        }
    }
    std::size_t oitReadIndex = 0;
    for (const AssetPrimitive* primitive : transparentPrimitives) {
        if (!oitIndexBufferMapped_.empty()) {
            const VkDeviceSize offsetBytes =
                static_cast<VkDeviceSize>(oitReadIndex)
                * sizeof(std::uint32_t);
            vkCmdBindIndexBuffer(
                context.commandBuffer,
                oitIndexBuffers_[context.currentFrame],
                offsetBytes,
                VK_INDEX_TYPE_UINT32);
        }
        drawPrimitive(context.commandBuffer, *primitive, 0);
        oitReadIndex += primitive->indexCount;
    }
    vkCmdEndRenderPass(context.commandBuffer);
    if (context.gpuTimingEnabled) {
        vkCmdWriteTimestamp(
            context.commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            context.timestampQueryPool,
            2);
    }
}

void CharacterSceneRenderer::drawPrimitive(
    const VkCommandBuffer commandBuffer,
    const AssetPrimitive& primitive,
    const std::uint32_t firstIndexOffset) {
    const AssetMaterial& material = asset_.materials[primitive.materialIndex];
    const bool blend = material.alphaMode == AssetAlphaMode::Blend;
    const VkPipeline pipeline = blend
        ? (material.doubleSided ? blendDoubleSidedPipeline_ : blendPipeline_)
        : (material.doubleSided ? opaqueDoubleSidedPipeline_ : opaquePipeline_);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    const std::size_t descriptorIndex =
        currentFrame_ * asset_.materials.size() + primitive.materialIndex;
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[descriptorIndex],
        0,
        nullptr);
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
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout_,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(materialConstants),
        &materialConstants);
    const MorphPushConstants morphConstants{
        renderSettings_->morphWeights,
        {0.0F, 0.0F},
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
        }(),
    };
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT,
        sizeof(MaterialPushConstants),
        sizeof(morphConstants),
        &morphConstants);
    vkCmdDrawIndexed(
        commandBuffer,
        primitive.indexCount,
        1,
        firstIndexOffset,
        0,
        0);
}

void CharacterSceneRenderer::buildSceneState() {
    state_.asset = &asset_;
    state_.modelMatrix = currentModel_.data();
    state_.selectedPrimitiveIndex = selectedPrimitiveIndex_;
    state_.primitiveCount = asset_.primitives.size();
}

}  // namespace azurerender
