#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace azurerender::internal;


void AzureRenderApp::createPostProcessDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding normalBinding{};
    normalBinding.binding = 0;
    normalBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalBinding.descriptorCount = 1;
    normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding depthBinding = normalBinding;
    depthBinding.binding = 1;
    VkDescriptorSetLayoutBinding shadowBinding = normalBinding;
    shadowBinding.binding = 2;
    VkDescriptorSetLayoutBinding sceneColorBinding = normalBinding;
    sceneColorBinding.binding = 3;
    const std::array bindings = {
        normalBinding,
        depthBinding,
        shadowBinding,
        sceneColorBinding,
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount =
        static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    vkCheck(
        vkCreateDescriptorSetLayout(
            device_,
            &layoutInfo,
            nullptr,
            &postProcessDescriptorSetLayout_),
        "vkCreateDescriptorSetLayout(post process)");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0F;
    vkCheck(
        vkCreateSampler(
            device_,
            &samplerInfo,
            nullptr,
            &screenAttachmentSampler_),
        "vkCreateSampler(screen attachments)");
}




void AzureRenderApp::createPostProcessDescriptorSets() {
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(swapchainImages_.size());
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = descriptorCount * 4;
    VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = descriptorCount;
    vkCheck(
        vkCreateDescriptorPool(
            device_,
            &poolInfo,
            nullptr,
            &postProcessDescriptorPool_),
        "vkCreateDescriptorPool(post process)");

    const std::vector<VkDescriptorSetLayout> layouts(
        descriptorCount,
        postProcessDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = postProcessDescriptorPool_;
    allocateInfo.descriptorSetCount = descriptorCount;
    allocateInfo.pSetLayouts = layouts.data();
    postProcessDescriptorSets_.resize(descriptorCount);
    vkCheck(
        vkAllocateDescriptorSets(
            device_,
            &allocateInfo,
            postProcessDescriptorSets_.data()),
        "vkAllocateDescriptorSets(post process)");

    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = normalImageViews_[index];
        normalInfo.sampler = screenAttachmentSampler_;
        VkDescriptorImageInfo depthInfo{};
        depthInfo.imageLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthInfo.imageView = depthImageViews_[index];
        depthInfo.sampler = screenAttachmentSampler_;
        VkDescriptorImageInfo shadowInfo{};
        shadowInfo.imageLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowInfo.imageView = shadowImageView_;
        shadowInfo.sampler = shadowSampler_;
        VkDescriptorImageInfo sceneColorInfo{};
        sceneColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sceneColorInfo.imageView = sceneColorImageViews_[index];
        sceneColorInfo.sampler = screenAttachmentSampler_;
        std::array<VkWriteDescriptorSet, 4> writes{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = postProcessDescriptorSets_[index];
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &normalInfo;
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = postProcessDescriptorSets_[index];
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        writes[1].pImageInfo = &depthInfo;
        writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[2].dstSet = postProcessDescriptorSets_[index];
        writes[2].dstBinding = 2;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].pImageInfo = &shadowInfo;
        writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[3].dstSet = postProcessDescriptorSets_[index];
        writes[3].dstBinding = 3;
        writes[3].descriptorCount = 1;
        writes[3].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[3].pImageInfo = &sceneColorInfo;
        vkUpdateDescriptorSets(
            device_,
            static_cast<std::uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr);
    }
}
