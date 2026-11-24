#pragma once
#include "render/RenderViewService.hpp"
#include "render/ColorGradingLutGpu.hpp"
#include "render/SpatialAntiAliasing.hpp"
namespace azurerender {
// Owned LDR resolve resources, separate from scene and temporal attachments.
class RenderViewOutput final {
public:
    RenderViewOutput(const RenderContext&,VkImageView hdr,VkSampler sampler,ViewColorTransfer,VkImageView displayMask=VK_NULL_HANDLE,std::array<VkImageView,2> displayHistory={});
    ~RenderViewOutput();
    void schedule(RenderGraph&,RenderGraph::ResourceId hdr,const std::string& name,bool temporalReady=false,RenderGraph::ResourceId displayMask=UINT32_MAX,unsigned displayHistorySelector=0);
    const rhi::GpuImage& image() const{return image_;}
    VkImageView view() const{return view_;}
private:
    RenderContext context_;
    ViewColorTransfer transfer_;
    bool displayMaskEnabled_=false;
    rhi::GpuImage image_{};
    VkImageView view_=VK_NULL_HANDLE;
    VkRenderPass pass_=VK_NULL_HANDLE;
    VkFramebuffer framebuffer_=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;
    VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkDescriptorSet set_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
    std::unique_ptr<ColorGradingLutGpu> lut_;
    std::unique_ptr<SpatialAntiAliasing> spatial_;
    void destroy() noexcept;
};
}
