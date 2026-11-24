#include "render/ReflectionTemporal.hpp"
#include "render/ComputePass.hpp"
#include <cmath>
#include <cstring>
namespace azurerender {
ReflectionTemporalFrame ReflectionTemporalParameters::frame(unsigned width,unsigned height) const {
    if(!width||!height||width>8192||height>8192||!std::isfinite(varianceScale)||varianceScale<0||varianceScale>10||
        !std::isfinite(historyWeight)||historyWeight<0||historyWeight>1||!std::isfinite(minimumColor)||minimumColor<0||minimumColor>1)
        throw std::invalid_argument("Temporal reflection requires bounded dimensions and finite parameters");
    return {{{float(width),float(height),1.F/float(width),1.F/float(height)},
        {varianceScale,historyWeight,minimumColor,float((historyValid?1U:0U)|(rejectOutside?2U:0U))}}};
}
ReflectionTemporalGpu::ReflectionTemporalGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,VkFormat color,VkFormat confidence,const ReflectionTemporalShaders& shaders)
    :backend_(backend),extent_(extent),color_(color),confidence_(confidence){
    (void)ReflectionTemporalParameters{}.frame(extent.width,extent.height);
    if(!frames||frames>8)throw std::invalid_argument("Temporal reflection requires one to eight slots");
    const bool packed=color==VK_FORMAT_B10G11R11_UFLOAT_PACK32&&confidence==VK_FORMAT_R8_UNORM;
    if(!packed&&(color!=VK_FORMAT_R32G32B32A32_SFLOAT||confidence!=VK_FORMAT_R32_SFLOAT))throw std::invalid_argument("Invalid temporal reflection output formats");
    if(!backend.supportsStorageImage(color)||!backend.supportsStorageImage(confidence))throw std::runtime_error("Temporal reflection storage format unavailable");
    const auto& code=packed?shaders.packed:shaders.wide;std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    if(code.size()<20||code.size()>1024*1024||code.size()%4||magic!=0x07230203U)throw std::invalid_argument("Temporal reflection requires bounded SPIR-V");
    try{
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.maxLod=0;sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend.createSampler(sampler);
        std::vector<rhi::DescriptorBindingDesc> bindings;
        for(unsigned i=0;i<6;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({6,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({7,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({8,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        descriptors_=backend.createDescriptorSetLayout(bindings);
        rhi::DescriptorPoolDesc desc;desc.maxSets=frames;desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6*frames},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2*frames},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames}};
        pool_=backend.createDescriptorPool(desc);const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,frames);
        layout_=backend.createPipelineLayout(descriptors_,nullptr);shader_=backend.createShaderModule(code);pipeline_=backend.createComputePipeline({shader_,layout_});
        slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){auto& slot=slots_[i];slot.set=sets.at(i);slot.parameters=backend.allocator().createBuffer(sizeof(ReflectionTemporalFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend.writeDescriptorBuffer({slot.set,8,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,slot.parameters.size});}
    }catch(...){release();throw;}
}
ReflectionTemporalGpu::~ReflectionTemporalGpu(){release();}
void ReflectionTemporalGpu::release() noexcept {
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_={};for(auto& slot:slots_)backend_.allocator().destroyBuffer(slot.parameters);slots_.clear();
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_={};if(shader_)backend_.destroyShaderModule(shader_);shader_={};
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_={};if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);descriptors_={};
    if(point_)backend_.destroySampler(point_);point_={};
}
void ReflectionTemporalGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ReflectionTemporalInputs& inputs,const ReflectionPyramidViews& output,const ReflectionTemporalParameters& parameters){
    auto& slot=slots_.at(index);
    const VkImageView sources[]={inputs.color,inputs.confidence,inputs.previousColor,inputs.previousConfidence,inputs.motion,inputs.weight};
    if(inputs.width!=extent_.width||inputs.height!=extent_.height||output.width!=extent_.width||output.height!=extent_.height||
        output.levels!=1||!output.color||!output.confidence||output.color==output.confidence||output.colorFormat!=color_||output.confidenceFormat!=confidence_)
        throw std::invalid_argument("Temporal reflection requires matching inputs and base outputs");
    for(auto view:sources)if(!view||view==output.color||view==output.confidence)throw std::invalid_argument("Temporal reflection source is empty or aliases its output");
    const auto frame=parameters.frame(inputs.width,inputs.height);
    std::memcpy(slot.parameters.mapped,&frame,sizeof(frame));backend_.allocator().flush(slot.parameters,0,sizeof(frame));
    for(unsigned i=0;i<6;++i)backend_.writeDescriptorImage({slot.set,i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sources[i],point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend_.writeDescriptorImage({slot.set,6,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,output.color,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    backend_.writeDescriptorImage({slot.set,7,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,output.confidence,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    ComputePass({extent_.width,extent_.height,8,8,1,true}).record(commands,pipeline_,layout_,slot.set);
}
}
