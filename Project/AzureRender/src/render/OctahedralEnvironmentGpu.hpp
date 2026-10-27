#pragma once
#include "render/OctahedralEnvironment.hpp"
#include "rhi/Rhi.hpp"

namespace azurerender {
// The RHI outlives this immutable owner. Callers retire submitted uses before
// destruction; parameter rows are CPU values and require no resident buffer.
class OctahedralEnvironmentGpu final {
public:
    using Parameters=std::array<std::array<float,4>,2>;
    OctahedralEnvironmentGpu(rhi::IRhi& backend,const OctahedralEnvironment& source):backend_(backend){
        source.validate();constexpr auto format=VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        if(!backend_.supportsSampledImage(format,true))throw std::runtime_error("Device lacks linear packed HDR sampling");
        rhi::GpuBuffer staging;
        try{
            const auto bytes=VkDeviceSize(source.texels.size())*4;
            staging=backend_.allocator().createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
            std::memcpy(staging.mapped,source.texels.data(),std::size_t(bytes));backend_.allocator().flush(staging,0,bytes);
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;
            info.extent={source.width,source.height,1};info.mipLevels=source.mipLevels;info.arrayLayers=source.layers;
            info.format=format;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;info.samples=VK_SAMPLE_COUNT_1_BIT;
            image_=backend_.allocator().createImage(info,false);
            backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,info.mipLevels);
            backend_.copyBufferToImage(staging,image_,source.width,source.height,source.mipLevels);
            backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,info.mipLevels);
            backend_.allocator().destroyBuffer(staging);
            view_=backend_.createImageView(image_.image,format,VK_IMAGE_ASPECT_COLOR_BIT,info.mipLevels,0,VK_IMAGE_VIEW_TYPE_2D_ARRAY,source.layers);
            rhi::SamplerDesc sampler;sampler.addressU=sampler.addressV=sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler.maxLod=float(source.mipLevels-1);sampler_=backend_.createSampler(sampler);
            parameters_={source.uvScaleOffset,{float(source.foldZeroSign==OctahedralEnvironment::FoldZeroSign::Positive),
                float(source.width),float(source.height),float(source.layers)}};
        }catch(...){backend_.allocator().destroyBuffer(staging);release();throw;}
    }
    ~OctahedralEnvironmentGpu(){release();}
    OctahedralEnvironmentGpu(const OctahedralEnvironmentGpu&)=delete;
    OctahedralEnvironmentGpu& operator=(const OctahedralEnvironmentGpu&)=delete;
    [[nodiscard]] const rhi::GpuImage& image() const noexcept{return image_;}
    [[nodiscard]] VkImageView view() const noexcept{return view_;}
    [[nodiscard]] VkSampler sampler() const noexcept{return sampler_;}
    [[nodiscard]] const Parameters& parameters() const noexcept{return parameters_;}
    void writeTexture(VkDescriptorSet set,std::uint32_t binding) const {
        backend_.writeDescriptorImage({set,binding,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,view_,sampler_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }
private:
    void release() noexcept {
        if(view_)backend_.destroyImageView(view_);if(sampler_)backend_.destroySampler(sampler_);
        backend_.allocator().destroyImage(image_);view_=VK_NULL_HANDLE;sampler_=VK_NULL_HANDLE;
    }
    rhi::IRhi& backend_;rhi::GpuImage image_{};VkImageView view_=VK_NULL_HANDLE;VkSampler sampler_=VK_NULL_HANDLE;
    Parameters parameters_{};
};
static_assert(sizeof(OctahedralEnvironmentGpu::Parameters)==32);
}
