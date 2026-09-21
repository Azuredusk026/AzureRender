#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

// Forward declarations keep the VMA header out of this interface so translation
// units that only allocate do not need to see the implementation details.
struct VmaAllocator_T;
struct VmaAllocation_T;
using VmaAllocator = VmaAllocator_T*;
using VmaAllocation = VmaAllocation_T*;

namespace azurerender::rhi {

// A buffer or image allocation owned by GpuAllocator. The allocation handle is
// opaque here; callers pass the whole struct back to the matching destroy call
// so the allocator stays the single owner of the backing memory.
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

// Owns the VMA allocator for a device. Constructed once by the engine and
// handed to subsystems that need GPU memory; scene renderers never create their
// own allocator.
class GpuAllocator final {
public:
    GpuAllocator() = default;
    ~GpuAllocator();

    GpuAllocator(const GpuAllocator&) = delete;
    GpuAllocator& operator=(const GpuAllocator&) = delete;
    GpuAllocator(GpuAllocator&&) = delete;
    GpuAllocator& operator=(GpuAllocator&&) = delete;

    void initialize(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        std::uint32_t apiVersion);
    void shutdown();

    [[nodiscard]] bool valid() const noexcept { return allocator_ != nullptr; }

    // Allocates a buffer. `hostVisible` selects a mappable memory type and
    // keeps the allocation persistently mapped, which removes the map/unmap
    // pair from per-frame upload paths.
    [[nodiscard]] GpuBuffer createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        bool hostVisible);
    void destroyBuffer(GpuBuffer& buffer) noexcept;

    [[nodiscard]] GpuImage createImage(
        const VkImageCreateInfo& createInfo,
        bool hostVisible);
    void destroyImage(GpuImage& image) noexcept;

    // Standard 2D optimal-tiling image; the common shape for textures and
    // offscreen targets.
    [[nodiscard]] GpuImage createImage2D(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        std::uint32_t mipLevels = 1,
        bool hostVisible = false);

    // Flushes a persistently mapped range on a non-coherent memory type. Safe
    // to call unconditionally; it is a no-op when the memory is coherent.
    void flush(const GpuBuffer& buffer, VkDeviceSize offset, VkDeviceSize size);

    [[nodiscard]] const GpuAllocatorStatistics& statistics() const noexcept {
        return statistics_;
    }

private:
    VmaAllocator allocator_ = nullptr;
    GpuAllocatorStatistics statistics_;
};

}  // namespace azurerender::rhi
