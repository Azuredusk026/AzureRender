#pragma once
#include "rhi/Rhi.hpp"
#include <string>

namespace azurerender {
// Owns a linear display-color target and a resolve pipeline. The caller owns
// submission retirement, the final framebuffer and its active render pass.
class SpatialAntiAliasing final {
public:
    static constexpr VkFormat kLinearFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
    SpatialAntiAliasing(rhi::IRhi&,VkExtent2D,const std::string& shaderDirectory,VkRenderPass outputPass);
    ~SpatialAntiAliasing();
    SpatialAntiAliasing(const SpatialAntiAliasing&)=delete;
    SpatialAntiAliasing& operator=(const SpatialAntiAliasing&)=delete;
    VkRenderPass inputPass() const{return pass_;}
    VkFramebuffer inputFramebuffer() const{return framebuffer_;}
    const rhi::GpuImage& inputImage() const{return image_;}
    void resolve(rhi::ICommandRecorder&,int mode,bool encodeSrgb,bool bypassEdge=false) const;
private:
    rhi::IRhi& backend_;
    rhi::GpuImage image_{};
    VkImageView view_=VK_NULL_HANDLE;
    VkSampler sampler_=VK_NULL_HANDLE;
    VkRenderPass pass_=VK_NULL_HANDLE;
    VkFramebuffer framebuffer_=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_=VK_NULL_HANDLE;
    VkDescriptorPool pool_=VK_NULL_HANDLE;
    VkDescriptorSet set_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
    void release() noexcept;
};
}
