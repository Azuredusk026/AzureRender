#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace azurerender::rhi {
using PipelineCacheBytes=std::vector<std::uint8_t>;
struct PipelineCacheIdentity {
    std::uint32_t vendor=0,device=0,driver=0;
    std::array<std::uint8_t,16> uuid{};
};
struct PipelineCacheLimits {
    std::uint64_t maximumFileBytes=128ULL*1024*1024;
    std::uint64_t maximumPoolBytes=256ULL*1024*1024;
    std::size_t maximumEntries=8;
    std::int64_t retentionSeconds=7*24*60*60;
};
std::optional<PipelineCacheBytes> encodePipelineCache(const PipelineCacheIdentity&,
    const PipelineCacheBytes&,std::uint64_t maximumBytes);
std::optional<PipelineCacheBytes> decodePipelineCache(const PipelineCacheIdentity&,
    const PipelineCacheBytes&,std::uint64_t maximumBytes);

// Owns only the vk1-*.azpc files in this dedicated directory. The payload is
// optional driver data; failure leaves pipeline creation on the empty-cache path.
class PipelineCacheStore final {
public:
    PipelineCacheStore(std::filesystem::path directory,PipelineCacheIdentity identity,
        PipelineCacheLimits limits={});
    std::filesystem::path file() const;
    PipelineCacheBytes load() const noexcept;
    bool save(const PipelineCacheBytes&) const noexcept;
private:
    std::filesystem::path directory_;
    PipelineCacheIdentity identity_;
    PipelineCacheLimits limits_;
};
} // namespace azurerender::rhi
