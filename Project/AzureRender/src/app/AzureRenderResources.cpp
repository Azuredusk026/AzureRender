#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// stb_image declarations only; the implementation is emitted by
// GltfLoader.cpp (STB_IMAGE_IMPLEMENTATION), symbols link at program level.
#include <stb_image.h>

using namespace azurerender::internal;

namespace {

// IEEE 754 half-precision encode for the HDR environment texture.
std::uint16_t floatToHalf(const float value) {
    const std::uint32_t bits = *reinterpret_cast<const std::uint32_t*>(&value);
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
// .hdr files use stb_image float decoding (RGBE); LDR images are decoded as
// 8-bit and normalized into the same HDR pipeline.
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
            // LDR 0..255 -> HDR 0..~1; keep some headroom for emissive parts.
            halfPixels[index] = floatToHalf(
                static_cast<float>(data[index]) * (1.0F / 255.0F));
        }
        stbi_image_free(data);
    }
    outWidth = static_cast<std::uint32_t>(width);
    outHeight = static_cast<std::uint32_t>(height);
    return halfPixels;
}

}  // namespace

void AzureRenderApp::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = swapchainImages_[index];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat_;
        createInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.layerCount = 1;

        vkCheck(
            vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[index]),
            "vkCreateImageView");
    }
}

void AzureRenderApp::createEditorViewportResources() {
    if (!editorUiEnabled_) {
        return;
    }
    editorViewportImages_.resize(swapchainImages_.size());
    editorViewportImageMemories_.resize(swapchainImages_.size());
    editorViewportImageViews_.resize(swapchainImages_.size());
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        createImage(
            renderExtent_.width,
            renderExtent_.height,
            swapchainFormat_,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            editorViewportImages_[index],
            editorViewportImageMemories_[index]);
        editorViewportImageViews_[index] = createImageView(
            editorViewportImages_[index],
            swapchainFormat_,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    if (editorViewportSampler_ == VK_NULL_HANDLE) {
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 1.0F;
        vkCheck(
            vkCreateSampler(
                device_, &samplerInfo, nullptr, &editorViewportSampler_),
            "vkCreateSampler(editor viewport)");
    }
}

void AzureRenderApp::createSceneColorResources() {
    sceneColorImages_.resize(swapchainImages_.size());
    sceneColorImageMemories_.resize(swapchainImages_.size());
    sceneColorImageViews_.resize(swapchainImages_.size());

    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        createImage(
            renderExtent_.width,
            renderExtent_.height,
            kHdrSceneColorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            sceneColorImages_[index],
            sceneColorImageMemories_[index]);
        sceneColorImageViews_[index] = createImageView(
            sceneColorImages_[index],
            kHdrSceneColorFormat,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void AzureRenderApp::createDepthResources() {
    depthFormat_ = findDepthFormat();
    depthImages_.resize(swapchainImages_.size());
    depthImageMemories_.resize(swapchainImages_.size());
    depthImageViews_.resize(swapchainImages_.size());

    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        createImage(
            renderExtent_.width,
            renderExtent_.height,
            depthFormat_,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT,
            depthImages_[index],
            depthImageMemories_[index]);
        depthImageViews_[index] = createImageView(
            depthImages_[index], depthFormat_, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void AzureRenderApp::createNormalResources() {
    normalImages_.resize(swapchainImages_.size());
    normalImageMemories_.resize(swapchainImages_.size());
    normalImageViews_.resize(swapchainImages_.size());
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        createImage(
            renderExtent_.width,
            renderExtent_.height,
            normalFormat_,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT,
            normalImages_[index],
            normalImageMemories_[index]);
        normalImageViews_[index] = createImageView(
            normalImages_[index],
            normalFormat_,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void AzureRenderApp::createShadowResources() {
    shadowFormat_ = findDepthFormat();
    createImage(
        kShadowMapSize,
        kShadowMapSize,
        shadowFormat_,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT,
        shadowImage_,
        shadowImageMemory_);
    shadowImageView_ = createImageView(
        shadowImage_,
        shadowFormat_,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxLod = 1.0F;
    vkCheck(
        vkCreateSampler(device_, &samplerInfo, nullptr, &shadowSampler_),
        "vkCreateSampler(shadow)");

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = shadowFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 0;
    depthReference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount =
        static_cast<std::uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();
    vkCheck(
        vkCreateRenderPass(
            device_,
            &renderPassInfo,
            nullptr,
            &shadowRenderPass_),
        "vkCreateRenderPass(shadow)");

    VkFramebufferCreateInfo framebufferInfo{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = shadowRenderPass_;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &shadowImageView_;
    framebufferInfo.width = kShadowMapSize;
    framebufferInfo.height = kShadowMapSize;
    framebufferInfo.layers = 1;
    vkCheck(
        vkCreateFramebuffer(
            device_,
            &framebufferInfo,
            nullptr,
            &shadowFramebuffer_),
        "vkCreateFramebuffer(shadow)");
}



void AzureRenderApp::createVertexBuffer() {
    const VkDeviceSize size = sizeof(AssetVertex) * asset_.vertices.size();

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);

    void* mapped = nullptr;
    vkCheck(vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped), "vkMapMemory(vertex)");
    std::memcpy(mapped, asset_.vertices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, stagingMemory);

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer_,
        vertexBufferMemory_);
    copyBuffer(stagingBuffer, vertexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void AzureRenderApp::createIndexBuffer() {
    const VkDeviceSize size = sizeof(std::uint32_t) * asset_.indices.size();

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);

    void* mapped = nullptr;
    vkCheck(vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped), "vkMapMemory(index)");
    std::memcpy(mapped, asset_.indices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, stagingMemory);

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer_,
        indexBufferMemory_);
    copyBuffer(stagingBuffer, indexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void AzureRenderApp::createTexture() {
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
        createBuffer(
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
        createImage(
            width,
            height,
            format,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                | (mipLevels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0),
            texture.image,
            texture.memory,
            mipLevels);
        transitionImageLayout(
            texture.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, texture.image, width, height);
        if (mipLevels > 1) {
            generateMipmaps(texture.image, format, width, height, mipLevels);
        } else {
            transitionImageLayout(
                texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        texture.view = createImageView(
            texture.image, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
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
    // HDR environment: one float16 RGBA per pixel (values may exceed 1.0).
    std::vector<std::uint16_t> environmentPixels;
    std::uint32_t environmentWidth = kEnvironmentWidth;
    std::uint32_t environmentHeight = kEnvironmentHeight;
    if (!runOptions_.environmentPath.empty()) {
        // Import a real equirectangular environment asset (.hdr uses stb_image
        // float decoding; .png/.jpg are decoded as LDR and scaled to HDR).
        environmentPixels = loadEnvironmentAsset(
            runOptions_.environmentPath,
            environmentWidth,
            environmentHeight);
        azurerender::RuntimeDiagnostics::instance().print(
            "asset",
            "Environment: " + runOptions_.environmentPath + " ("
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
                // HDR sky values: bright sun disc well above 1.0, cool ambient.
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

    const auto toonRamp = loadPpmTexture(resourceLocator_.rampAtlas().string());
    uploadTexture(
        toonRamp.pixels,
        toonRamp.width,
        toonRamp.height,
        VK_FORMAT_R8G8B8A8_UNORM,
        true,
        toonRampTexture_);
}

void AzureRenderApp::createUniformBuffers() {
    const VkDeviceSize size = sizeof(UniformBufferObject);
    uniformBuffers_.resize(kMaxFramesInFlight);
    uniformBufferMemories_.resize(kMaxFramesInFlight);
    uniformBufferMapped_.resize(kMaxFramesInFlight);

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        createBuffer(

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

void AzureRenderApp::createJointBuffers() {
    if (asset_.jointMatrices.empty()) {
        throw std::runtime_error("Asset has no joint-matrix fallback");
    }
    const VkDeviceSize size =
        sizeof(asset_.jointMatrices.front()) * asset_.jointMatrices.size();
    jointBuffers_.resize(kMaxFramesInFlight);
    jointBufferMemories_.resize(kMaxFramesInFlight);
    jointBufferMapped_.resize(kMaxFramesInFlight);

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        createBuffer(
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

void AzureRenderApp::createOitIndexBuffers() {
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
        createBuffer(
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

void AzureRenderApp::createHudBuffers() {
    const VkDeviceSize size =
        sizeof(HudVertex) * kMaxHudVertices;
    hudVertexBuffers_.resize(kMaxFramesInFlight);
    hudVertexBufferMemories_.resize(kMaxFramesInFlight);
    hudVertexBufferMapped_.resize(kMaxFramesInFlight);
    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        createBuffer(
            size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            hudVertexBuffers_[index],
            hudVertexBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                hudVertexBufferMemories_[index],
                0,
                size,
                0,
                &hudVertexBufferMapped_[index]),
            "vkMapMemory(HUD)");
    }
}
