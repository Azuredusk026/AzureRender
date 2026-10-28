#pragma once
#include "rhi/Rhi.hpp"
#include <vector>
namespace azurerender {
enum class DepthReduction {Minimum,Maximum};
struct DepthPyramidOutput {VkImage image{};VkImageView view{};VkSampler sampler{};VkExtent2D extent{};unsigned levels=0;};
// Source red values must be finite. Source ownership and sampled visibility are external.
// Slot reuse and destruction require completion of all consuming submissions.
class DepthPyramidGpu final {
public:
    DepthPyramidGpu(rhi::IRhi&,VkExtent2D,unsigned levels,unsigned frames,const std::vector<char>& shader);
    ~DepthPyramidGpu();
    DepthPyramidGpu(const DepthPyramidGpu&)=delete;
    DepthPyramidGpu& operator=(const DepthPyramidGpu&)=delete;
    // An explicit source extent selects a same-size copy or a floor-half base reduction.
    void record(rhi::ICommandRecorder&,unsigned slot,VkImageView source,DepthReduction,VkExtent2D sourceExtent={});
    DepthPyramidOutput output(unsigned slot) const;
private:
    struct Mip {VkImageView view{};VkDescriptorSet set{};rhi::GpuBuffer parameters{};};
    struct Slot {rhi::GpuImage image{};VkImageView all{};std::vector<Mip> mips;};
    void release() noexcept;
    rhi::IRhi& backend_;VkExtent2D extent_{};unsigned levels_=0;std::vector<Slot> slots_;
    VkSampler point_{};VkDescriptorSetLayout descriptors_{};VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};VkShaderModule shader_{};VkPipeline pipeline_{};
};
}
