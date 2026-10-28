#include "render/ReflectionReprojection.hpp"
#include "render/ComputePass.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace azurerender {
ReflectionReprojectionFrame ReflectionReprojectionParameters::frame(VkExtent2D source,VkExtent2D output) const{
    const auto& p=previousDepthProjection;
    const bool same=source.width==output.width&&source.height==output.height;
    const bool half=std::max(1U,source.width/2)==output.width&&std::max(1U,source.height/2)==output.height;
    if(!source.width||!source.height||source.width>8192||source.height>8192||!output.width||!output.height||(!same&&!half)
        ||!std::all_of(p.begin(),p.end(),[](float value){return std::isfinite(value);})
        ||!std::isfinite(p[0]*p[3]-p[1]*p[2])||std::abs(p[0]*p[3]-p[1]*p[2])<1e-12F
        ||!std::isfinite(absoluteDepthTolerance)||absoluteDepthTolerance<0||absoluteDepthTolerance>100
        ||!std::isfinite(relativeDepthTolerance)||relativeDepthTolerance<0||relativeDepthTolerance>1)
        throw std::invalid_argument("Reflection reprojection requires matching dimensions, invertible depth projection and bounded tolerances");
    return {{{float(output.width),float(output.height),float(source.width),float(source.height)},p,
        {absoluteDepthTolerance,relativeDepthTolerance,historyValid?1.F:0.F,0}}};
}
ReflectionReprojectionGpu::ReflectionReprojectionGpu(rhi::IRhi& backend,VkExtent2D source,unsigned frames,const std::vector<char>& code,unsigned divisor)
    :backend_(backend),source_(source){
    if((divisor!=1&&divisor!=2)||!frames||frames>8)throw std::invalid_argument("Reflection reprojection requires a divisor of one or two and one to eight slots");
    extent_={std::max(1U,source.width/divisor),std::max(1U,source.height/divisor)};
    (void)ReflectionReprojectionParameters{}.frame(source_,extent_);
    bytes_=std::uint64_t(extent_.width)*extent_.height*kBytesPerPixel*frames;
    if(bytes_>kMaximumBytes)throw std::invalid_argument("Reflection reprojection exceeds 256 MiB");
    constexpr auto usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    for(auto format:{VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT})
        if(!backend.supportsStorageImage(format)||!backend.supportsSampledImage(format,false)||!(backend.imageSampleCounts(format,usage)&VK_SAMPLE_COUNT_1_BIT))
            throw std::runtime_error("Reflection reprojection requires sampled float storage formats");
    std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    if(code.size()<20||code.size()>1024*1024||code.size()%4||magic!=0x07230203U)throw std::invalid_argument("Reflection reprojection requires bounded SPIR-V");
    try{
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.maxLod=0;sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend.createSampler(sampler);
        descriptors_=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},{4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc pool;pool.maxSets=frames;pool.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3*frames},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2*frames},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames}};
        pool_=backend.createDescriptorPool(pool);const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,frames);
        layout_=backend.createPipelineLayout(descriptors_,nullptr);shader_=backend.createShaderModule(code);pipeline_=backend.createComputePipeline({shader_,layout_});
        slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){auto& slot=slots_[i];slot.set=sets.at(i);
            slot.motion=backend.allocator().createImage2D(extent_.width,extent_.height,VK_FORMAT_R32G32B32A32_SFLOAT,usage);
            slot.acceptance=backend.allocator().createImage2D(extent_.width,extent_.height,VK_FORMAT_R32_SFLOAT,usage);
            slot.motionView=backend.createImageView(slot.motion.image,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            slot.acceptanceView=backend.createImageView(slot.acceptance.image,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            slot.parameters=backend.allocator().createBuffer(sizeof(ReflectionReprojectionFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend.writeDescriptorBuffer({slot.set,5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,sizeof(ReflectionReprojectionFrame)});
        }
    }catch(...){release();throw;}
}
ReflectionReprojectionGpu::~ReflectionReprojectionGpu(){release();}
void ReflectionReprojectionGpu::release() noexcept{
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_={};
    for(auto& slot:slots_){if(slot.motionView)backend_.destroyImageView(slot.motionView);if(slot.acceptanceView)backend_.destroyImageView(slot.acceptanceView);
        backend_.allocator().destroyImage(slot.motion);backend_.allocator().destroyImage(slot.acceptance);backend_.allocator().destroyBuffer(slot.parameters);}slots_.clear();
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_={};if(shader_)backend_.destroyShaderModule(shader_);shader_={};
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_={};if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);descriptors_={};
    if(point_)backend_.destroySampler(point_);point_={};
}
ReflectionReprojectionOutput ReflectionReprojectionGpu::output(unsigned index) const{
    const auto& slot=slots_.at(index);return {slot.motion.image,slot.acceptance.image,slot.motionView,slot.acceptanceView,extent_};
}
void ReflectionReprojectionGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ReflectionReprojectionInputs& inputs,const ReflectionReprojectionParameters& parameters){
    auto& slot=slots_.at(index);const auto values=parameters.frame(source_,extent_);
    if(!inputs.surface||!inputs.motion||inputs.width!=source_.width||inputs.height!=source_.height||(parameters.historyValid&&!inputs.previousSelectedMotion))
        throw std::invalid_argument("Reflection reprojection requires complete matching source views");
    for(const auto& own:slots_)for(auto view:{own.motionView,own.acceptanceView})
        if(inputs.surface==view||inputs.motion==view)throw std::invalid_argument("Reflection reprojection source aliases its output");
    if(inputs.previousSelectedMotion==slot.motionView||inputs.previousSelectedMotion==slot.acceptanceView)
        throw std::invalid_argument("Reflection reprojection history aliases its current output");
    std::memcpy(slot.parameters.mapped,&values,sizeof(values));backend_.allocator().flush(slot.parameters,0,sizeof(values));
    const VkImageView sources[]={inputs.surface,inputs.motion,parameters.historyValid?inputs.previousSelectedMotion:inputs.motion};
    for(unsigned i=0;i<3;++i)backend_.writeDescriptorImage({slot.set,i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sources[i],point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend_.writeDescriptorImage({slot.set,3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.motionView,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    backend_.writeDescriptorImage({slot.set,4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.acceptanceView,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    // Every texel is replaced. Discard transitions also support abandoned recordings.
    for(auto image:{slot.motion.image,slot.acceptance.image})commands.imageBarrier({image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,1,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
    ComputePass({extent_.width,extent_.height,8,8,1,true}).record(commands,pipeline_,layout_,slot.set);
    for(auto image:{slot.motion.image,slot.acceptance.image})commands.imageBarrier({image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
}
}
