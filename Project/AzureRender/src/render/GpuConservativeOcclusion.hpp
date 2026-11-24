#pragma once
#include "render/DepthPyramid.hpp"
#include <array>
#include <memory>
namespace azurerender {
struct alignas(16) GpuOcclusionBounds {
    std::array<float,16> clipFromLocal{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,4> minimum{},maximum{};
};
static_assert(sizeof(GpuOcclusionBounds)==96,"Occlusion bounds must match std430");
// Bounds enclose all shader deformation. Matrices match the vertex stage.
// Depth is a full-size, same-view maximum pyramid of finite forward [0,1]
// depth with clear value 1. The caller publishes source visibility and retires
// the slot before preparing, replacing or releasing resources.
class GpuConservativeOcclusion final {
public:
    static constexpr std::uint64_t kMaximumBytes=128ULL*1024*1024;
    struct Ticket {unsigned slot=0;std::uint64_t generation=0;std::uint32_t count=0;};
    GpuConservativeOcclusion(rhi::IRhi&,std::uint32_t capacity,unsigned slots,const std::vector<char>& shader);
    ~GpuConservativeOcclusion();
    GpuConservativeOcclusion(const GpuConservativeOcclusion&)=delete;
    GpuConservativeOcclusion& operator=(const GpuConservativeOcclusion&)=delete;
    Ticket prepare(unsigned slot,const rhi::GpuBuffer& source,const std::vector<GpuOcclusionBounds>&,
        const DepthPyramidOutput&,VkViewport,float depthBias=1e-5F,unsigned guardPixels=1);
    // Consumes a ticket once. Output is compute-readable; indirect and copy
    // consumers publish their own dependency. Multi-instance commands survive.
    void record(rhi::ICommandRecorder&,Ticket);
    const rhi::GpuBuffer& output(unsigned slot) const;
    // Call only after the recorded slot retires, before its next prepare.
    // Counts every zero-instance output, including initially empty commands.
    std::uint32_t retiredZeroCommandCount(unsigned slot) const;
    std::uint64_t bytes() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
