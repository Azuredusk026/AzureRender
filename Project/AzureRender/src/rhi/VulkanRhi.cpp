#include "rhi/VulkanRhi.hpp"

#include "render/RenderMath.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace azurerender::rhi {

using azurerender::internal::vkCheck;

void VulkanRhi::runOneShot(
    const char* label,
    const std::function<void(VkCommandBuffer)>& record) {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        label);
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), label);
    record(commandBuffer);
    vkCheck(vkEndCommandBuffer(commandBuffer), label);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE),
        label);
    vkCheck(vkQueueWaitIdle(graphicsQueue_), label);
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanRhi::copyBuffer(
    const GpuBuffer& source,
    const GpuBuffer& destination,
    const VkDeviceSize size) {
    runOneShot("copyBuffer", [&](VkCommandBuffer commandBuffer) {
        const VkBufferCopy region{0, 0, size};
        vkCmdCopyBuffer(
            commandBuffer, source.buffer, destination.buffer, 1, &region);
    });
}

void VulkanRhi::transitionImageLayout(
    const GpuImage& image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout,
    const std::uint32_t mipLevels) {
    runOneShot("transitionImageLayout", [&](VkCommandBuffer commandBuffer) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage = 0;
        VkPipelineStageFlags destinationStage = 0;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (
            oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::runtime_error("Unsupported image layout transition");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage,
            destinationStage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    });
}

void VulkanRhi::copyBufferToImage(
    const GpuBuffer& source,
    const GpuImage& destination,
    const std::uint32_t width,
    const std::uint32_t height) {
    runOneShot("copyBufferToImage", [&](VkCommandBuffer commandBuffer) {
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(
            commandBuffer,
            source.buffer,
            destination.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
    });
}

void VulkanRhi::clearImage(const GpuImage& image) {
    runOneShot("clearImage", [&](VkCommandBuffer commandBuffer) {
        const VkClearColorValue clear{{0.0F, 0.0F, 0.0F, 1.0F}};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;
        vkCmdClearColorImage(
            commandBuffer,
            image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear,
            1,
            &range);
    });
}

void VulkanRhi::generateMipmaps(
    const GpuImage& image,
    const VkFormat format,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t mipLevels) {
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(
        physicalDevice_, format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures
          & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error(
            "Texture image format does not support linear blitting");
    }

    runOneShot("generateMipmaps", [&](VkCommandBuffer commandBuffer) {
        std::int32_t mipWidth = static_cast<std::int32_t>(width);
        std::int32_t mipHeight = static_cast<std::int32_t>(height);
        for (std::uint32_t level = 1; level < mipLevels; ++level) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image.image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = level - 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = level - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {
                mipWidth > 1 ? mipWidth / 2 : 1,
                mipHeight > 1 ? mipHeight / 2 : 1,
                1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = level;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            vkCmdBlitImage(
                commandBuffer,
                image.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
            if (mipWidth > 1) {
                mipWidth /= 2;
            }
            if (mipHeight > 1) {
                mipHeight /= 2;
            }
        }

        VkImageMemoryBarrier lastBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        lastBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        lastBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        lastBarrier.image = image.image;
        lastBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        lastBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
        lastBarrier.subresourceRange.levelCount = 1;
        lastBarrier.subresourceRange.layerCount = 1;
        lastBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        lastBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        lastBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        lastBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &lastBarrier);
    });
}

VkImageView VulkanRhi::createImageView(
    const VkImage image,
    const VkFormat format,
    const VkImageAspectFlags aspect,
    const std::uint32_t mipLevels,
    const std::uint32_t baseMipLevel) {
    VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.baseMipLevel = baseMipLevel;
    createInfo.subresourceRange.levelCount = std::max(mipLevels, 1U);
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    vkCheck(
        vkCreateImageView(device_, &createInfo, nullptr, &imageView),
        "vkCreateImageView");
    return imageView;
}

void VulkanRhi::destroyImageView(const VkImageView view) {
    vkDestroyImageView(device_, view, nullptr);
}

VkSampler VulkanRhi::createSampler(const SamplerDesc& desc) {
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = desc.filter;
    samplerInfo.minFilter = desc.filter;
    samplerInfo.addressModeU = desc.addressU;
    samplerInfo.addressModeV = desc.addressV;
    samplerInfo.addressModeW = desc.addressW;
    samplerInfo.mipmapMode = desc.mipmapLinear
        ? VK_SAMPLER_MIPMAP_MODE_LINEAR
        : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.maxLod = desc.maxLod;
    VkSampler sampler = VK_NULL_HANDLE;
    vkCheck(
        vkCreateSampler(device_, &samplerInfo, nullptr, &sampler),
        "vkCreateSampler");
    return sampler;
}

void VulkanRhi::destroySampler(const VkSampler sampler) {
    vkDestroySampler(device_, sampler, nullptr);
}

void VulkanRhi::executeOneShot(
    const std::function<void(ICommandRecorder&)>& record) {
    runOneShot("executeOneShot", [&](VkCommandBuffer commandBuffer) {
        VulkanCommandRecorder recorder(commandBuffer);
        record(recorder);
    });
}

VkShaderModule VulkanRhi::createShaderModule(
    const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    vkCheck(
        vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule),
        "vkCreateShaderModule");
    return shaderModule;
}

void VulkanRhi::destroyShaderModule(const VkShaderModule shader) {
    vkDestroyShaderModule(device_, shader, nullptr);
}

VkPipelineLayout VulkanRhi::createPipelineLayout(
    const VkDescriptorSetLayout setLayout,
    const PushConstantRangeDesc* pushConstants) {
    VkPipelineLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    VkPushConstantRange pushConstantRange{};
    if (pushConstants != nullptr && pushConstants->size > 0) {
        pushConstantRange.stageFlags = pushConstants->stages;
        pushConstantRange.size = pushConstants->size;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
    }
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vkCheck(
        vkCreatePipelineLayout(
            device_, &layoutInfo, nullptr, &pipelineLayout),
        "vkCreatePipelineLayout");
    return pipelineLayout;
}

void VulkanRhi::destroyPipelineLayout(const VkPipelineLayout layout) {
    vkDestroyPipelineLayout(device_, layout, nullptr);
}

VkPipeline VulkanRhi::createComputePipeline(const ComputePipelineDesc& desc) {
    if (!desc.shader || !desc.layout) throw std::invalid_argument("Compute pipeline requires shader and layout");
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = desc.shader;
    info.stage.pName = "main";
    info.layout = desc.layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCheck(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "vkCreateComputePipelines");
    return pipeline;
}
void VulkanCommandRecorder::bindComputePipeline(VkPipeline pipeline) {
    if (exclusiveRecording_ && boundComputePipeline_ == pipeline) return;
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    boundComputePipeline_ = pipeline;
}
void VulkanCommandRecorder::bindComputeDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set) {
    if (exclusiveRecording_ && boundComputeLayout_ == layout && boundComputeSet_ == set) return;
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
    boundComputeLayout_ = layout;
    boundComputeSet_ = set;
}

VkPipeline VulkanRhi::createGraphicsPipeline(
    const GraphicsPipelineDesc& desc) {
    VkPipelineShaderStageCreateInfo vertexStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = desc.vertexShader;
    vertexStage.pName = "main";
    VkPipelineShaderStageCreateInfo fragmentStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = desc.fragmentShader;
    fragmentStage.pName = "main";
    const std::array shaderStages = {vertexStage, fragmentStage};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = desc.vertexStride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::vector<VkVertexInputAttributeDescription> attributes(
        desc.vertexAttributes.size());
    for (std::size_t index = 0; index < desc.vertexAttributes.size(); ++index) {
        attributes[index] = {
            desc.vertexAttributes[index].location,
            0,
            desc.vertexAttributes[index].format,
            desc.vertexAttributes[index].offset,
        };
    }
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (desc.vertexStride > 0) {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();
    }

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
    rasterizer.cullMode = desc.cullMode;
    rasterizer.frontFace = desc.frontFace;
    rasterizer.lineWidth = 1.0F;
    if (desc.depthBias) {
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = desc.depthBiasConstant;
        rasterizer.depthBiasSlopeFactor = desc.depthBiasSlope;
    }
    VkPipelineMultisampleStateCreateInfo multisampling{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(
        desc.colorAttachmentCount);
    for (auto& attachment : colorBlendAttachments) {
        attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (desc.alphaBlend) {
            attachment.blendEnable = VK_TRUE;
            attachment.srcColorBlendFactor = desc.multiplicativeTint ? VK_BLEND_FACTOR_DST_COLOR : desc.premultipliedAlpha ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp = VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachment.dstAlphaBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }
    VkPipelineColorBlendStateCreateInfo colorBlending{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.attachmentCount =
        static_cast<std::uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments =
        colorBlendAttachments.empty() ? nullptr : colorBlendAttachments.data();
    const std::array dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount =
        static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

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
    pipelineInfo.layout = desc.layout;
    pipelineInfo.renderPass = desc.renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCheck(
        vkCreateGraphicsPipelines(
            device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
        "vkCreateGraphicsPipelines");
    return pipeline;
}

void VulkanRhi::destroyPipeline(const VkPipeline pipeline) {
    vkDestroyPipeline(device_, pipeline, nullptr);
}

VkDescriptorSetLayout VulkanRhi::createDescriptorSetLayout(
    const std::vector<DescriptorBindingDesc>& bindings) {
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings(bindings.size());
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        layoutBindings[index].binding = bindings[index].binding;
        layoutBindings[index].descriptorType = bindings[index].type;
        layoutBindings[index].descriptorCount = bindings[index].count;
        layoutBindings[index].stageFlags = bindings[index].stages;
    }
    VkDescriptorSetLayoutCreateInfo createInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount =
        static_cast<std::uint32_t>(layoutBindings.size());
    createInfo.pBindings = layoutBindings.data();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    vkCheck(
        vkCreateDescriptorSetLayout(device_, &createInfo, nullptr, &layout),
        "vkCreateDescriptorSetLayout");
    return layout;
}

void VulkanRhi::destroyDescriptorSetLayout(const VkDescriptorSetLayout layout) {
    vkDestroyDescriptorSetLayout(device_, layout, nullptr);
}

VkDescriptorPool VulkanRhi::createDescriptorPool(
    const DescriptorPoolDesc& desc) {
    std::vector<VkDescriptorPoolSize> poolSizes(desc.sizes.size());
    for (std::size_t index = 0; index < desc.sizes.size(); ++index) {
        poolSizes[index].type = desc.sizes[index].type;
        poolSizes[index].descriptorCount = desc.sizes[index].count;
    }
    VkDescriptorPoolCreateInfo createInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = desc.maxSets;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    vkCheck(
        vkCreateDescriptorPool(device_, &createInfo, nullptr, &pool),
        "vkCreateDescriptorPool");
    return pool;
}

void VulkanRhi::destroyDescriptorPool(const VkDescriptorPool pool) {
    vkDestroyDescriptorPool(device_, pool, nullptr);
}

std::vector<VkDescriptorSet> VulkanRhi::allocateDescriptorSets(
    const VkDescriptorPool pool,
    const VkDescriptorSetLayout layout,
    const std::uint32_t count) {
    const std::vector<VkDescriptorSetLayout> layouts(count, layout);
    VkDescriptorSetAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = count;
    allocateInfo.pSetLayouts = layouts.data();
    std::vector<VkDescriptorSet> sets(count);
    vkCheck(
        vkAllocateDescriptorSets(device_, &allocateInfo, sets.data()),
        "vkAllocateDescriptorSets");
    return sets;
}

void VulkanRhi::writeDescriptorImage(const DescriptorImageWrite& write) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = write.layout;
    imageInfo.imageView = write.view;
    imageInfo.sampler = write.sampler;
    VkWriteDescriptorSet descriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = write.set;
    descriptorWrite.dstBinding = write.binding;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = write.type;
    descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
}

void VulkanRhi::writeDescriptorImageArray(
    const DescriptorImageArrayWrite& write) {
    VkWriteDescriptorSet descriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = write.set;
    descriptorWrite.dstBinding = write.binding;
    descriptorWrite.descriptorCount =
        static_cast<std::uint32_t>(write.elements.size());
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = write.elements.data();
    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
}

void VulkanRhi::writeDescriptorBuffer(const DescriptorBufferWrite& write) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = write.buffer;
    bufferInfo.range = write.range;
    VkWriteDescriptorSet descriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = write.set;
    descriptorWrite.dstBinding = write.binding;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = write.type;
    descriptorWrite.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
}

VkRenderPass VulkanRhi::createRenderPass(const RenderPassDesc& desc) {
    std::vector<VkAttachmentDescription> attachments(desc.attachments.size());
    std::vector<VkAttachmentReference> colorReferences;
    VkAttachmentReference depthReference{};
    for (std::size_t index = 0; index < desc.attachments.size(); ++index) {
        const RenderPassAttachmentDesc& source = desc.attachments[index];
        VkAttachmentDescription& attachment = attachments[index];
        attachment.format = source.format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = source.clear
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = source.store
            ? VK_ATTACHMENT_STORE_OP_STORE
            : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = source.initialLayout;
        attachment.finalLayout = source.finalLayout;
        if (source.isDepth) {
            depthReference.attachment = static_cast<std::uint32_t>(index);
            depthReference.layout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else {
            VkAttachmentReference reference{};
            reference.attachment = static_cast<std::uint32_t>(index);
            reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorReferences.push_back(reference);
        }
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount =
        static_cast<std::uint32_t>(colorReferences.size());
    subpass.pColorAttachments =
        colorReferences.empty() ? nullptr : colorReferences.data();
    subpass.pDepthStencilAttachment =
        desc.depthAttachment >= 0 ? &depthReference : nullptr;

    std::vector<VkSubpassDependency> dependencies;
    if (desc.externalReadDependency) {
        VkSubpassDependency externalToPass{};
        externalToPass.srcSubpass = VK_SUBPASS_EXTERNAL;
        externalToPass.dstSubpass = 0;
        externalToPass.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        externalToPass.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        externalToPass.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        externalToPass.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if(desc.depthAttachment>=0){
            externalToPass.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            externalToPass.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        externalToPass.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies.push_back(externalToPass);

        VkSubpassDependency passToExternal{};
        passToExternal.srcSubpass = 0;
        passToExternal.dstSubpass = VK_SUBPASS_EXTERNAL;
        passToExternal.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        passToExternal.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        passToExternal.srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if(desc.depthAttachment>=0){
            passToExternal.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            passToExternal.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        passToExternal.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        passToExternal.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies.push_back(passToExternal);
    }

    VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount =
        static_cast<std::uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount =
        static_cast<std::uint32_t>(dependencies.size());
    renderPassInfo.pDependencies =
        dependencies.empty() ? nullptr : dependencies.data();
    VkRenderPass renderPass = VK_NULL_HANDLE;
    vkCheck(
        vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass),
        "vkCreateRenderPass");
    return renderPass;
}

void VulkanRhi::destroyRenderPass(const VkRenderPass renderPass) {
    vkDestroyRenderPass(device_, renderPass, nullptr);
}

VkFramebuffer VulkanRhi::createFramebuffer(const FramebufferDesc& desc) {
    VkFramebufferCreateInfo framebufferInfo{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = desc.renderPass;
    framebufferInfo.attachmentCount =
        static_cast<std::uint32_t>(desc.attachments.size());
    framebufferInfo.pAttachments = desc.attachments.data();
    framebufferInfo.width = desc.width;
    framebufferInfo.height = desc.height;
    framebufferInfo.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    vkCheck(
        vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffer),
        "vkCreateFramebuffer");
    return framebuffer;
}

void VulkanRhi::destroyFramebuffer(const VkFramebuffer framebuffer) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
}

// ---------------------------------------------------------------------------
// VulkanCommandRecorder
// ---------------------------------------------------------------------------

void VulkanCommandRecorder::beginRenderPass(const RenderPassBeginDesc& desc) {
    VkRenderPassBeginInfo beginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    beginInfo.renderPass = desc.renderPass;
    beginInfo.framebuffer = desc.framebuffer;
    beginInfo.renderArea.extent = desc.extent;
    beginInfo.clearValueCount =
        static_cast<std::uint32_t>(desc.clearValues.size());
    beginInfo.pClearValues = desc.clearValues.data();
    vkCmdBeginRenderPass(
        commandBuffer_, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandRecorder::endRenderPass() {
    vkCmdEndRenderPass(commandBuffer_);
}

void VulkanCommandRecorder::setViewport(
    const float width,
    const float height,
    const float x,
    const float y) {
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
}

void VulkanCommandRecorder::setScissor(
    const VkExtent2D extent,
    const VkOffset2D offset) {
    const VkRect2D scissor{offset, extent};
    vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
}

void VulkanCommandRecorder::bindPipeline(const VkPipeline pipeline) {
    if (exclusiveRecording_ && boundGraphicsPipeline_ == pipeline) return;
    vkCmdBindPipeline(
        commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    boundGraphicsPipeline_ = pipeline;
}

void VulkanCommandRecorder::bindDescriptorSet(
    const VkPipelineLayout layout,
    const VkDescriptorSet set) {
    if (exclusiveRecording_ && boundGraphicsLayout_ == layout && boundGraphicsSet_ == set) return;
    vkCmdBindDescriptorSets(
        commandBuffer_,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout,
        0,
        1,
        &set,
        0,
        nullptr);
    boundGraphicsLayout_ = layout;
    boundGraphicsSet_ = set;
}

void VulkanCommandRecorder::pushConstants(
    const VkPipelineLayout layout,
    const VkShaderStageFlags stages,
    const std::uint32_t offset,
    const void* data,
    const std::uint32_t size) {
    vkCmdPushConstants(commandBuffer_, layout, stages, offset, size, data);
}

void VulkanCommandRecorder::bindVertexBuffer(
    const VkBuffer buffer,
    const VkDeviceSize offset) {
    const VkDeviceSize offsets[] = {offset};
    vkCmdBindVertexBuffers(commandBuffer_, 0, 1, &buffer, offsets);
}

void VulkanCommandRecorder::bindIndexBuffer(
    const VkBuffer buffer,
    const VkDeviceSize offset) {
    vkCmdBindIndexBuffer(
        commandBuffer_, buffer, offset, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandRecorder::draw(const std::uint32_t vertexCount) {
    vkCmdDraw(commandBuffer_, vertexCount, 1, 0, 0);
}

void VulkanCommandRecorder::drawIndexed(
    const std::uint32_t indexCount,
    const std::uint32_t firstIndex,
    const std::uint32_t instanceCount,
    const std::uint32_t firstInstance) {
    vkCmdDrawIndexed(
        commandBuffer_, indexCount, instanceCount, firstIndex, 0,
        firstInstance);
}

void VulkanCommandRecorder::imageBarrier(const ImageBarrierDesc& barrier) {
    validateImageBarrier(barrier);
    VkImageMemoryBarrier imageBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageBarrier.oldLayout = barrier.oldLayout;
    imageBarrier.newLayout = barrier.newLayout;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = barrier.image;
    imageBarrier.subresourceRange.aspectMask = barrier.aspectMask;
    imageBarrier.subresourceRange.baseMipLevel = barrier.baseMipLevel;
    imageBarrier.subresourceRange.levelCount = barrier.mipLevels;
    imageBarrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = barrier.srcStageMask;
    VkPipelineStageFlags destinationStage = barrier.dstStageMask;
    imageBarrier.srcAccessMask = barrier.srcAccessMask;
    imageBarrier.dstAccessMask = barrier.dstAccessMask;
    if (sourceStage == 0 && destinationStage == 0) {
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (barrier.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (
            barrier.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        if (barrier.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (
            barrier.oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            imageBarrier.srcAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
    }
    vkCmdPipelineBarrier(
        commandBuffer_,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageBarrier);
}

void VulkanCommandRecorder::drawIndexedIndirect(
    VkBuffer buffer, VkDeviceSize offset, std::uint32_t drawCount,
    std::uint32_t stride) {
    if (drawCount == 0) return;
    if (buffer == VK_NULL_HANDLE || offset % 4 != 0
        || stride < sizeof(VkDrawIndexedIndirectCommand) || stride % 4 != 0)
        throw std::invalid_argument("Invalid indexed indirect draw parameters");
    vkCmdDrawIndexedIndirect(commandBuffer_, buffer, offset, drawCount, stride);
}

void VulkanCommandRecorder::dispatch(
    const std::uint32_t groupCountX,
    const std::uint32_t groupCountY,
    const std::uint32_t groupCountZ) {
    if (!groupCountX || !groupCountY || !groupCountZ) throw std::invalid_argument("Empty compute dispatch");
    vkCmdDispatch(commandBuffer_, groupCountX, groupCountY, groupCountZ);
}

void VulkanCommandRecorder::bufferBarrier(const BufferBarrierDesc& barrier) {
    VkBufferMemoryBarrier bufferBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    bufferBarrier.srcAccessMask = barrier.srcAccessMask;
    bufferBarrier.dstAccessMask = barrier.dstAccessMask;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = barrier.buffer;
    bufferBarrier.offset = barrier.offset;
    bufferBarrier.size = barrier.size;
    const VkPipelineStageFlags sourceStage = barrier.srcStageMask == 0
        ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        : barrier.srcStageMask;
    const VkPipelineStageFlags destinationStage = barrier.dstStageMask == 0
        ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
        : barrier.dstStageMask;
    vkCmdPipelineBarrier(
        commandBuffer_, sourceStage, destinationStage, 0, 0, nullptr, 1,
        &bufferBarrier, 0, nullptr);
}

void VulkanCommandRecorder::clearColorImage(
    const VkImage image,
    const VkImageLayout layout,
    const VkClearColorValue& color) {
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    vkCmdClearColorImage(commandBuffer_, image, layout, &color, 1, &range);
}

void VulkanCommandRecorder::copyImageToBuffer(
    const VkImage image,
    const VkBuffer buffer,
    const VkExtent2D extent) {
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(
        commandBuffer_,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        buffer,
        1,
        &copy);
}

void VulkanCommandRecorder::writeTimestamp(
    const VkQueryPool pool,
    const std::uint32_t query,
    const VkPipelineStageFlagBits stage) {
    vkCmdWriteTimestamp(commandBuffer_, stage, pool, query);
}

}  // namespace azurerender::rhi
