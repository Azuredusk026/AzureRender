#pragma once
#include "render/RenderViewService.hpp"
namespace azurerender {
// Owned LDR resolve resources, separate from scene and temporal attachments.
class RenderViewOutput final {
public:
    RenderViewOutput(const RenderContext&,VkImageView hdr,VkSampler sampler,ViewColorTransfer);
    ~RenderViewOutput();
    void schedule(RenderGraph&,RenderGraph::ResourceId hdr,const std::string& name);
    const rhi::GpuImage& image() const{return image_;}
    VkImageView view() const{return view_;}
private:
    RenderContext context_;
    ViewColorTransfer transfer_;
    rhi::GpuImage image_{};
    VkImageView view_=VK_NULL_HANDLE;
    VkRenderPass pass_=VK_NULL_HANDLE;
    VkFramebuffer framebuffer_=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;
    VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkDescriptorSet set_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
    void destroy() noexcept;
};
}
