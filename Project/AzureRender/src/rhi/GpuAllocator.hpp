#pragma once

#include "rhi/IGpuAllocator.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace azurerender::rhi {

// Owns the VMA allocator for a device. Constructed once by the engine and
// handed to subsystems that need GPU memory; scene renderers never create their
// own allocator.
class GpuAllocator final : public IGpuAllocator {
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
        bool hostVisible) override;
    void destroyBuffer(GpuBuffer& buffer) noexcept override;

    [[nodiscard]] GpuImage createImage(
        const VkImageCreateInfo& createInfo,
        bool hostVisible) override;
    void destroyImage(GpuImage& image) noexcept override;

    // Standard 2D optimal-tiling image; the common shape for textures and
    // offscreen targets.
    [[nodiscard]] GpuImage createImage2D(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        std::uint32_t mipLevels = 1,
        bool hostVisible = false) override;

    // Flushes a persistently mapped range on a non-coherent memory type. Safe
    // to call unconditionally; it is a no-op when the memory is coherent.
    void flush(
        const GpuBuffer& buffer,
        VkDeviceSize offset,
        VkDeviceSize size) override;

    [[nodiscard]] const GpuAllocatorStatistics& statistics()
        const noexcept override {
        return statistics_;
    }

private:
    VmaAllocator allocator_ = nullptr;
    GpuAllocatorStatistics statistics_;
};

}  // namespace azurerender::rhi
