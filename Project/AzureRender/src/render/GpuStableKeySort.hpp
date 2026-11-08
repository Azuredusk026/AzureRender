#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace azurerender {
// Ascending uint32 keys with stable input order for equal keys. Each record is
// {key, payload}. Segment counts partition the input, including empty segments.
// The caller publishes input reads and retires each slot before record/release.
// Nonempty output is owned by this sorter and published for compute reads.
class GpuStableKeySort final {
public:
    static constexpr std::uint32_t kMaximumRecords=1048576;
    static constexpr std::uint32_t kMaximumSegments=65536;
    static constexpr std::uint64_t kMaximumBytes=64ULL*1024*1024;
    struct Output {const rhi::GpuBuffer* buffer=nullptr;std::uint32_t count=0;};
    static bool supported(std::uint32_t capacity,unsigned slots,std::uint64_t limit=kMaximumBytes) noexcept;
    GpuStableKeySort(rhi::IRhi&,std::uint32_t capacity,unsigned slots,const std::vector<char>& shader);
    ~GpuStableKeySort();
    GpuStableKeySort(const GpuStableKeySort&)=delete;
    GpuStableKeySort& operator=(const GpuStableKeySort&)=delete;
    Output record(rhi::ICommandRecorder&,unsigned slot,const rhi::GpuBuffer& input,
                  const std::vector<std::uint32_t>& segmentCounts);
    std::uint64_t bytes() const noexcept{return bytes_;}
private:
    struct Frame {
        std::array<rhi::GpuBuffer,2> scratch{};
        rhi::GpuBuffer ranges{};
        std::array<VkDescriptorSet,3> sets{};
        std::vector<std::uint32_t> segments;
    };
    void release() noexcept;
    rhi::IRhi& rhi_;
    std::uint32_t capacity_=0;
    std::uint64_t bytes_=0;
    std::vector<Frame> frames_;
    VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;
    VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
};
}
