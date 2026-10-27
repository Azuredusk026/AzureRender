#pragma once
#include "render/FogVolumeData.hpp"
#include "rhi/Rhi.hpp"
#include <cstring>

namespace azurerender {
class FogVolumeGpu final {
public:
    // The backend outlives this immutable owner. Retire in-flight use first.
    FogVolumeGpu(rhi::IRhi& backend,const FogVolumeData& source):backend_(backend) {
        source.validate();
        constexpr auto format=VK_FORMAT_R16G16B16A16_SFLOAT;
        if(!backend_.supportsSampledImage(format,true))throw std::runtime_error("Device does not support linear fog volume sampling");
        rhi::GpuBuffer staging;
        try {
            const auto bytes=VkDeviceSize(source.rgba16f.size())*2;
            staging=backend_.allocator().createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
            std::memcpy(staging.mapped,source.rgba16f.data(),std::size_t(bytes));backend_.allocator().flush(staging,0,bytes);
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_3D;
            info.extent={source.dimensions[0],source.dimensions[1],source.dimensions[2]};info.mipLevels=info.arrayLayers=1;
            info.format=format;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;info.samples=VK_SAMPLE_COUNT_1_BIT;
            image_=backend_.allocator().createImage(info,false);
            backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
            backend_.copyBufferToImage(staging,image_,info.extent.width,info.extent.height);
            backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
            backend_.allocator().destroyBuffer(staging);
            view_=backend_.createImageView(image_.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1,0,VK_IMAGE_VIEW_TYPE_3D);
            rhi::SamplerDesc sampler;sampler.addressU=sampler.addressV=sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler.mipmapLinear=false;sampler_=backend_.createSampler(sampler);
        }catch(...){backend_.allocator().destroyBuffer(staging);release();throw;}
    }
    ~FogVolumeGpu(){release();}
    FogVolumeGpu(const FogVolumeGpu&)=delete;
    FogVolumeGpu& operator=(const FogVolumeGpu&)=delete;
    void writeTexture(VkDescriptorSet set,std::uint32_t binding) const {
        rhi::DescriptorImageWrite write;write.set=set;write.binding=binding;write.view=view_;write.sampler=sampler_;
        backend_.writeDescriptorImage(write);
    }
private:
    void release() noexcept {
        if(view_)backend_.destroyImageView(view_);view_=VK_NULL_HANDLE;
        if(sampler_)backend_.destroySampler(sampler_);sampler_=VK_NULL_HANDLE;
        backend_.allocator().destroyImage(image_);
    }
    rhi::IRhi& backend_;
    rhi::GpuImage image_{};
    VkImageView view_=VK_NULL_HANDLE;
    VkSampler sampler_=VK_NULL_HANDLE;
};
}
