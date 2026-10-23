#include "MotionVectorTarget.hpp"
#include "rhi/GpuAllocator.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace azurerender {
namespace {
bool depthFormatSupported(VkFormat format){
    return format==VK_FORMAT_D16_UNORM||format==VK_FORMAT_X8_D24_UNORM_PACK32||format==VK_FORMAT_D32_SFLOAT
        ||format==VK_FORMAT_D16_UNORM_S8_UINT||format==VK_FORMAT_D24_UNORM_S8_UINT||format==VK_FORMAT_D32_SFLOAT_S8_UINT;
}
}
MotionVectorTarget::MotionVectorTarget(rhi::IRhi& rhi,rhi::IGpuAllocator& allocator,VkExtent2D extent,
    VkFormat depthFormat,const std::vector<VkImageView>& depths):rhi_(rhi),allocator_(allocator){
    const auto pixels=std::uint64_t(extent.width)*extent.height;
    if(!extent.width||!extent.height||pixels>kMaximumBytes/kBytesPerPixel||!depthFormatSupported(depthFormat)
        ||depths.empty()||depths.size()>std::numeric_limits<std::uint32_t>::max()
        ||std::any_of(depths.begin(),depths.end(),[](VkImageView value){return value==VK_NULL_HANDLE;}))
        throw std::invalid_argument("Motion target requires bounded extent, a depth format and valid source depth views");
    if(!rhi_.supportsSampledImage(kFormat,false)||!rhi_.supportsSampledImage(kSurfaceDepthFormat,false))
        throw std::runtime_error("Motion and color-surface depth formats must be sampleable on this device");
    bytes_=pixels*kBytesPerPixel;framebuffers_.reserve(depths.size());
    try{
        image_=allocator_.createImage2D(extent.width,extent.height,kFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        view_=rhi_.createImageView(image_.image,kFormat,VK_IMAGE_ASPECT_COLOR_BIT,1);
        surfaceDepth_=allocator_.createImage2D(extent.width,extent.height,kSurfaceDepthFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        surfaceDepthView_=rhi_.createImageView(surfaceDepth_.image,kSurfaceDepthFormat,VK_IMAGE_ASPECT_COLOR_BIT,1);
        rhi::RenderPassDesc description;description.depthAttachment=2;description.depthReadOnly=true;description.externalReadDependency=true;
        description.attachments={{kFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
            {kSurfaceDepthFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
            {depthFormat,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,false,true,true}};
        description.attachments[2].load=true;pass_=rhi_.createRenderPass(description);
        for(auto depth:depths)framebuffers_.push_back(rhi_.createFramebuffer({pass_,{view_,surfaceDepthView_,depth},extent.width,extent.height}));
    }catch(...){reset();throw;}
}
MotionVectorTarget::~MotionVectorTarget(){reset();}
void MotionVectorTarget::reset() noexcept {
    for(auto framebuffer:framebuffers_)if(framebuffer)rhi_.destroyFramebuffer(framebuffer);
    framebuffers_.clear();
    if(pass_)rhi_.destroyRenderPass(pass_);pass_=VK_NULL_HANDLE;
    if(view_)rhi_.destroyImageView(view_);view_=VK_NULL_HANDLE;
    if(surfaceDepthView_)rhi_.destroyImageView(surfaceDepthView_);surfaceDepthView_=VK_NULL_HANDLE;
    if(image_.image)allocator_.destroyImage(image_);image_={};bytes_=0;
    if(surfaceDepth_.image)allocator_.destroyImage(surfaceDepth_);surfaceDepth_={};
}
}
