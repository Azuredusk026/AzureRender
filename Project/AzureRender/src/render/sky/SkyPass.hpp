#pragma once
#include "render/RenderContext.hpp"
#include "render/RenderSettings.hpp"
#include "rhi/IGpuAllocator.hpp"
#include "rhi/Rhi.hpp"
namespace azurerender {
// Draws an infinite background inside a host-owned scene render pass.
class SkyPass {
public:
 void load(const RenderContext&);
 void resize(const RenderContext&);
 void update(const SceneFrameData&);
 void draw(rhi::ICommandRecorder&);
 void release();
private:
 struct Uniform{std::array<float,4> right{},up{},forward{},color{},parameters{};};
 rhi::IRhi* rhi_=nullptr;rhi::IGpuAllocator* allocator_=nullptr;
 VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;VkDescriptorPool pool_=VK_NULL_HANDLE;
 VkPipelineLayout layout_=VK_NULL_HANDLE;VkPipeline pipeline_=VK_NULL_HANDLE;
 rhi::GpuImage image_;VkImageView view_=VK_NULL_HANDLE;VkSampler sampler_=VK_NULL_HANDLE;
 std::vector<rhi::GpuBuffer> uniforms_;std::vector<VkDescriptorSet> sets_;
 unsigned frame_=0;float aspect_=1;SkySettings settings_;
};
}
