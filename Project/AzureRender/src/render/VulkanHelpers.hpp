#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace azurerender::vk {

// Shared low-level Vulkan helpers extracted from the application class so both
// the engine core and scene renderers use identical buffer/image/shader
// creation semantics. Every function takes its device/queue dependencies
// explicitly; nothing here owns resources across calls.

std::vector<char> readBinaryFile(const std::string& path);

VkShaderModule createShaderModule(
    VkDevice device,
    const std::vector<char>& code);

std::uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties);

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

void createBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory);

void copyBuffer(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    VkBuffer source,
    VkBuffer destination,
    VkDeviceSize size);

void transitionImageLayout(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    std::uint32_t mipLevels);

void copyBufferToImage(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    VkBuffer source,
    VkImage destination,
    std::uint32_t width,
    std::uint32_t height);

void createImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& memory,
    std::uint32_t mipLevels);

void generateMipmaps(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkQueue queue,
    VkCommandPool commandPool,
    VkImage image,
    VkFormat format,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mipLevels);

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect,
    std::uint32_t mipLevels);

}  // namespace azurerender::vk
