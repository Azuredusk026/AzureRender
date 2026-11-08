#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace azurerender {
struct SurfaceRadianceIdentity {
    std::array<std::uint32_t,4> words{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    bool valid() const noexcept{return words[2]!=UINT32_MAX;}
};
inline SurfaceRadianceIdentity surfaceRadianceIdentity(float depth,std::uint32_t instance,
    std::uint32_t firstIndex,std::uint32_t triangle,std::uint32_t material) noexcept {
    SurfaceRadianceIdentity result;
    if(!std::isfinite(depth)||firstIndex==UINT32_MAX||triangle>(UINT32_MAX-1-firstIndex)/3)return result;
    std::memcpy(&result.words[0],&depth,4);result.words[1]=instance;result.words[2]=firstIndex+triangle*3;result.words[3]=material;return result;
}
class SurfaceRadianceCache final {
public:
    static constexpr unsigned kAttachments=4;
    static constexpr std::uint64_t kMaximumBytes=512ULL*1024*1024;
    struct Texture {rhi::GpuImage image{};VkImageView view=VK_NULL_HANDLE;};
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned slots,std::uint64_t byteLimit=kMaximumBytes);
    SurfaceRadianceCache(rhi::IRhi&,VkExtent2D,unsigned slots,std::uint64_t byteLimit=kMaximumBytes);
    ~SurfaceRadianceCache();
    SurfaceRadianceCache(const SurfaceRadianceCache&)=delete;
    SurfaceRadianceCache& operator=(const SurfaceRadianceCache&)=delete;
    const Texture& texture(unsigned slot,unsigned attachment) const{return textures_.at(slot).at(attachment);}
    VkSampler sampler() const noexcept{return sampler_;}
    std::uint64_t bytes() const noexcept{return bytes_;}
    static VkFormat format(unsigned attachment){return attachment==3?VK_FORMAT_R32G32B32A32_UINT:VK_FORMAT_R32G32B32A32_SFLOAT;}
    static VkClearValue clearValue(unsigned attachment) noexcept {
        VkClearValue value{};if(attachment==3)for(auto& word:value.color.uint32)word=UINT32_MAX;return value;
    }
private:
    void clear() noexcept;
    rhi::IRhi& rhi_;std::vector<std::array<Texture,kAttachments>> textures_;
    VkSampler sampler_=VK_NULL_HANDLE;std::uint64_t bytes_=0;
};
}
