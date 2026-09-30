#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

// Forward declarations keep the VMA header out of this interface so translation
// units that only allocate do not need to see the implementation details.
struct VmaAllocator_T;
struct VmaAllocation_T;
using VmaAllocator = VmaAllocator_T*;
using VmaAllocation = VmaAllocation_T*;

namespace azurerender::rhi {

// A buffer or image allocation owned by the allocator. Callers pass the whole
// struct back to the matching destroy call so the allocator stays the single
// owner of the backing memory.
struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    // Persistently mapped pointer for host-visible allocations, else null.
    void* mapped = nullptr;
    VkDeviceSize size = 0;
};

struct GpuImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipLevels = 1;
};

// Running totals used by the performance baseline to show that a structural
// change reduced allocation pressure.
struct GpuAllocatorStatistics {
    std::uint64_t bufferAllocations = 0;
    std::uint64_t imageAllocations = 0;
    std::uint64_t liveBuffers = 0;
    std::uint64_t liveImages = 0;
    VkDeviceSize bufferBytes = 0;
    VkDeviceSize imageBytes = 0;
};

// Allocation interface. The production implementation is GpuAllocator (VMA);
// test backends provide host-backed fakes so recording logic can run without
// a device.
class IGpuAllocator {
public:
    virtual ~IGpuAllocator() = default;

    [[nodiscard]] virtual GpuBuffer createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        bool hostVisible) = 0;
    virtual void destroyBuffer(GpuBuffer& buffer) noexcept = 0;

    [[nodiscard]] virtual GpuImage createImage(
        const VkImageCreateInfo& createInfo,
        bool hostVisible) = 0;
    virtual void destroyImage(GpuImage& image) noexcept = 0;

    [[nodiscard]] virtual GpuImage createImage2D(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        std::uint32_t mipLevels = 1,
        bool hostVisible = false) = 0;

    virtual void flush(
        const GpuBuffer& buffer,
        VkDeviceSize offset,
        VkDeviceSize size) = 0;

    [[nodiscard]] virtual const GpuAllocatorStatistics& statistics()
        const noexcept = 0;
};

}  // namespace azurerender::rhi
