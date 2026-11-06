#pragma once

#include "resources/ResourceLocator.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace azurerender {

// Owned by one loading session and called on its owner thread. Successful paths
// remain valid for that session until clear(). A batch failure preserves the
// last confirmed active table; successful batches retain only requested paths.
class PreparedResourcePaths final {
public:
    explicit PreparedResourcePaths(ResourceLocator locator)
        : locator_(std::move(locator)) {}

    [[nodiscard]] std::vector<std::string> resolve(
        const std::vector<std::string>& requested) {
        std::unordered_map<std::string, std::string> next;
        next.reserve(requested.size());
        std::vector<std::string> result;
        result.reserve(requested.size());
        for (const auto& path : requested) {
            auto found = next.find(path);
            if (found == next.end()) {
                const auto previous = active_.find(path);
                auto resolved = previous == active_.end()
                    ? locator_.resolveAsset(std::filesystem::u8path(path)).u8string()
                    : previous->second;
                found = next.emplace(path, std::move(resolved)).first;
            }
            result.push_back(found->second);
        }
        active_.swap(next);
        return result;
    }

    void clear() noexcept { active_.clear(); }

private:
    ResourceLocator locator_;
    std::unordered_map<std::string, std::string> active_;
};

} // namespace azurerender
