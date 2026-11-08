#pragma once
#include "assets/GltfLoader.hpp"
#include "render/TransparentTopology.hpp"
#include "rhi/Rhi.hpp"
#include <memory>
namespace azurerender {
struct GpuTransparentSortLimits {
    std::uint32_t vertices=65536,triangles=65536,instances=2048,morphWeights=16384;
    unsigned slots=2;
};
struct GpuTransparentSortShaders {std::vector<char> deform,keys,scatter,sort;};
struct GpuTransparentSortGeometry {const LoadedAsset* asset=nullptr;const TransparentTopology* topology=nullptr;};
struct GpuTransparentSortInstance {
    std::uint32_t geometry=0;
    internal::Matrix4 model{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const AssetPose* pose=nullptr;
    std::uint32_t jointBase=0;
    std::array<float,2> morph{};
};
// Sources are copied at construction. Each prepare consumes CPU pose values;
// the caller retains the matching GPU joint data until that slot is retired.
// Outputs are published for compute reads; index/transfer consumers publish
// their own dependency. A ticket records once after its owning slot retires.
class GpuTransparentIndexSort final {
public:
    static constexpr std::uint64_t kMaximumBytes=128ULL*1024*1024;
    struct Ticket {unsigned slot=0;std::uint64_t generation=0;std::uint32_t vertexCount=0,indexCount=0;};
    GpuTransparentIndexSort(rhi::IRhi&,const std::vector<GpuTransparentSortGeometry>&,
        GpuTransparentSortLimits,const GpuTransparentSortShaders&);
    ~GpuTransparentIndexSort();
    GpuTransparentIndexSort(const GpuTransparentIndexSort&)=delete;
    GpuTransparentIndexSort& operator=(const GpuTransparentIndexSort&)=delete;
    bool matchesGeometry(unsigned,const LoadedAsset&,const TransparentTopology&) const;
    Ticket prepare(unsigned slot,const rhi::GpuBuffer& joints,const std::vector<GpuTransparentSortInstance>&,
        const internal::Vector3& cameraPosition,const internal::Vector3& cameraForward);
    void record(rhi::ICommandRecorder&,Ticket);
    const rhi::GpuBuffer& indices(unsigned slot) const;
    const rhi::GpuBuffer& positions(unsigned slot) const;
    const rhi::GpuBuffer& keys(unsigned slot) const;
    std::uint64_t bytes() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
