#pragma once
#include "assets/OutlineProjectionData.hpp"
#include "render/SampledTextureCache.hpp"

namespace azurerender {
// Immutable vertex-stage data. The loading host serializes initialization;
// GPU uses must retire before reset. Sampled images belong to the shared cache.
class OutlineProjectionResources final {
public:
    explicit OutlineProjectionResources(rhi::IGpuAllocator& allocator,std::uint64_t maximumBytes=0)
        :allocator_(allocator),maximumBytes_(maximumBytes){}
    ~OutlineProjectionResources(){reset();}
    OutlineProjectionResources(const OutlineProjectionResources&)=delete;
    OutlineProjectionResources& operator=(const OutlineProjectionResources&)=delete;
    void initialize(const OutlineProjectionData& data,SampledTextureCache& cache);
    void reset() noexcept;
    const rhi::GpuBuffer& buffer() const noexcept{return buffer_;}
    const std::vector<SampledTextureBinding>& masks() const noexcept{return masks_;}
    std::uint32_t recordCount() const noexcept{return recordCount_;}
    std::uint32_t coordinateCount() const noexcept{return coordinateCount_;}
private:
    rhi::IGpuAllocator& allocator_;
    std::uint64_t maximumBytes_=0;
    rhi::GpuBuffer buffer_{};
    std::vector<SampledTextureBinding> masks_;
    std::uint32_t recordCount_=0,coordinateCount_=0;
};
} // namespace azurerender
