#include "render/TransientResourcePool.hpp"

namespace azurerender {

TransientResourceLease TransientResourcePool::acquire(
    const TransientResourceKey& key,
    const std::uint64_t frame) {
    for (Entry& entry : entries_) {
        if (!entry.inUse && entry.key == key) {
            entry.inUse = true;
            entry.lastFrame = frame;
            return {entry.id, frame};
        }
    }
    entries_.push_back({key, nextId_++, frame, true});
    return {entries_.back().id, frame};
}

void TransientResourcePool::retireFrame(const std::uint64_t frame) {
    for (Entry& entry : entries_)
        if (entry.inUse && entry.lastFrame <= frame) entry.inUse = false;
}

void TransientResourcePool::clear() { entries_.clear(); }

}  // namespace azurerender
