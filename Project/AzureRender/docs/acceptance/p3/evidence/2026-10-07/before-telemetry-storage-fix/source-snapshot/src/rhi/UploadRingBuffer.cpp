#include "rhi/UploadRingBuffer.hpp"

#include <stdexcept>

namespace azurerender::rhi {

UploadRingBuffer::~UploadRingBuffer() {
    shutdown();
}

void UploadRingBuffer::initialize(
    GpuAllocator& allocator,
    const VkDeviceSize perFrameCapacity,
    const std::uint32_t frameCount,
    const VkDeviceSize offsetAlignment) {
    if (valid()) {
        throw std::runtime_error("UploadRingBuffer is already initialized");
    }
    ring_ = RingFrameAllocator(perFrameCapacity, frameCount, offsetAlignment);
    allocator_ = &allocator;
    // The usage set covers every planned slice consumer (vertex data, uniform
    // and storage bindings, transfer sources) so the buffer never needs a
    // recreate when a new consumer joins.
    buffer_ = allocator_->createBuffer(
        ring_.totalSize(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        true);
}

void UploadRingBuffer::shutdown() {
    if (allocator_ != nullptr) {
        allocator_->destroyBuffer(buffer_);
        allocator_ = nullptr;
    }
    ring_ = RingFrameAllocator{};
}

void UploadRingBuffer::beginFrame(const std::uint32_t frameIndex) {
    ring_.beginFrame(frameIndex);
}

UploadRingBuffer::Slice UploadRingBuffer::allocate(
    const VkDeviceSize size,
    const VkDeviceSize alignment) {
    const VkDeviceSize offset = ring_.allocate(size, alignment);
    Slice slice;
    slice.buffer = buffer_.buffer;
    slice.mapped = static_cast<std::uint8_t*>(buffer_.mapped) + offset;
    slice.offset = offset;
    slice.size = size;
    return slice;
}

}  // namespace azurerender::rhi
