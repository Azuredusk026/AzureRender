#include "rhi/GpuAllocator.hpp"

#include <vk_mem_alloc.h>

#include <stdexcept>
#include <string>

namespace azurerender::rhi {
namespace {

void check(const VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::string(what) + " failed with VkResult "
            + std::to_string(static_cast<int>(result)));
    }
}

}  // namespace

GpuAllocator::~GpuAllocator() {
    shutdown();
}

void GpuAllocator::initialize(
    const VkInstance instance,
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const std::uint32_t apiVersion) {
    if (allocator_ != nullptr) {
        throw std::runtime_error("GpuAllocator is already initialized");
    }

    // Vulkan entry points are resolved dynamically. The project links the
    // loader rather than a static Vulkan library, so letting VMA fetch pointers
    // through vkGetInstanceProcAddr keeps it independent of how the loader was
    // linked.
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo{};
    createInfo.instance = instance;
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = device;
    createInfo.vulkanApiVersion = apiVersion;
    createInfo.pVulkanFunctions = &functions;

    check(
        vmaCreateAllocator(&createInfo, &allocator_),
        "vmaCreateAllocator");
    statistics_ = GpuAllocatorStatistics{};
}

void GpuAllocator::shutdown() {
    if (allocator_ == nullptr) {
        return;
    }
    vmaDestroyAllocator(allocator_);
    allocator_ = nullptr;
}

GpuBuffer GpuAllocator::createBuffer(
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const bool hostVisible) {
    if (allocator_ == nullptr) {
        throw std::runtime_error("GpuAllocator is not initialized");
    }
    if (size == 0) {
        throw std::invalid_argument("GpuAllocator buffer size must be non-zero");
    }

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    if (hostVisible) {
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocationInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    } else {
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

    GpuBuffer result;
    VmaAllocationInfo info{};
    check(
        vmaCreateBuffer(
            allocator_,
            &bufferInfo,
            &allocationInfo,
            &result.buffer,
            &result.allocation,
            &info),
        "vmaCreateBuffer");
    result.mapped = hostVisible ? info.pMappedData : nullptr;
    result.size = size;

    ++statistics_.bufferAllocations;
    ++statistics_.liveBuffers;
    statistics_.bufferBytes += info.size;
    return result;
}

void GpuAllocator::destroyBuffer(GpuBuffer& buffer) noexcept {
    if (allocator_ == nullptr || buffer.buffer == VK_NULL_HANDLE) {
        return;
    }
    vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
    if (statistics_.liveBuffers > 0) {
        --statistics_.liveBuffers;
    }
    buffer = GpuBuffer{};
}

GpuImage GpuAllocator::createImage(
    const VkImageCreateInfo& createInfo,
    const bool hostVisible) {
    if (allocator_ == nullptr) {
        throw std::runtime_error("GpuAllocator is not initialized");
    }

    VmaAllocationCreateInfo allocationInfo{};
    if (hostVisible) {
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocationInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    } else {
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

    GpuImage result;
    VmaAllocationInfo info{};
    check(
        vmaCreateImage(
            allocator_,
            &createInfo,
            &allocationInfo,
            &result.image,
            &result.allocation,
            &info),
        "vmaCreateImage");

    ++statistics_.imageAllocations;
    ++statistics_.liveImages;
    statistics_.imageBytes += info.size;
    return result;
}

void GpuAllocator::destroyImage(GpuImage& image) noexcept {
    if (allocator_ == nullptr || image.image == VK_NULL_HANDLE) {
        return;
    }
    vmaDestroyImage(allocator_, image.image, image.allocation);
    if (statistics_.liveImages > 0) {
        --statistics_.liveImages;
    }
    image = GpuImage{};
}

void GpuAllocator::flush(
    const GpuBuffer& buffer,
    const VkDeviceSize offset,
    const VkDeviceSize size) {
    if (allocator_ == nullptr || buffer.allocation == nullptr) {
        return;
    }
    check(
        vmaFlushAllocation(allocator_, buffer.allocation, offset, size),
        "vmaFlushAllocation");
}

}  // namespace azurerender::rhi
