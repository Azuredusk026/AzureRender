#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace azurerender::internal;

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
