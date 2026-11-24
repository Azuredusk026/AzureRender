#pragma once
#include "render/IrradianceClipmap.hpp"
#include "rhi/Rhi.hpp"

namespace azurerender {
// Immutable GPU owner. The backend outlives it; callers retire in-flight use
// before destruction.
class IrradianceClipmapGpu final {
public:
    static constexpr unsigned maxTextureDimension=256;
    IrradianceClipmapGpu(rhi::IRhi& backend,const IrradianceClipmap& source):backend_(backend){
        source.validate();
        for(unsigned a=0;a<3;++a)if(source.dimensions[a]>maxTextureDimension/(a==1?3U:1U))
            throw std::invalid_argument("Clipmap exceeds the bounded volume texture dimensions");
        const VkFormat formats[2]={VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_FORMAT_R8G8B8A8_UNORM};
        for(auto format:formats)if(!backend_.supportsSampledImage(format,true))
            throw std::runtime_error("Device does not support linear clipmap texture sampling");
        rhi::GpuBuffer staging;
        try{
            for(unsigned i=0;i<2;++i){rhi::SamplerDesc desc;
                desc.addressU=desc.addressV=desc.addressW=i?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:VK_SAMPLER_ADDRESS_MODE_REPEAT;
                desc.mipmapLinear=false;samplers_[i]=backend_.createSampler(desc);}
            for(unsigned i=0;i<6;++i){
                const auto layer=i/2,kind=i%2;
                const auto bytes=VkDeviceSize(source.amplitude[layer].size())*(kind?12U:4U);
                const void* data=kind?static_cast<const void*>(source.directional[layer].data()):static_cast<const void*>(source.amplitude[layer].data());
                staging=backend_.allocator().createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
                std::memcpy(staging.mapped,data,std::size_t(bytes));backend_.allocator().flush(staging,0,bytes);
                VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_3D;
                info.extent={source.dimensions[0],source.dimensions[1]*(kind?3U:1U),source.dimensions[2]};
                info.mipLevels=info.arrayLayers=1;info.format=formats[kind];info.tiling=VK_IMAGE_TILING_OPTIMAL;
                info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;info.samples=VK_SAMPLE_COUNT_1_BIT;
                images_[i]=backend_.allocator().createImage(info,false);
                backend_.transitionImageLayout(images_[i],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
                backend_.copyBufferToImage(staging,images_[i],info.extent.width,info.extent.height);
                backend_.transitionImageLayout(images_[i],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
                backend_.allocator().destroyBuffer(staging);
                views_[i]=backend_.createImageView(images_[i].image,info.format,VK_IMAGE_ASPECT_COLOR_BIT,1,0,VK_IMAGE_VIEW_TYPE_3D);
            }
            const auto rows=source.shaderParameters();
            parameters_=backend_.allocator().createBuffer(sizeof(rows),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            std::memcpy(parameters_.mapped,rows.data(),sizeof(rows));backend_.allocator().flush(parameters_,0,parameters_.size);
        }catch(...){backend_.allocator().destroyBuffer(staging);release();throw;}
    }
    ~IrradianceClipmapGpu(){release();}
    IrradianceClipmapGpu(const IrradianceClipmapGpu&)=delete;
    IrradianceClipmapGpu& operator=(const IrradianceClipmapGpu&)=delete;
    [[nodiscard]] const rhi::GpuBuffer& parameters() const noexcept{return parameters_;}
    void writeTextures(VkDescriptorSet set,std::uint32_t binding) const {
        rhi::DescriptorImageArrayWrite write;write.set=set;write.binding=binding;
        for(unsigned i=0;i<6;++i)write.elements.push_back({samplers_[i%2],views_[i],VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        backend_.writeDescriptorImageArray(write);
    }
private:
    void release() noexcept{
        for(auto& view:views_){if(view)backend_.destroyImageView(view);view=VK_NULL_HANDLE;}
        for(auto& sampler:samplers_){if(sampler)backend_.destroySampler(sampler);sampler=VK_NULL_HANDLE;}
        for(auto& image:images_)backend_.allocator().destroyImage(image);
        backend_.allocator().destroyBuffer(parameters_);
    }
    rhi::IRhi& backend_;
    std::array<rhi::GpuImage,6> images_{};
    std::array<VkImageView,6> views_{};
    std::array<VkSampler,2> samplers_{};
    rhi::GpuBuffer parameters_{};
};
}
