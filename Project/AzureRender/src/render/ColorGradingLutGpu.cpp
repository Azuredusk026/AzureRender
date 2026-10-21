#include "render/ColorGradingLutGpu.hpp"
#include <cstring>
#include <stdexcept>
namespace azurerender {
ColorGradingLutGpu::ColorGradingLutGpu(rhi::IRhi& backend,const ColorGradingLut& lut,ColorGradingLutUpload upload):backend_(backend){
    validateColorGradingLut(lut);
    if(upload!=ColorGradingLutUpload::Float32&&upload!=ColorGradingLutUpload::IdentityPlaceholder)
        throw std::invalid_argument("Unknown LUT upload policy");
    const bool placeholder=upload==ColorGradingLutUpload::IdentityPlaceholder;
    if(placeholder&&(lut.dimension!=2||lut.inputDomain!=ColorGradingInputDomain::Linear||lut.image.rgba32!=makeIdentityColorGradingLut().image.rgba32))
        throw std::invalid_argument("LUT placeholder requires the exact two-step linear identity lattice");
    const auto format=placeholder?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R32G32B32A32_SFLOAT;
    if(!backend_.supportsSampledImage(format,true))throw std::runtime_error("Device lacks the requested color grading LUT sampling format");
    auto& allocator=backend_.allocator();rhi::GpuBuffer staging{};
    try{
        const auto bytes=lut.image.rgba32.size()*(placeholder?1:sizeof(float));
        staging=allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
        if(placeholder){auto* data=static_cast<std::uint8_t*>(staging.mapped);for(std::size_t i=0;i<lut.image.rgba32.size();++i)data[i]=lut.image.rgba32[i]==1?255:0;}
        else std::memcpy(staging.mapped,lut.image.rgba32.data(),bytes);
        allocator.flush(staging,0,bytes);
        image_=allocator.createImage2D(lut.image.width,lut.image.height,format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        backend_.transitionImageLayout(image_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
        backend_.copyBufferToImage(staging,image_,lut.image.width,lut.image.height,1,placeholder?4:16);
        backend_.executeOneShot([&](rhi::ICommandRecorder& commands){
            commands.imageBarrier({image_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        });
        allocator.destroyBuffer(staging);
        view_=backend_.createImageView(image_.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1);
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_LINEAR;sampler.addressU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.maxLod=0;sampler.mipmapLinear=false;sampler_=backend_.createSampler(sampler);
        dimension_=lut.dimension;domain_=lut.inputDomain;
    }catch(...){allocator.destroyBuffer(staging);release();throw;}
}
ColorGradingLutGpu::~ColorGradingLutGpu(){release();}
void ColorGradingLutGpu::release() noexcept{
    if(sampler_)backend_.destroySampler(sampler_);sampler_=VK_NULL_HANDLE;
    if(view_)backend_.destroyImageView(view_);view_=VK_NULL_HANDLE;
    backend_.allocator().destroyImage(image_);
}
}
