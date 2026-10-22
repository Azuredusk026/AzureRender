#include "render/EnvironmentCubeTexture.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace azurerender {

EnvironmentCubeTexture::EnvironmentCubeTexture(rhi::IRhi& backend,EnvironmentImage source):backend_(backend) {
    auto cube=environmentAsCubeFaces(std::move(source));
    constexpr auto format=VK_FORMAT_R16G16B16A16_SFLOAT;
    if(!backend_.supportsSampledImage(format,true))
        throw std::runtime_error("Device does not support linear HDR cube sampling");
    rhi::GpuBuffer staging;
    try {
        const auto bytes=VkDeviceSize(cube.rgba16f.size())*sizeof(std::uint16_t);
        staging=backend_.allocator().createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
        std::memcpy(staging.mapped,cube.rgba16f.data(),std::size_t(bytes));
        backend_.allocator().flush(staging,0,bytes);
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;info.imageType=VK_IMAGE_TYPE_2D;
        info.extent={cube.width,cube.height,1};info.mipLevels=std::max(1U,cube.authoredMipLevels);
        info.arrayLayers=6;info.format=format;info.tiling=VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        info.samples=VK_SAMPLE_COUNT_1_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        image_=backend_.allocator().createImage(info,false);
        backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,info.mipLevels);
        backend_.copyBufferToImage(staging,image_,cube.width,cube.height,info.mipLevels,8);
        backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,info.mipLevels);
        backend_.allocator().destroyBuffer(staging);
        view_=backend_.createImageView(image_.image,format,VK_IMAGE_ASPECT_COLOR_BIT,info.mipLevels,0,VK_IMAGE_VIEW_TYPE_CUBE,6);
        rhi::SamplerDesc sampler;
        sampler.addressU=sampler.addressV=sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.maxLod=float(info.mipLevels-1);sampler.maxAnisotropy=1;
        sampler_=backend_.createSampler(sampler);
    } catch(...) {
        backend_.allocator().destroyBuffer(staging);release();throw;
    }
}

EnvironmentCubeTexture::~EnvironmentCubeTexture(){release();}
void EnvironmentCubeTexture::release() noexcept {
    if(sampler_)backend_.destroySampler(sampler_);
    if(view_)backend_.destroyImageView(view_);
    backend_.allocator().destroyImage(image_);sampler_=VK_NULL_HANDLE;view_=VK_NULL_HANDLE;
}

}
