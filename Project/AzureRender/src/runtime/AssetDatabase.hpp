#pragma once
#include "runtime/Project.hpp"
#include "assets/GeneratorRegistry.hpp"
#include <optional>
#include <cstdint>
#include <functional>
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
    std::optional<GenerationManifest> generation;
    std::optional<GenerationManifest> importGeneration;
};
class AssetDatabase {
public:
    explicit AssetDatabase(Project project) : project_(std::move(project)) {}
    struct RefreshStatistics { std::uint64_t sourceBytesRead=0, filesRead=0, filesReused=0; };
    const RefreshStatistics& refreshStatistics() const noexcept { return statistics_; }
    std::vector<std::string> refresh(bool verifyAll=false, std::function<void()> check={});
    std::string idForPath(const std::string& virtualPath) const;
    std::filesystem::path resolve(const std::string& id) const;
    std::filesystem::path resolveReference(const std::string& reference) const;
    std::filesystem::path cacheFile(const std::string& id) const;
    std::string readSource(const std::string& id) const;
    std::string generateAsset(const GeneratorRegistry& generators,const std::string& generator,const std::string& output,
        const nlohmann::json& parameters,const std::vector<std::string>& inputs,const std::vector<std::string>& dependencies,
        const std::string& license,std::function<void(const std::string&)> validate,std::function<void()> check={});
    const std::map<std::string, AssetRecord>& records() const noexcept { return records_; }
    void writePack(const std::filesystem::path& destination) const;
    static std::filesystem::path resolvePack(const std::filesystem::path& directory, const std::string& id);
private:
    Project project_;
    std::map<std::string, AssetRecord> records_;
    RefreshStatistics statistics_;
    struct SourceState {
        std::filesystem::file_time_type sourceTime, metadataTime;
        std::uintmax_t size=0;
        std::string id;
        nlohmann::json metadata, document;
        std::uint64_t contentHash=0, baseFingerprint=0;
    };
    std::map<std::filesystem::path,SourceState> sources_;
};
} // namespace azurerender
