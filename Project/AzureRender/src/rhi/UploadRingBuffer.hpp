#pragma once

#include "rhi/GpuAllocator.hpp"
#include "rhi/RingFrameAllocator.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace azurerender::rhi {

// One large persistently mapped buffer serving all per-frame CPU-to-GPU
// uploads. Replaces a population of small per-frame buffers: callers carve a
// slice after beginFrame and bind it with its byte offset. The frame fence
// retires the window before it is rewritten, so slices need no individual
// lifetime management.
class UploadRingBuffer final {
public:
    struct Slice {
        VkBuffer buffer = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize offset = 0;
        VkDeviceSize size = 0;
    };

    UploadRingBuffer() = default;
    ~UploadRingBuffer();

    UploadRingBuffer(const UploadRingBuffer&) = delete;
    UploadRingBuffer& operator=(const UploadRingBuffer&) = delete;
    UploadRingBuffer(UploadRingBuffer&&) = delete;
    UploadRingBuffer& operator=(UploadRingBuffer&&) = delete;

    // `offsetAlignment` must cover minUniformBufferOffsetAlignment and
    // nonCoherentAtomSize so every slice stays directly bindable.
    void initialize(
        GpuAllocator& allocator,
        VkDeviceSize perFrameCapacity,
        std::uint32_t frameCount,
        VkDeviceSize offsetAlignment);
    void shutdown();

    [[nodiscard]] bool valid() const noexcept {
        return buffer_.buffer != VK_NULL_HANDLE;
    }

    void beginFrame(std::uint32_t frameIndex);
    [[nodiscard]] Slice allocate(VkDeviceSize size, VkDeviceSize alignment = 1);

    // The backing buffer handle, bound with a slice's byte offset.
    [[nodiscard]] VkBuffer buffer() const noexcept { return buffer_.buffer; }

    [[nodiscard]] VkDeviceSize perFrameCapacity() const noexcept {
        return ring_.perFrameCapacity();
    }
    [[nodiscard]] VkDeviceSize totalSize() const noexcept {
        return ring_.totalSize();
    }

private:
    GpuAllocator* allocator_ = nullptr;
    GpuBuffer buffer_;
    RingFrameAllocator ring_;
};

}  // namespace azurerender::rhi
