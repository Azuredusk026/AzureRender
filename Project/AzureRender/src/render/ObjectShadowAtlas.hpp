#pragma once
#include "render/ObjectShadow.hpp"
#include "rhi/Rhi.hpp"

namespace azurerender {
// The scene renderer retires its frame fences and pipelines before this owner.
// Filtering is borrowed through the shared depth sampler descriptor.
class ObjectShadowAtlas final {
    rhi::IRhi& backend_;
    rhi::GpuImage image_{};
    VkImageView view_{};
    VkRenderPass pass_{};
    VkFramebuffer framebuffer_{};
public:
    static constexpr unsigned kWidth=kObjectShadowColumns*kObjectShadowTileSize;
    static constexpr unsigned kHeight=kObjectShadowRows*kObjectShadowTileSize;
    static constexpr VkDeviceSize kDepthBytes=VkDeviceSize(kWidth)*kHeight*2;
    explicit ObjectShadowAtlas(rhi::IRhi& backend):backend_(backend){}
    ObjectShadowAtlas(const ObjectShadowAtlas&)=delete;
    ~ObjectShadowAtlas(){releaseImage();if(pass_)backend_.destroyRenderPass(pass_);}
    void initialize(){
        if(pass_)throw std::logic_error("Object shadow pass is already initialized");
        rhi::RenderPassDesc desc;
        desc.attachments={{VK_FORMAT_D16_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true}};
        desc.depthAttachment=0;desc.externalReadDependency=true;
        pass_=backend_.createRenderPass(desc);
    }
    void ensureImage(unsigned maximumDimension){
        if(!pass_)throw std::logic_error("Object shadow pass is not initialized");
        if(image_.image)return;
        if(maximumDimension&&(maximumDimension<kWidth||maximumDimension<kHeight))
            throw std::runtime_error("Object shadow atlas exceeds image dimension capacity");
        if(!backend_.supportsSampledImage(VK_FORMAT_D16_UNORM,true))
            throw std::runtime_error("Object shadow atlas requires sampled D16 with shared linear filtering");
        try{
            image_=backend_.allocator().createImage2D(kWidth,kHeight,VK_FORMAT_D16_UNORM,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
            view_=backend_.createImageView(image_.image,VK_FORMAT_D16_UNORM,VK_IMAGE_ASPECT_DEPTH_BIT,1);
            framebuffer_=backend_.createFramebuffer({pass_,{view_},kWidth,kHeight});
        }catch(...){releaseImage();throw;}
    }
    const rhi::GpuImage& image()const{return image_;}
    VkImageView view()const{return view_;}
    VkRenderPass renderPass()const{return pass_;}
    rhi::RenderPassBeginDesc beginDescription()const{
        if(!framebuffer_)throw std::logic_error("Object shadow atlas has no image");
        rhi::RenderPassBeginDesc result;result.renderPass=pass_;result.framebuffer=framebuffer_;result.extent={kWidth,kHeight};
        VkClearValue clear{};clear.depthStencil={1,0};result.clearValues={clear};return result;
    }
private:
    void releaseImage(){
        if(framebuffer_)backend_.destroyFramebuffer(framebuffer_);
        if(view_)backend_.destroyImageView(view_);
        backend_.allocator().destroyImage(image_);framebuffer_={};view_={};
    }
};
}
