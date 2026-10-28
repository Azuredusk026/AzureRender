#include "render/DepthPyramid.hpp"
#include "render/ComputePass.hpp"
#include <algorithm>
#include <array>
#include <cstring>
namespace azurerender {
DepthPyramidGpu::DepthPyramidGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned levels,unsigned frames,const std::vector<char>& code)
    :backend_(backend),extent_(extent),levels_(levels){
    unsigned full=1;for(auto side=std::max(extent.width,extent.height);side>1;side/=2)++full;
    if(!extent.width||!extent.height||extent.width>8192||extent.height>8192||!levels||levels>full||!frames||frames>8)
        throw std::invalid_argument("Depth pyramid requires bounded dimensions, levels and slots");
    std::uint64_t pixels=0;for(unsigned mip=0;mip<levels;++mip)pixels+=std::uint64_t(std::max(1U,extent.width>>mip))*std::max(1U,extent.height>>mip);
    if(pixels*4*frames>512ULL*1024*1024)throw std::invalid_argument("Depth pyramid exceeds 512 MiB");
    constexpr auto format=VK_FORMAT_R32_SFLOAT;
    constexpr auto usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(!backend.supportsStorageImage(format)||!backend.supportsSampledImage(format,false)||!(backend.imageSampleCounts(format,usage)&VK_SAMPLE_COUNT_1_BIT))
        throw std::runtime_error("Sampled storage depth format unavailable");
    std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    if(code.size()<20||code.size()>1024*1024||code.size()%4||magic!=0x07230203U)throw std::invalid_argument("Depth pyramid requires bounded SPIR-V");
    try{
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.maxLod=float(levels-1);sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend.createSampler(sampler);
        descriptors_=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        const unsigned count=frames*levels;rhi::DescriptorPoolDesc desc;desc.maxSets=count;
        desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,count},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,count},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,count}};
        pool_=backend.createDescriptorPool(desc);const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,count);
        layout_=backend.createPipelineLayout(descriptors_,nullptr);shader_=backend.createShaderModule(code);pipeline_=backend.createComputePipeline({shader_,layout_});
        slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){auto& slot=slots_[i];slot.image=backend.allocator().createImage2D(extent.width,extent.height,format,usage,levels);
            slot.all=backend.createImageView(slot.image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,levels);slot.mips.resize(levels);
            for(unsigned mip=0;mip<levels;++mip){auto& target=slot.mips[mip];target.view=backend.createImageView(slot.image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1,mip);
                target.set=sets.at(i*levels+mip);target.parameters=backend.allocator().createBuffer(16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
                backend.writeDescriptorBuffer({target.set,2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,target.parameters.buffer,16});}
        }
    }catch(...){release();throw;}
}
DepthPyramidGpu::~DepthPyramidGpu(){release();}
void DepthPyramidGpu::release() noexcept{
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_={};
    for(auto& slot:slots_){for(auto& mip:slot.mips){if(mip.view)backend_.destroyImageView(mip.view);backend_.allocator().destroyBuffer(mip.parameters);}
        if(slot.all)backend_.destroyImageView(slot.all);backend_.allocator().destroyImage(slot.image);}slots_.clear();
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_={};if(shader_)backend_.destroyShaderModule(shader_);shader_={};
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_={};if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);descriptors_={};
    if(point_)backend_.destroySampler(point_);point_={};
}
DepthPyramidOutput DepthPyramidGpu::output(unsigned index) const{const auto& slot=slots_.at(index);return {slot.image.image,slot.all,point_,extent_,levels_};}
void DepthPyramidGpu::record(rhi::ICommandRecorder& commands,unsigned index,VkImageView source,DepthReduction reduction,VkExtent2D sourceExtent){
    auto& slot=slots_.at(index);
    if(!source||(reduction!=DepthReduction::Minimum&&reduction!=DepthReduction::Maximum))throw std::invalid_argument("Invalid depth pyramid input or reduction");
    if(!sourceExtent.width&&!sourceExtent.height)sourceExtent=extent_;
    const bool reduced=sourceExtent.width!=extent_.width||sourceExtent.height!=extent_.height;
    if(!sourceExtent.width||!sourceExtent.height||sourceExtent.width>8192||sourceExtent.height>8192||
        (reduced&&(std::max(1U,sourceExtent.width/2)!=extent_.width||std::max(1U,sourceExtent.height/2)!=extent_.height)))
        throw std::invalid_argument("Depth source must match the output or its floor-half reduction");
    for(const auto& candidate:slots_){if(source==candidate.all)throw std::invalid_argument("Depth input aliases pyramid output");
        for(const auto& mip:candidate.mips)if(source==mip.view)throw std::invalid_argument("Depth input aliases pyramid mip");}
    for(unsigned mip=0;mip<levels_;++mip){auto& target=slot.mips[mip];const std::array<std::uint32_t,4> values{mip||reduced?1U:0U,reduction==DepthReduction::Maximum?1U:0U,0,0};
        std::memcpy(target.parameters.mapped,values.data(),16);backend_.allocator().flush(target.parameters,0,16);
        backend_.writeDescriptorImage({target.set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,mip?slot.mips[mip-1].view:source,point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        backend_.writeDescriptorImage({target.set,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,target.view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});}
    for(unsigned mip=0;mip<levels_;++mip){
        commands.imageBarrier({slot.image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,1,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_IMAGE_ASPECT_COLOR_BIT,mip});
        ComputePass({std::max(1U,extent_.width>>mip),std::max(1U,extent_.height>>mip),8,8,1,true}).record(commands,pipeline_,layout_,slot.mips[mip].set);
        commands.imageBarrier({slot.image.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_COLOR_BIT,mip});
    }
}
}
