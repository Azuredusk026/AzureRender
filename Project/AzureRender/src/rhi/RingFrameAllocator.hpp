#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace azurerender::rhi {

// Pure offset math for a segmented upload ring. The ring is one buffer split
// into one window per in-flight frame; a frame rewrites only its own window
// after its fence has retired, so no per-allocation lifetime tracking exists.
// Kept free of Vulkan types so the alignment and reset rules are unit-testable
// without a device.
class RingFrameAllocator {
public:
    RingFrameAllocator() = default;
    RingFrameAllocator(
        const std::uint64_t perFrameCapacity,
        const std::uint32_t frameCount,
        const std::uint64_t alignment)
        : perFrameCapacity_(perFrameCapacity),
          frameCount_(frameCount),
          alignment_(std::max<std::uint64_t>(alignment, 1)) {
        if (perFrameCapacity_ == 0) {
            throw std::invalid_argument("Ring frame capacity must be non-zero");
        }
        if (frameCount_ == 0) {
            throw std::invalid_argument("Ring frame count must be non-zero");
        }
        if ((alignment_ & (alignment_ - 1)) != 0) {
            throw std::invalid_argument(
                "Ring alignment must be a power of two");
        }
        configured_ = true;
    }

    void beginFrame(const std::uint32_t frameIndex) {
        if (!configured_) {
            throw std::logic_error("Ring frame allocator is not configured");
        }
        if (frameIndex >= frameCount_) {
            throw std::out_of_range("Ring frame index out of range");
        }
        windowBegin_ = perFrameCapacity_ * frameIndex;
        cursor_ = windowBegin_;
        windowEnd_ = windowBegin_ + perFrameCapacity_;
    }

    // Returns the absolute byte offset of a slice of `size` bytes. The slice
    // offset honors the ring alignment and any larger caller alignment.
    [[nodiscard]] std::uint64_t allocate(
        const std::uint64_t size,
        const std::uint64_t alignment = 1) {
        if (size == 0) {
            throw std::invalid_argument("Ring allocation size must be non-zero");
        }
        if ((alignment & (alignment - 1)) != 0) {
            throw std::invalid_argument(
                "Ring allocation alignment must be a power of two");
        }
        const std::uint64_t effective =
            std::max(alignment_, std::max<std::uint64_t>(alignment, 1));
        const std::uint64_t aligned =
            (cursor_ + effective - 1) / effective * effective;
        if (aligned < cursor_ || size > windowEnd_ - aligned) {
            throw std::overflow_error("Upload ring frame window exhausted");
        }
        cursor_ = aligned + size;
        return aligned;
    }

    [[nodiscard]] std::uint64_t totalSize() const noexcept {
        return perFrameCapacity_ * frameCount_;
    }
    [[nodiscard]] std::uint64_t perFrameCapacity() const noexcept {
        return perFrameCapacity_;
    }
    [[nodiscard]] bool configured() const noexcept { return configured_; }

private:
    std::uint64_t perFrameCapacity_ = 0;
    std::uint32_t frameCount_ = 0;
    std::uint64_t alignment_ = 1;
    std::uint64_t windowBegin_ = 0;
    std::uint64_t windowEnd_ = 0;
    std::uint64_t cursor_ = 0;
    bool configured_ = false;
};

}  // namespace azurerender::rhi
