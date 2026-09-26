#pragma once
#include "rhi/IGpuAllocator.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace azurerender {
struct TransientResourceKey {
    std::string name;
    std::uint32_t width = 0, height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    bool operator==(const TransientResourceKey& other) const {
        return width == other.width && height == other.height
            && format == other.format && usage == other.usage;
    }
};
struct TransientResourceLease {
    std::uint64_t frame = 0;
    rhi::GpuImage image{};
    rhi::GpuBuffer buffer{};
};
// Owner must establish GPU completion before retirement and destruction.
class TransientResourcePool final {
public:
    explicit TransientResourcePool(rhi::IGpuAllocator& allocator) : allocator_(allocator) {}
    ~TransientResourcePool() { clear(); }
    TransientResourcePool(const TransientResourcePool&) = delete;
    TransientResourcePool& operator=(const TransientResourcePool&) = delete;
    TransientResourceLease acquireImage(const TransientResourceKey& key, std::uint64_t frame);
    TransientResourceLease acquireBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                         bool hostVisible, std::uint64_t frame);
    void retireFrame(std::uint64_t completedFrame);
    void trim();
    void clear() noexcept;
    std::size_t liveCount() const noexcept { return images_.size() + buffers_.size(); }
private:
    struct ImageEntry { TransientResourceKey key; TransientResourceLease lease; bool inUse = true; };
    struct BufferEntry {
        VkDeviceSize size; VkBufferUsageFlags usage; bool hostVisible;
        TransientResourceLease lease; bool inUse = true;
    };
    rhi::IGpuAllocator& allocator_;
    std::vector<ImageEntry> images_;
    std::vector<BufferEntry> buffers_;
};
} // namespace azurerender
