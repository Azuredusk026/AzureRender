#pragma once
#include "render/ProjectedReflectionCamera.hpp"
#include "render/ProjectedReflectionPreparation.hpp"
#include "render/DepthPyramid.hpp"
#include "render/ReflectionSignalPyramid.hpp"
#include <memory>
namespace azurerender {
struct ProjectedScreenReflectionShaders {
    std::vector<char> prepare,ray,depth,filter;
    ReflectionPyramidShaders signal;
    static ProjectedScreenReflectionShaders load(const std::string& directory);
};
struct ProjectedScreenReflectionParameters {
    unsigned maxSteps=96,jitterSeed=0;
    float edgeFade=.08F,maxRoughness=.7F,strength=1;
    std::array<float,2> motionUvOffset{};
    bool requireTrackedMotion=false;
};
struct ProjectedScreenReflectionInputs {
    // Borrowed sampled RGBA32F snapshot and finite nonnegative HDR color.
    // Optional motion RG is current-minus-previous UV in normalized coordinates.
    VkImageView surface{},color{},motion{};VkExtent2D extent{};
};
struct ProjectedScreenReflectionHistory {
    // This slot must contain a successfully submitted depth from this owner.
    unsigned slot=0;VkImageView color{};ReflectionTraceView view;
};
// Combines public producers and footprint-driven filtering with packed signals.
// Perspective cameras use a finite far plane. The caller publishes borrowed inputs,
// owns submission/history identity and retires all slot consumers before reuse/destruction.
class ProjectedScreenReflectionGpu final {
public:
    ProjectedScreenReflectionGpu(rhi::IRhi&,VkExtent2D,unsigned frames,const ProjectedScreenReflectionShaders&,unsigned maximumLevels=7,unsigned resolutionDivisor=1);
    ~ProjectedScreenReflectionGpu();
    ProjectedScreenReflectionGpu(const ProjectedScreenReflectionGpu&)=delete;
    ProjectedScreenReflectionGpu& operator=(const ProjectedScreenReflectionGpu&)=delete;
    static bool supported(rhi::IRhi&,VkExtent2D,unsigned frames,unsigned maximumLevels=7,unsigned resolutionDivisor=1);
    unsigned levels() const noexcept{return levels_;}
    std::uint64_t imageByteBudget() const noexcept{return imageBytes_;}
    ReflectionFilterOutput output(unsigned slot) const{return filter_->output(slot);}
    ProjectedReflectionPrepared prepared(unsigned slot) const{return prepare_->output(slot);}
    ReflectionPyramidViews signal(unsigned slot) const{return signals_->base(slot);}
    void record(rhi::ICommandRecorder&,unsigned slot,const ProjectedScreenReflectionInputs&,const ReflectionTraceView&,
        const ProjectedScreenReflectionParameters&,const ProjectedScreenReflectionHistory* history=nullptr);
private:
    rhi::IRhi& backend_;VkExtent2D source_{},extent_{};unsigned frames_=0,levels_=0;std::uint64_t imageBytes_=0;
    std::unique_ptr<ProjectedReflectionPreparationGpu> prepare_;
    std::unique_ptr<ProjectedReflectionRayGpu> ray_;
    std::unique_ptr<DepthPyramidGpu> depth_;
    std::unique_ptr<ReflectionSignalPyramidGpu> signals_;
    std::unique_ptr<ReflectionSpatialFilterGpu> filter_;
    rhi::GpuImage neutral_{};VkImageView neutralView_{};
};
}
