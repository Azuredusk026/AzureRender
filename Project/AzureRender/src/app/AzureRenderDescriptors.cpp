#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace azurerender::internal;

void AzureRenderApp::createDescriptorSetLayout() {
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
    };
    VkDescriptorSetLayoutCreateInfo createInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    vkCheck(
        vkCreateDescriptorSetLayout(device_, &createInfo, nullptr, &descriptorSetLayout_),
        "vkCreateDescriptorSetLayout");
}

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



void AzureRenderApp::createDescriptorPool() {
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight * asset_.materials.size());
    const std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
    }};

    VkDescriptorPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = descriptorCount;
    vkCheck(
        vkCreateDescriptorPool(device_, &createInfo, nullptr, &descriptorPool_),
        "vkCreateDescriptorPool");
}

void AzureRenderApp::createDescriptorSets() {
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

        for (std::size_t material = 0; material < asset_.materials.size(); ++material) {
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

            std::array<VkWriteDescriptorSet, 12> writes{};
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
            vkUpdateDescriptorSets(
                device_,
                static_cast<std::uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr);
        }
    }
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
