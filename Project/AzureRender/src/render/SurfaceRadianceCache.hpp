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
    struct Statistics {
        std::uint64_t producerStores=0,consumerVisits=0,eligibleRequests=0,pixelMatches=0,quadMatches=0,frames=0;
        bool overflow=false;
    };
    struct SelectionStatistics {std::uint64_t frames=0,fastFrames=0,fullFrames=0;bool lastFallback=true;};
    static constexpr unsigned kAttachments=4;
    static constexpr std::uint64_t kMaximumBytes=512ULL*1024*1024;
    struct Texture {rhi::GpuImage image{};VkImageView view=VK_NULL_HANDLE;};
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned slots,std::uint64_t byteLimit=kMaximumBytes);
    SurfaceRadianceCache(rhi::IRhi&,VkExtent2D,unsigned slots,std::uint64_t byteLimit=kMaximumBytes,bool diagnostics=false,bool selection=false);
    ~SurfaceRadianceCache();
    SurfaceRadianceCache(const SurfaceRadianceCache&)=delete;
    SurfaceRadianceCache& operator=(const SurfaceRadianceCache&)=delete;
    const Texture& texture(unsigned slot,unsigned attachment) const{return textures_.at(slot).at(attachment);}
    VkSampler sampler() const noexcept{return sampler_;}
    std::uint64_t bytes() const noexcept{return bytes_;}
    bool statisticsEnabled() const noexcept{return !statisticsFrames_.empty();}
    std::uint64_t statisticsBytes() const noexcept{return statisticsFrames_.size()*24ULL;}
    const rhi::GpuBuffer& statisticsBuffer(unsigned slot) const{return statisticsFrames_.at(slot).buffer;}
    // Reuse only after the owner's existing frame-slot completion fence.
    void beginStatisticsFrame(unsigned slot);
    void recordStatisticsReady(rhi::ICommandRecorder&,unsigned slot);
    Statistics statistics() const noexcept{return statistics_;}
    bool selectionEnabled() const noexcept{return !selectionFrames_.empty();}
    std::uint64_t selectionBytes() const noexcept{return selectionFrames_.size()*4ULL;}
    const rhi::GpuBuffer& selectionBuffer(unsigned slot)const{return selectionFrames_.at(slot).buffer;}
    void beginSelectionFrame(unsigned slot,bool permitted=false);
    void recordSelectionReady(rhi::ICommandRecorder&,unsigned slot);
    SelectionStatistics selectionStatistics()const noexcept{return selectionStatistics_;}

    static VkFormat format(unsigned attachment){return attachment==3?VK_FORMAT_R32G32B32A32_UINT:VK_FORMAT_R32G32B32A32_SFLOAT;}
    static VkClearValue clearValue(unsigned attachment) noexcept {
        VkClearValue value{};if(attachment==3)for(auto& word:value.color.uint32)word=UINT32_MAX;return value;
    }
private:
    void clear() noexcept;
    rhi::IRhi& rhi_;std::vector<std::array<Texture,kAttachments>> textures_;
    VkSampler sampler_=VK_NULL_HANDLE;std::uint64_t bytes_=0;
    struct StatisticsFrame {rhi::GpuBuffer buffer{};bool recorded=false;};
    std::vector<StatisticsFrame> statisticsFrames_;
    Statistics statistics_{};
    std::vector<StatisticsFrame> selectionFrames_;
    SelectionStatistics selectionStatistics_{};
};
}
