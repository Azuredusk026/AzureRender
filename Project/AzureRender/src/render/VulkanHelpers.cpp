#include "render/VulkanHelpers.hpp"
#include "app/AzureRenderInternal.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace azurerender::vk {

using azurerender::internal::vkCheck;

std::vector<char> readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open shader: " + path);
    }
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

VkShaderModule createShaderModule(
    const VkDevice device,
    const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    vkCheck(
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
        "vkCreateShaderModule");
    return shaderModule;
}

std::uint32_t findMemoryType(
    const VkPhysicalDevice physicalDevice,
    const std::uint32_t typeFilter,
    const VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount;
         ++index) {
        if ((typeFilter & (1U << index)) != 0U
            && (memoryProperties.memoryTypes[index].propertyFlags & properties)
                == properties) {
            return index;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type was found");
}

VkFormat findDepthFormat(const VkPhysicalDevice physicalDevice) {
    constexpr std::array candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(
            physicalDevice, format, &properties);
        constexpr VkFormatFeatureFlags requiredFeatures =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & requiredFeatures)
            == requiredFeatures) {
            return format;
        }
    }
    throw std::runtime_error("No supported depth buffer format was found");
}

void createBuffer(
    const VkDevice device,
    const VkPhysicalDevice physicalDevice,
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex =
        findMemoryType(physicalDevice, requirements.memoryTypeBits, properties);
    vkCheck(
        vkAllocateMemory(device, &allocateInfo, nullptr, &memory),
        "vkAllocateMemory(buffer)");
    vkCheck(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
}

void copyBuffer(
    const VkDevice device,
    const VkQueue queue,
    const VkCommandPool commandPool,
    const VkBuffer source,
    const VkBuffer destination,
    const VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(copy)");
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "vkBeginCommandBuffer(copy)");

    const VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(commandBuffer, source, destination, 1, &region);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(copy)");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(copy)");
    vkCheck(vkQueueWaitIdle(queue), "vkQueueWaitIdle(copy)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void transitionImageLayout(
    const VkDevice device,
    const VkQueue queue,
    const VkCommandPool commandPool,
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout,
    const std::uint32_t mipLevels) {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(image transition)");

    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "vkBeginCommandBuffer(image transition)");

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
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
    vkCheck(
        vkEndCommandBuffer(commandBuffer),
        "vkEndCommandBuffer(image transition)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(image transition)");
    vkCheck(vkQueueWaitIdle(queue), "vkQueueWaitIdle(image transition)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBufferToImage(
    const VkDevice device,
    const VkQueue queue,
    const VkCommandPool commandPool,
    const VkBuffer source,
    const VkImage destination,
    const std::uint32_t width,
    const std::uint32_t height) {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(image copy)");

    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "vkBeginCommandBuffer(image copy)");
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(
        commandBuffer,
        source,
        destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(image copy)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(image copy)");
    vkCheck(vkQueueWaitIdle(queue), "vkQueueWaitIdle(image copy)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void createImage(
    const VkDevice device,
    const VkPhysicalDevice physicalDevice,
    const std::uint32_t width,
    const std::uint32_t height,
    const VkFormat format,
    const VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& memory,
    const std::uint32_t mipLevels) {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = std::max(mipLevels, 1U);
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateImage(device, &imageInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, image, &requirements);
    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(
        physicalDevice,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkCheck(
        vkAllocateMemory(device, &allocateInfo, nullptr, &memory),
        "vkAllocateMemory(image)");
    vkCheck(vkBindImageMemory(device, image, memory, 0), "vkBindImageMemory");
}

void generateMipmaps(
    const VkDevice device,
    const VkPhysicalDevice physicalDevice,
    const VkQueue queue,
    const VkCommandPool commandPool,
    const VkImage image,
    const VkFormat format,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t mipLevels) {
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(
        physicalDevice, format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures
          & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error(
            "Texture image format does not support linear blitting");
    }

    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(mipmap)");
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "vkBeginCommandBuffer(mipmap)");

    std::int32_t mipWidth = static_cast<std::int32_t>(width);
    std::int32_t mipHeight = static_cast<std::int32_t>(height);
    for (std::uint32_t level = 1; level < mipLevels; ++level) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
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
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
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
    lastBarrier.image = image;
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

    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(mipmap)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(mipmap)");
    vkCheck(vkQueueWaitIdle(queue), "vkQueueWaitIdle(mipmap)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

VkImageView createImageView(
    const VkDevice device,
    const VkImage image,
    const VkFormat format,
    const VkImageAspectFlags aspect,
    const std::uint32_t mipLevels) {
    VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.levelCount = std::max(mipLevels, 1U);
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    vkCheck(
        vkCreateImageView(device, &createInfo, nullptr, &imageView),
        "vkCreateImageView");
    return imageView;
}

}  // namespace azurerender::vk
