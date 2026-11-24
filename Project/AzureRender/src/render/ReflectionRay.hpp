#pragma once
#include "render/ReflectionSignalPyramid.hpp"
#include "render/ScreenSpaceReflection.hpp"
namespace azurerender {
using ReflectionTraceFrame=std::array<std::array<float,4>,12>;
struct ReflectionTraceView {
    std::array<float,16> projection{},worldToClip{};
    std::array<float,3> position{},right{},up{},forward{};
    ReflectionTraceFrame pack(unsigned width,unsigned height,const ScreenSpaceReflectionSettings&,ReflectionColorEncoding) const;
};
struct ReflectionTraceInputs {VkImageView surface{},color{};unsigned width=0,height=0;};
struct ReflectionTraceShaders {std::vector<char> packed,wide;};
// Inputs are borrowed sampled views. Outputs are borrowed base-level GENERAL views.
// The signal owner prepares base writes, publishes the subsequent levels and retires outputs.
class ReflectionRayGpu final {
public:
    ReflectionRayGpu(rhi::IRhi&,VkExtent2D,unsigned frames,VkFormat color,VkFormat confidence,const ReflectionTraceShaders&);
    ~ReflectionRayGpu();
    ReflectionRayGpu(const ReflectionRayGpu&)=delete;
    ReflectionRayGpu& operator=(const ReflectionRayGpu&)=delete;
    void record(rhi::ICommandRecorder&,unsigned slot,const ReflectionTraceInputs&,const ReflectionPyramidViews& base,
        const ReflectionTraceView&,const ScreenSpaceReflectionSettings&,ReflectionColorEncoding=ReflectionColorEncoding::MaxComponent);
private:
    void release() noexcept;
    struct Slot {VkDescriptorSet set{};rhi::GpuBuffer parameters{};};
    rhi::IRhi& backend_;VkExtent2D extent_{};VkFormat color_{},confidence_{};
    std::vector<Slot> slots_;VkSampler point_{};VkDescriptorSetLayout descriptors_{};VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};VkShaderModule shader_{};VkPipeline pipeline_{};
};
}
