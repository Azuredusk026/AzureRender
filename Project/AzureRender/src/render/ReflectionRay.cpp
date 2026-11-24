#include "render/ReflectionRay.hpp"
#include "render/ComputePass.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
ReflectionTraceFrame ReflectionTraceView::pack(unsigned width,unsigned height,const ScreenSpaceReflectionSettings& settings,ReflectionColorEncoding encoding) const{
    if(!width||!height||width>8192||height>8192)throw std::invalid_argument("Reflection trace extent must be within 1..8192");
    validateScreenSpaceReflection(settings);
    if(encoding!=ReflectionColorEncoding::Linear&&encoding!=ReflectionColorEncoding::MaxComponent)throw std::invalid_argument("Unknown reflection trace encoding");
    for(const auto* values:{&projection,&worldToClip})for(float value:*values)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite reflection projection");
    for(const auto* values:{&position,&right,&up,&forward})for(float value:*values)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite reflection camera pose");
    const auto dot=[](const auto& a,const auto& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    for(const auto* axis:{&right,&up,&forward})if(std::abs(dot(*axis,*axis)-1)>1e-3F)throw std::invalid_argument("Reflection camera basis must be unit length");
    if(std::abs(dot(right,up))>1e-3F||std::abs(dot(right,forward))>1e-3F||std::abs(dot(up,forward))>1e-3F)
        throw std::invalid_argument("Reflection camera basis must be orthogonal");
    const std::array<float,3> cross{right[1]*up[2]-right[2]*up[1],right[2]*up[0]-right[0]*up[2],right[0]*up[1]-right[1]*up[0]};
    if(std::abs(dot(cross,forward)+1)>1e-3F)throw std::invalid_argument("Reflection camera basis has inconsistent handedness");
    const bool perspective=std::abs(projection[15])<1e-6F&&std::abs(projection[11]+1)<1e-6F;
    const bool orthographic=std::abs(projection[15]-1)<1e-6F&&std::abs(projection[11])<1e-6F;
    if((!perspective&&!orthographic)||std::abs(projection[0])<1e-6F||std::abs(projection[5])<1e-6F)
        throw std::invalid_argument("Reflection projection must be perspective or orthographic");
    for(unsigned index:{1U,2U,3U,4U,6U,7U})if(std::abs(projection[index])>1e-6F)throw std::invalid_argument("Reflection projection has unsupported skew");
    if(perspective?(std::abs(projection[12])+std::abs(projection[13])>1e-6F):(std::abs(projection[8])+std::abs(projection[9])>1e-6F))
        throw std::invalid_argument("Reflection projection offsets use an unsupported placement");
    ReflectionTraceFrame frame{};std::memcpy(frame.data(),worldToClip.data(),64);
    for(unsigned i=0;i<3;++i){frame[4][i]=position[i];frame[5][i]=right[i];frame[6][i]=up[i];frame[7][i]=forward[i];}
    frame[8]={1.F/projection[0],1.F/projection[5],perspective?projection[8]:-projection[12],perspective?projection[9]:-projection[13]};
    frame[9]={settings.enabled?1.F:0.F,settings.maxDistance,settings.thickness,settings.normalBias};
    frame[10]={float(settings.maxSteps),settings.maxRoughness,settings.edgeFade,settings.strength};
    frame[11]={float(width),float(height),perspective?1.F:0.F,encoding==ReflectionColorEncoding::MaxComponent?1.F:0.F};return frame;
}
ReflectionRayGpu::ReflectionRayGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,VkFormat color,VkFormat confidence,const ReflectionTraceShaders& shaders)
    :backend_(backend),extent_(extent),color_(color),confidence_(confidence){
    if(!extent.width||!extent.height||extent.width>8192||extent.height>8192||!frames||frames>8)throw std::invalid_argument("Invalid reflection trace dimensions or slots");
    const bool packed=color==VK_FORMAT_B10G11R11_UFLOAT_PACK32&&confidence==VK_FORMAT_R8_UNORM;
    if(!packed&&(color!=VK_FORMAT_R32G32B32A32_SFLOAT||confidence!=VK_FORMAT_R32_SFLOAT))throw std::invalid_argument("Invalid reflection trace output formats");
    if(!backend.supportsStorageImage(color)||!backend.supportsStorageImage(confidence))throw std::runtime_error("Reflection trace storage format unavailable");
    const auto& code=packed?shaders.packed:shaders.wide;std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    if(code.size()<20||code.size()>1024*1024||code.size()%4||magic!=0x07230203U)throw std::invalid_argument("Reflection trace requires bounded SPIR-V");
    try{
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.maxLod=0;sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend.createSampler(sampler);
        descriptors_=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},{4,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc desc;desc.maxSets=frames;desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2*frames},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2*frames},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames}};pool_=backend.createDescriptorPool(desc);
        const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,frames);layout_=backend.createPipelineLayout(descriptors_,nullptr);
        shader_=backend.createShaderModule(code);pipeline_=backend.createComputePipeline({shader_,layout_});slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){auto& slot=slots_[i];slot.set=sets.at(i);slot.parameters=backend.allocator().createBuffer(sizeof(ReflectionTraceFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend.writeDescriptorBuffer({slot.set,4,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,slot.parameters.size});}
    }catch(...){release();throw;}
}
ReflectionRayGpu::~ReflectionRayGpu(){release();}
void ReflectionRayGpu::release() noexcept{
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_={};for(auto& slot:slots_)backend_.allocator().destroyBuffer(slot.parameters);slots_.clear();
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_={};if(shader_)backend_.destroyShaderModule(shader_);shader_={};
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_={};if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);descriptors_={};
    if(point_)backend_.destroySampler(point_);point_={};
}
void ReflectionRayGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ReflectionTraceInputs& inputs,const ReflectionPyramidViews& base,
    const ReflectionTraceView& view,const ScreenSpaceReflectionSettings& settings,ReflectionColorEncoding encoding){
    auto& slot=slots_.at(index);
    const bool same=inputs.width==extent_.width&&inputs.height==extent_.height;
    const bool half=std::max(1U,inputs.width/2)==extent_.width&&std::max(1U,inputs.height/2)==extent_.height;
    if(!inputs.surface||!inputs.color||!inputs.width||!inputs.height||inputs.width>8192||inputs.height>8192||(!same&&!half)||base.width!=extent_.width||base.height!=extent_.height
        ||base.levels!=1||!base.color||!base.confidence||base.color==base.confidence||base.colorFormat!=color_||base.confidenceFormat!=confidence_)
        throw std::invalid_argument("Reflection trace requires matching sampled inputs and writable base views");
    for(const auto source:{inputs.surface,inputs.color})if(source==base.color||source==base.confidence)throw std::invalid_argument("Reflection trace input aliases its output");
    const auto frame=view.pack(extent_.width,extent_.height,settings,encoding);std::memcpy(slot.parameters.mapped,&frame,sizeof(frame));backend_.allocator().flush(slot.parameters,0,sizeof(frame));
    backend_.writeDescriptorImage({slot.set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,inputs.surface,point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend_.writeDescriptorImage({slot.set,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,inputs.color,point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend_.writeDescriptorImage({slot.set,2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,base.color,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    backend_.writeDescriptorImage({slot.set,3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,base.confidence,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    ComputePass({extent_.width,extent_.height,8,8,1,true}).record(commands,pipeline_,layout_,slot.set);
}
}
