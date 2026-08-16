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
