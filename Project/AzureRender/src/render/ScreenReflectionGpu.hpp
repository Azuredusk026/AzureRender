#pragma once
#include "render/DepthPyramid.hpp"
#include "render/ReflectionRay.hpp"
#include "render/ReflectionTemporal.hpp"
#include <memory>
namespace azurerender {
struct ScreenReflectionShaders {
    std::vector<char> depth,filter;
    ReflectionTraceShaders ray;
    ReflectionPyramidShaders signal;
    ReflectionTemporalShaders temporal;
    static ScreenReflectionShaders load(const std::string& directory);
};
struct ScreenReflectionHistoryInputs {
    // Sampled views use the trace output's colour encoding and formats.
    // Motion is current-minus-previous UV; acceptance already includes disocclusion.
    VkImageView previousColor{},previousConfidence{},motion{},acceptance{};
    ReflectionTemporalParameters parameters;
};
// Combined resources have a conservative 512 MiB image budget, including float fallback.
// Source snapshots are borrowed. The caller retires all slot consumers before reuse/destruction.
class ScreenReflectionGpu final {
public:
    ScreenReflectionGpu(rhi::IRhi&,VkExtent2D,unsigned frames,const ScreenReflectionShaders&,unsigned maximumLevels=7,ReflectionPyramidPrecision=ReflectionPyramidPrecision::PreferPacked,unsigned resolutionDivisor=1,bool temporal=false);
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned frames,unsigned maximumLevels=7,ReflectionPyramidPrecision=ReflectionPyramidPrecision::PreferPacked,unsigned resolutionDivisor=1,bool temporal=false);
    unsigned levels() const noexcept{return levels_;}
    std::uint64_t imageByteBudget() const noexcept{return imageBytes_;}
    ReflectionFilterOutput output(unsigned slot) const{return filter_->output(slot);}
    // Sampleable after record; submission validity and slot retirement belong to the caller.
    ReflectionPyramidViews history(unsigned slot) const{return signals_->base(slot);}
    void record(rhi::ICommandRecorder&,unsigned slot,const ReflectionTraceInputs&,const ReflectionTraceView&,const ScreenSpaceReflectionSettings&,const ScreenReflectionHistoryInputs* history=nullptr);
private:
    VkExtent2D extent_{},sourceExtent_{};unsigned frames_=0,levels_=0;std::uint64_t imageBytes_=0;
    std::unique_ptr<DepthPyramidGpu> depth_;
    std::unique_ptr<ReflectionSignalPyramidGpu> signals_;
    std::unique_ptr<ReflectionRayGpu> rays_;
    std::unique_ptr<ReflectionSpatialFilterGpu> filter_;
    std::unique_ptr<ReflectionSignalPyramidGpu> current_;
    std::unique_ptr<ReflectionTemporalGpu> temporal_;
};
}
