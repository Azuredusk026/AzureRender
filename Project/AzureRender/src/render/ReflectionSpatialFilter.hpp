#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <vector>
namespace azurerender {
enum class ReflectionColorEncoding { Linear,MaxComponent };
using ReflectionFilterFrame=std::array<std::array<float,4>,3>;
struct ReflectionFilterParameters {
    float maxLod=6,sharpLod=.25F,depthFalloff=.01F,minimumWeight=.01F;
    std::array<float,2> depthDecode{1,0};
    bool reciprocalDepth=false;
    ReflectionColorEncoding encoding=ReflectionColorEncoding::MaxComponent;
    ReflectionFilterFrame frame(unsigned width,unsigned height,unsigned depthLevels,unsigned colorLevels) const;
};
std::array<float,3> encodeReflectionRadiance(const std::array<float,3>& color);
std::array<float,3> decodeReflectionRadiance(const std::array<float,3>& color);
struct ReflectionFilterInputViews {
    VkImageView depth=VK_NULL_HANDLE,surface=VK_NULL_HANDLE,color=VK_NULL_HANDLE;
    unsigned width=0,height=0,depthLevels=0,colorLevels=0;
    VkImageView confidence=VK_NULL_HANDLE;
    unsigned confidenceLevels=0,roughnessChannel=0;
    bool quantizeConfidence=true;
};
struct ReflectionFilterOutput {
    VkImage image=VK_NULL_HANDLE;
    VkImageView view=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    VkExtent2D extent{};
};
// Inputs are borrowed in SHADER_READ_ONLY_OPTIMAL. Surface R/G holds roughness/confidence.
// Each slot is reusable after its consuming submission completes. The caller waits before destruction.
class ReflectionSpatialFilterGpu final {
public:
    ReflectionSpatialFilterGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,const std::vector<char>& shader);
    ~ReflectionSpatialFilterGpu();
    ReflectionSpatialFilterGpu(const ReflectionSpatialFilterGpu&)=delete;
    ReflectionSpatialFilterGpu& operator=(const ReflectionSpatialFilterGpu&)=delete;
    void record(rhi::ICommandRecorder& commands,unsigned slot,const ReflectionFilterInputViews& inputs,const ReflectionFilterParameters& parameters);
    ReflectionFilterOutput output(unsigned slot) const;
private:
    struct Slot {rhi::GpuImage image{};VkImageView view=VK_NULL_HANDLE;rhi::GpuBuffer parameters{};VkDescriptorSet set=VK_NULL_HANDLE;bool initialized=false;};
    void release() noexcept;
    rhi::IRhi& backend_;
    VkExtent2D extent_{};
    std::vector<Slot> slots_;
    VkSampler point_=VK_NULL_HANDLE,linear_=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;
    VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkShaderModule shader_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
};
}
