#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace azurerender {

struct TransientResourceKey {
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t format = 0;
    bool operator==(const TransientResourceKey& other) const {
        return name == other.name && width == other.width
            && height == other.height && format == other.format;
    }
};

struct TransientResourceLease {
    std::uint32_t id = 0;
    std::uint64_t frame = 0;
};

class TransientResourcePool final {
public:
    TransientResourceLease acquire(const TransientResourceKey& key, std::uint64_t frame);
    void retireFrame(std::uint64_t frame);
    void clear();
    [[nodiscard]] std::size_t liveCount() const noexcept { return entries_.size(); }

private:
    struct Entry {
        TransientResourceKey key;
        std::uint32_t id = 0;
        std::uint64_t lastFrame = 0;
        bool inUse = false;
    };
    std::vector<Entry> entries_;
    std::uint32_t nextId_ = 1;
};

}  // namespace azurerender
