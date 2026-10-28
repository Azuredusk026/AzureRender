#pragma once
#include "render/DepthPyramid.hpp"
#include "render/ReflectionRay.hpp"
#include <memory>
namespace azurerender {
struct ScreenReflectionShaders {
    std::vector<char> depth,filter;
    ReflectionTraceShaders ray;
    ReflectionPyramidShaders signal;
    static ScreenReflectionShaders load(const std::string& directory);
};
// Combined resources have a conservative 512 MiB image budget, including float fallback.
// Source snapshots are borrowed. The caller retires all slot consumers before reuse/destruction.
class ScreenReflectionGpu final {
public:
    ScreenReflectionGpu(rhi::IRhi&,VkExtent2D,unsigned frames,const ScreenReflectionShaders&,unsigned maximumLevels=7,ReflectionPyramidPrecision=ReflectionPyramidPrecision::PreferPacked);
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned frames,unsigned maximumLevels=7,ReflectionPyramidPrecision=ReflectionPyramidPrecision::PreferPacked);
    unsigned levels() const noexcept{return levels_;}
    std::uint64_t imageByteBudget() const noexcept{return imageBytes_;}
    ReflectionFilterOutput output(unsigned slot) const{return filter_->output(slot);}
    void record(rhi::ICommandRecorder&,unsigned slot,const ReflectionTraceInputs&,const ReflectionTraceView&,const ScreenSpaceReflectionSettings&);
private:
    VkExtent2D extent_{};unsigned frames_=0,levels_=0;std::uint64_t imageBytes_=0;
    std::unique_ptr<DepthPyramidGpu> depth_;
    std::unique_ptr<ReflectionSignalPyramidGpu> signals_;
    std::unique_ptr<ReflectionRayGpu> rays_;
    std::unique_ptr<ReflectionSpatialFilterGpu> filter_;
};
}
