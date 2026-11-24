#pragma once
#include "rhi/Rhi.hpp"
#include <memory>
namespace azurerender {
struct alignas(8) ShadowHistoryEntry {std::uint32_t key=UINT32_MAX,visibilityBits=UINT32_MAX;};
static_assert(sizeof(ShadowHistoryEntry)==8);
// The caller retires frame slots before reuse and completes GPU work before
// destruction. Visibility eligibility and history selection belong to the view.
class ShadowHistoryCache final {
public:
    static constexpr std::uint64_t kMaximumBytes=128ULL*1024*1024;
    struct Ticket {unsigned slot=0,workflow=0;std::uint64_t generation=0;const void* owner=nullptr;};
    static bool fits(VkExtent2D,unsigned slots,unsigned workflows,std::uint64_t byteLimit=kMaximumBytes) noexcept;
    ShadowHistoryCache(rhi::IRhi&,VkExtent2D,unsigned slots,unsigned workflows,const std::vector<char>& clearShader,std::uint64_t byteLimit=kMaximumBytes);
    ~ShadowHistoryCache();
    ShadowHistoryCache(const ShadowHistoryCache&)=delete;
    ShadowHistoryCache& operator=(const ShadowHistoryCache&)=delete;
    Ticket prepareClear(unsigned slot,unsigned workflow);
    // Consumes the ticket once and publishes clear writes to fragment readers
    // and writers. A view publishes its previous fragment writes before reuse.
    void recordClear(rhi::ICommandRecorder&,Ticket);
    const rhi::GpuBuffer& buffer(unsigned slot,unsigned workflow) const;
    std::uint64_t bytes() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
