#pragma once

#include "assets/TextureMipChain.hpp"
#include "rhi/Rhi.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace azurerender {

// Borrowed descriptor values. The cache owns the native objects until clear().
struct SampledTextureBinding {
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

struct SampledTextureUpload {
    std::uint32_t width = 0, height = 0;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    // Zero generates semantic CPU mips. Positive counts preserve authored data
    // or use the existing RHI mip path when only a base level is supplied.
    std::uint32_t mipLevels = 0;
    TextureFilterSemantic semantic = TextureFilterSemantic::Linear;
    float alphaCutoff = 0;
    rhi::SamplerDesc sampler = [] {
        rhi::SamplerDesc value;
        value.maxLod = VK_LOD_CLAMP_NONE;
        return value;
    }();
};

struct SampledTextureCacheStatistics {
    std::uint64_t requests = 0, hits = 0, uniqueTextures = 0;
    std::uint64_t requestedBytes = 0, residentBytes = 0, retainedPixelBytes = 0;
    std::uint64_t hashedPixelBytes = 0, copiedPixelBytes = 0, transferredPixelBytes = 0, discardedPixelBytes = 0;
    double hashMs = 0, matchMs = 0, mipMs = 0, uploadMs = 0;
};

// Immutable RGBA8 material resources, scoped to one renderer and device.
// Calls are serialized by the loading host. All GPU uses must retire before
// clear() or destruction; bindings cannot outlive this owner.
class SampledTextureCache final {
public:
    SampledTextureCache(rhi::IGpuAllocator& allocator, rhi::IRhi& rhi);
    ~SampledTextureCache();
    SampledTextureCache(const SampledTextureCache&) = delete;
    SampledTextureCache& operator=(const SampledTextureCache&) = delete;

    SampledTextureBinding acquire(const std::vector<std::uint8_t>& pixels,
                                  const SampledTextureUpload& upload);
    // A successful call consumes the input allocation. Failure preserves it.
    // Misses transfer the identity bytes; hits release redundant storage.
    SampledTextureBinding acquire(std::vector<std::uint8_t>&& pixels,
                                  const SampledTextureUpload& upload);
    void clear() noexcept;
    const SampledTextureCacheStatistics& statistics() const noexcept { return statistics_; }

private:
    SampledTextureBinding acquireImpl(const std::vector<std::uint8_t>& pixels,
                                     const SampledTextureUpload& upload,
                                     std::vector<std::uint8_t>* consumed);
    struct Entry;
    rhi::IGpuAllocator& allocator_;
    rhi::IRhi& rhi_;
    std::unordered_multimap<std::uint32_t, std::unique_ptr<Entry>> entries_;
    SampledTextureCacheStatistics statistics_;
};

} // namespace azurerender
