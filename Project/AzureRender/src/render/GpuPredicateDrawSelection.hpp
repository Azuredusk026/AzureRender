#pragma once
#include "rhi/Rhi.hpp"
#include <memory>
namespace azurerender {
// Selects exactly one command region using a GPU predicate. Callers publish
// source/predicate writes, retain their storage, and retire slots before reuse.
// Predicate zero and explicit permission choose fast; all other cases use full.
class GpuPredicateDrawSelection final {
public:
    static constexpr std::uint64_t kMaximumBytes=128ULL*1024*1024;
    struct Ticket {unsigned slot=0;std::uint64_t generation=0;std::uint32_t count=0;const void* owner=nullptr;};
    GpuPredicateDrawSelection(rhi::IRhi&,std::uint32_t capacity,unsigned slots,const std::vector<char>& shader);
    ~GpuPredicateDrawSelection();
    GpuPredicateDrawSelection(const GpuPredicateDrawSelection&)=delete;
    GpuPredicateDrawSelection& operator=(const GpuPredicateDrawSelection&)=delete;
    Ticket prepare(unsigned slot,const rhi::GpuBuffer& source,const rhi::GpuBuffer& predicate,
        std::uint32_t count,bool permitted=false);
    // Consumes the ticket once. Output is compute-readable. Indirect or copy
    // consumers publish their own dependency from the compute writes.
    void record(rhi::ICommandRecorder&,Ticket);
    const rhi::GpuBuffer& output(unsigned slot) const;
    // Full commands start at zero. The fast region begins at this byte offset.
    VkDeviceSize fastOffset() const noexcept;
    std::uint64_t bytes() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
