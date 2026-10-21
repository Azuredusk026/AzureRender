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
    VkDescriptorSetLayoutBinding gridBinding=normalBinding;
    gridBinding.binding=4;gridBinding.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    VkDescriptorSetLayoutBinding lutBinding=normalBinding;lutBinding.binding=5;
    const std::array bindings = {
        normalBinding,
        depthBinding,
        shadowBinding,
        sceneColorBinding,
        gridBinding,
        lutBinding,
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
    synchronizeColorGradingLut(renderSettings_);
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(swapchainImages_.size());
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = descriptorCount * 5;
    VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    const std::array poolSizes{poolSize,VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,descriptorCount}};
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
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

    groundGridUniforms_.resize(descriptorCount);
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        groundGridUniforms_[index]=gpuAllocator_.createBuffer(sizeof(GroundGridUniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        const VkDescriptorBufferInfo gridInfo{groundGridUniforms_[index].buffer,0,sizeof(GroundGridUniform)};
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
        const VkDescriptorImageInfo lutInfo{colorGradingLut_->sampler(),colorGradingLut_->view(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet, 6> writes{};
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
        writes[4]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[4].dstSet=postProcessDescriptorSets_[index];writes[4].dstBinding=4;
        writes[4].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[4].descriptorCount=1;writes[4].pBufferInfo=&gridInfo;
        writes[5]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[5].dstSet=postProcessDescriptorSets_[index];writes[5].dstBinding=5;
        writes[5].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[5].descriptorCount=1;writes[5].pImageInfo=&lutInfo;
        vkUpdateDescriptorSets(
            device_,
            static_cast<std::uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr);
    }
}

void AzureRenderApp::synchronizeColorGradingLut(const azurerender::RenderSettings& settings){
    const auto& binding=settings.grade.lut;
    const auto path=binding.resolvedAsset.empty()?std::filesystem::u8path(binding.asset):binding.resolvedAsset;
    std::string key="identity";
    if(!binding.asset.empty()){
        key=path.generic_u8string()+":"+binding.resourceFingerprint+":"+std::to_string(std::filesystem::file_size(path))
            +":"+std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count());
    }
    if(colorGradingLut_&&colorGradingLutKey_==key)return;
    const auto lut=binding.asset.empty()?azurerender::makeIdentityColorGradingLut():azurerender::loadColorGradingLut(path);
    // A settings change is a load boundary: descriptors and immutable image are replaced only after completion.
    vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(color grading LUT reload)");
    auto candidate=std::make_unique<azurerender::ColorGradingLutGpu>(*rhi_,lut,binding.asset.empty()
        ?azurerender::ColorGradingLutUpload::IdentityPlaceholder:azurerender::ColorGradingLutUpload::Float32);
    for(const auto set:postProcessDescriptorSets_)
        rhi_->writeDescriptorImage({set,5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,candidate->view(),candidate->sampler(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    colorGradingLut_=std::move(candidate);colorGradingLutKey_=std::move(key);
}
