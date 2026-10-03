#pragma once
#include "runtime/Project.hpp"
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace azurerender {
struct AssetRecord {
    std::string id, virtualPath;
    std::filesystem::path path;
    std::uint64_t fingerprint = 0, contentHash = 0;
    std::vector<std::string> dependencies;
};
class AssetDatabase {
public:
    explicit AssetDatabase(Project project) : project_(std::move(project)) {}
    std::vector<std::string> refresh();
    std::string idForPath(const std::string& virtualPath) const;
    std::filesystem::path resolve(const std::string& id) const;
    std::filesystem::path resolveReference(const std::string& reference) const;
    std::filesystem::path cacheFile(const std::string& id) const;
    std::string readSource(const std::string& id) const;
    const std::map<std::string, AssetRecord>& records() const noexcept { return records_; }
    void writePack(const std::filesystem::path& destination) const;
    static std::filesystem::path resolvePack(const std::filesystem::path& directory, const std::string& id);
private:
    Project project_;
    std::map<std::string, AssetRecord> records_;
};
} // namespace azurerender
