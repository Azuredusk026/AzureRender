#pragma once
#include "render/ReflectionSignalPyramid.hpp"
namespace azurerender {
using ReflectionTemporalFrame=std::array<std::array<float,4>,2>;
struct ReflectionTemporalParameters {
    float varianceScale=1.25F,historyWeight=.97F,minimumColor=.001F;
    bool historyValid=false,rejectOutside=true;
    ReflectionTemporalFrame frame(unsigned width,unsigned height) const;
};
struct ReflectionTemporalInputs {
    VkImageView color{},confidence{},previousColor{},previousConfidence{},motion{},weight{};
    unsigned width=0,height=0;
};
struct ReflectionTemporalShaders {std::vector<char> packed,wide;};
// All images are borrowed. Sources are sampled; outputs are distinct GENERAL base views.
// Weight contains the caller's per-pixel history acceptance; motion is current-minus-previous UV.
// The caller retires slot consumers and owns history exchange and submission validity.
class ReflectionTemporalGpu final {
public:
    ReflectionTemporalGpu(rhi::IRhi&,VkExtent2D,unsigned frames,VkFormat color,VkFormat confidence,const ReflectionTemporalShaders&);
    ~ReflectionTemporalGpu();
    ReflectionTemporalGpu(const ReflectionTemporalGpu&)=delete;
    ReflectionTemporalGpu& operator=(const ReflectionTemporalGpu&)=delete;
    void record(rhi::ICommandRecorder&,unsigned slot,const ReflectionTemporalInputs&,const ReflectionPyramidViews&,const ReflectionTemporalParameters&);
private:
    struct Slot {VkDescriptorSet set{};rhi::GpuBuffer parameters{};};
    void release() noexcept;
    rhi::IRhi& backend_;VkExtent2D extent_{};VkFormat color_{},confidence_{};
    std::vector<Slot> slots_;VkSampler point_{};VkDescriptorSetLayout descriptors_{};VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};VkShaderModule shader_{};VkPipeline pipeline_{};
};
}
