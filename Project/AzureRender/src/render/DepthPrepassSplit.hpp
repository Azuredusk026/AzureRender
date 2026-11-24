#pragma once
#include "rhi/Rhi.hpp"
namespace azurerender {
// Single-sample, depth-only preparation followed by color clearing and depth
// loading. Formats, references and source dependencies remain compatible.
// The caller owns matching framebuffer images and retires all GPU consumers.
class DepthPrepassSplit final {
public:
    struct Plan {rhi::RenderPassDesc depth,color;};
    static Plan plan(const rhi::RenderPassDesc&);
    DepthPrepassSplit(rhi::IRhi&,const rhi::RenderPassDesc&);
    ~DepthPrepassSplit();
    DepthPrepassSplit(const DepthPrepassSplit&)=delete;
    DepthPrepassSplit& operator=(const DepthPrepassSplit&)=delete;
    VkRenderPass depthPass() const noexcept{return depth_;}
    VkRenderPass colorPass() const noexcept{return color_;}
    // After depth sampling and before beginning the color pass; all bindings
    // are distinct, single-layer, mip-zero images in attachment-reference order.
    void beforeColor(rhi::ICommandRecorder&,VkImage depth,const std::vector<VkImage>& colors) const;
private:
    rhi::IRhi& rhi_;VkRenderPass depth_{},color_{};std::size_t colors_=0;
};
}
