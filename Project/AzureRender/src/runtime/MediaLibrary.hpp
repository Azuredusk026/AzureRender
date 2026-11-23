#pragma once
#include "runtime/ProjectSaveStore.hpp"
#include "assets/AssetTransfer.hpp"

namespace azurerender {
struct MediaLibraryLimits {
    std::uint64_t maxSourceBytes = 128ULL * 1024 * 1024;
    std::uint64_t maxCacheBytes = 512ULL * 1024 * 1024;
    std::size_t maxEntries = 4096;
    std::size_t maxScanEntries = 16384;
};
struct MediaLibraryEntry {
    std::string id, title, format, blob, filename, fingerprint;
    std::uint64_t bytes = 0;
    bool available = false;
};
struct MediaLibraryImport {
    MediaLibraryEntry entry;
    bool reused = false;
};
// The owner keeps the store alive. Queries are read-only. Import may run on a
// worker; preparation precedes the short serialized metadata/publication phase.
class MediaLibrary final {
public:
    MediaLibrary(ProjectSaveStore& store, std::string slot, AssetDomain domain,
                 MediaLibraryLimits limits = {});
    [[nodiscard]] std::vector<MediaLibraryEntry> entries() const;
    [[nodiscard]] std::filesystem::path file(const std::string& id) const;
    MediaLibraryImport importFile(const std::filesystem::path& source,
                                 const AssetCancellation& cancelled = {}) const;
private:
    std::vector<MediaLibraryEntry> decode(const nlohmann::json& value) const;
    std::filesystem::path location(const MediaLibraryEntry& entry) const;
    ProjectSaveStore& store_;
    std::string slot_;
    AssetDomain domain_;
    MediaLibraryLimits limits_;
};
}
