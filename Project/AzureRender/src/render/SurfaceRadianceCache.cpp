#include "SurfaceRadianceCache.hpp"
#include <stdexcept>
namespace azurerender {
namespace {constexpr VkImageUsageFlags usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;}
bool SurfaceRadianceCache::supported(rhi::IRhi& backend,VkExtent2D extent,unsigned slots,std::uint64_t byteLimit){
    if(!extent.width||!extent.height||!slots||slots>8)return false;
    const auto pixels=std::uint64_t(extent.width)*extent.height;
    if(pixels>byteLimit/(64ULL*slots))return false;
    for(auto format:{VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32G32B32A32_UINT})
        if(!backend.supportsSampledImage(format,false)||!(backend.imageSampleCounts(format,usage)&VK_SAMPLE_COUNT_1_BIT))return false;
    return true;
}
SurfaceRadianceCache::SurfaceRadianceCache(rhi::IRhi& backend,VkExtent2D extent,unsigned slots,std::uint64_t byteLimit):rhi_(backend){
    if(!supported(backend,extent,slots,byteLimit))throw std::invalid_argument("Surface radiance cache exceeds format or byte capabilities");
    try{
        textures_.resize(slots);
        for(auto& frame:textures_)for(unsigned kind=0;kind<kAttachments;++kind){auto& t=frame[kind];
            t.image=backend.allocator().createImage2D(extent.width,extent.height,format(kind),usage);
            t.view=backend.createImageView(t.image.image,format(kind),VK_IMAGE_ASPECT_COLOR_BIT,1);
        }
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler_=backend.createSampler(sampler);
        bytes_=std::uint64_t(extent.width)*extent.height*64*slots;
    }catch(...){clear();throw;}
}
SurfaceRadianceCache::~SurfaceRadianceCache(){clear();}
void SurfaceRadianceCache::clear() noexcept {
    if(sampler_)rhi_.destroySampler(sampler_);sampler_=VK_NULL_HANDLE;
    for(auto& frame:textures_)for(auto& texture:frame){if(texture.view)rhi_.destroyImageView(texture.view);rhi_.allocator().destroyImage(texture.image);texture={};}
    textures_.clear();bytes_=0;
}
}
