#include "render/TransientResourcePool.hpp"
#include <algorithm>
#include <stdexcept>
namespace azurerender {
TransientResourceLease TransientResourcePool::acquireImage(
    const TransientResourceKey& key, std::uint64_t frame) {
    if (!key.width || !key.height || !key.usage || key.format == VK_FORMAT_UNDEFINED)
        throw std::invalid_argument("Invalid transient image description");
    for (auto& entry : images_) if (!entry.inUse && entry.key == key) {
        entry.inUse = true; entry.lease.frame = frame; return entry.lease;
    }
    TransientResourceLease lease{};
    lease.frame = frame;
    lease.image = allocator_.createImage2D(key.width, key.height, key.format, key.usage);
    try { images_.push_back({key, lease, true}); }
    catch (...) { allocator_.destroyImage(lease.image); throw; }
    return lease;
}
TransientResourceLease TransientResourcePool::acquireBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible, std::uint64_t frame) {
    if (!size || !usage) throw std::invalid_argument("Invalid transient buffer description");
    for (auto& entry : buffers_)
        if (!entry.inUse && entry.size == size && entry.usage == usage && entry.hostVisible == hostVisible) {
            entry.inUse = true; entry.lease.frame = frame; return entry.lease;
        }
    TransientResourceLease lease{};
    lease.frame = frame;
    lease.buffer = allocator_.createBuffer(size, usage, hostVisible);
    try { buffers_.push_back({size, usage, hostVisible, lease, true}); }
    catch (...) { allocator_.destroyBuffer(lease.buffer); throw; }
    return lease;
}
void TransientResourcePool::retireFrame(std::uint64_t completedFrame) {
    for (auto& entry : images_) if (entry.lease.frame <= completedFrame) entry.inUse = false;
    for (auto& entry : buffers_) if (entry.lease.frame <= completedFrame) entry.inUse = false;
}
void TransientResourcePool::trim() {
    images_.erase(std::remove_if(images_.begin(), images_.end(), [&](auto& entry) {
        if (entry.inUse) return false;
        allocator_.destroyImage(entry.lease.image); return true;
    }), images_.end());
    buffers_.erase(std::remove_if(buffers_.begin(), buffers_.end(), [&](auto& entry) {
        if (entry.inUse) return false;
        allocator_.destroyBuffer(entry.lease.buffer); return true;
    }), buffers_.end());
}
void TransientResourcePool::clear() noexcept {
    for (auto& entry : images_) allocator_.destroyImage(entry.lease.image);
    for (auto& entry : buffers_) allocator_.destroyBuffer(entry.lease.buffer);
    images_.clear(); buffers_.clear();
}
} // namespace azurerender
