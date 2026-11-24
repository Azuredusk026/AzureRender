#include "render/ProjectedReflectionRay.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace azurerender {
namespace {
void require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
bool extent(VkExtent2D value){return value.width&&value.height&&value.width<=8192&&value.height<=8192;}
template<class T>void finiteMatrix(const T& matrix){for(const auto& column:matrix)for(float value:column)require(std::isfinite(value),"Nonfinite projected reflection matrix");}
}
ProjectedReflectionFrame ProjectedReflectionView::pack(VkExtent2D source,VkExtent2D trace,const ProjectedReflectionSettings& settings) const{
    require(extent(source)&&extent(trace),"Projected reflection extent must be within 1..8192");
    const bool same=source.width==trace.width&&source.height==trace.height;
    const bool half=std::max(1U,source.width/2)==trace.width&&std::max(1U,source.height/2)==trace.height;
    require(same||half,"Projected reflection requires full or floor-half trace extent");
    finiteMatrix(worldNormalToView);finiteMatrix(viewDirectionToClip);finiteMatrix(clipToView);
    finiteMatrix(relativeWorldToClip);finiteMatrix(clipToWorld);finiteMatrix(previousClipToWorld);
    for(float value:position)require(std::isfinite(value),"Nonfinite projected reflection camera position");
    for(float value:depthReciprocal)require(std::isfinite(value),"Nonfinite projected reflection depth convention");
    require(depthReciprocal[1]>=0&&depthReciprocal[0]+depthReciprocal[1]>0,"Invalid projected reflection depth convention");
    require(convention==ProjectedReflectionConvention::MirroredViewY||convention==ProjectedReflectionConvention::VulkanUpperLeft,"Unknown projected reflection coordinate convention");
    require(settings.motionEncoding==ProjectedReflectionMotionEncoding::FourthRoot||settings.motionEncoding==ProjectedReflectionMotionEncoding::LinearUv,"Unknown projected reflection motion encoding");
    require(settings.jitterSeed<=16777215&&settings.maxSteps>=1&&settings.maxSteps<=4096,"Projected reflection sample budget outside range");
    require(std::isfinite(settings.edgeFade)&&settings.edgeFade>0&&settings.edgeFade<=1,"Projected reflection edge fade must be within (0,1]");
    require(std::isfinite(settings.motionBlend)&&settings.motionBlend>=0&&settings.motionBlend<=1,"Projected reflection motion blend must be within [0,1]");
    require(std::isfinite(settings.maximumFootprintLevel)&&settings.maximumFootprintLevel>0&&settings.maximumFootprintLevel<=16,"Projected reflection footprint level must be within (0,16]");
    for(float value:settings.motionUvOffset)require(std::isfinite(value)&&value>=-1&&value<=1,"Projected reflection motion UV offset must be within [-1,1]");
    require(settings.motionEncoding==ProjectedReflectionMotionEncoding::LinearUv||(!settings.requireTrackedMotion&&settings.motionUvOffset[0]==0&&settings.motionUvOffset[1]==0),"Projected reflection motion correction and response require linear UV encoding");
    ProjectedReflectionFrame frame{};unsigned offset=0;
    const auto append=[&](const auto& matrix){for(const auto& column:matrix)frame[offset++]=column;};
    append(worldNormalToView);append(viewDirectionToClip);append(clipToView);append(relativeWorldToClip);append(clipToWorld);append(previousClipToWorld);
    frame[22]={position[0],position[1],position[2],0};
    frame[23]={float(source.width),float(source.height),1.F/source.width,1.F/source.height};
    frame[24]={0,0,depthReciprocal[0],depthReciprocal[1]};
    frame[25]={float(trace.width),float(trace.height),1.F/trace.width,1.F/trace.height};
    frame[26]={float(settings.jitterSeed),settings.edgeFade,settings.motionUvOffset[0],settings.motionUvOffset[1]};frame[27]={0,0,settings.motionBlend,settings.requireTrackedMotion?1.F:0.F};
    frame[28]={float(settings.maxSteps),0,0,0};frame[29]={0,settings.maximumFootprintLevel,
        settings.motionEncoding==ProjectedReflectionMotionEncoding::LinearUv?1.F:0.F,convention==ProjectedReflectionConvention::VulkanUpperLeft?1.F:0.F};return frame;
}
ProjectedReflectionRayGpu::ProjectedReflectionRayGpu(rhi::IRhi& backend,unsigned frames,const std::vector<char>& code):backend_(backend){
    require(frames>=1&&frames<=8,"Projected reflection frame count must be within 1..8");
    std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    require(code.size()>=20&&code.size()<=1024*1024&&code.size()%4==0&&magic==0x07230203U,"Projected reflection requires bounded SPIR-V");
    for(const auto format:{VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_FORMAT_R16G16_UNORM,VK_FORMAT_R8_UNORM})
        if(!backend.supportsStorageImage(format))throw std::runtime_error("Projected reflection storage format unavailable");
    try{
        std::vector<rhi::DescriptorBindingDesc> bindings{{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}};
        for(unsigned i=2;i<8;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({8,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        for(unsigned i=9;i<12;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
        descriptors_=backend.createDescriptorSetLayout(bindings);rhi::DescriptorPoolDesc pool;pool.maxSets=frames;
        pool.sizes={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frames},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,6*frames},{VK_DESCRIPTOR_TYPE_SAMPLER,frames},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,3*frames}};
        pool_=backend.createDescriptorPool(pool);const auto sets=backend.allocateDescriptorSets(pool_,descriptors_,frames);
        layout_=backend.createPipelineLayout(descriptors_,nullptr);shader_=backend.createShaderModule(code);pipeline_=backend.createComputePipeline({shader_,layout_});
        rhi::SamplerDesc point;point.filter=VK_FILTER_NEAREST;point.mipmapLinear=false;point.maxLod=1000;
        point.addressU=point.addressV=point.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend.createSampler(point);
        slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){auto& slot=slots_[i];slot.set=sets.at(i);slot.parameters=backend.allocator().createBuffer(sizeof(ProjectedReflectionFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend.writeDescriptorBuffer({slot.set,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,slot.parameters.size});
            backend.writeDescriptorImage({slot.set,8,VK_DESCRIPTOR_TYPE_SAMPLER,VK_NULL_HANDLE,point_,VK_IMAGE_LAYOUT_UNDEFINED});}
    }catch(...){release();throw;}
}
ProjectedReflectionRayGpu::~ProjectedReflectionRayGpu(){release();}
void ProjectedReflectionRayGpu::release() noexcept{
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_={};
    for(auto& slot:slots_)backend_.allocator().destroyBuffer(slot.parameters);slots_.clear();
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_={};if(shader_)backend_.destroyShaderModule(shader_);shader_={};
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_={};if(descriptors_)backend_.destroyDescriptorSetLayout(descriptors_);descriptors_={};
    if(point_)backend_.destroySampler(point_);point_={};
}
void ProjectedReflectionRayGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ProjectedReflectionInputs& inputs,const ProjectedReflectionOutputs& outputs,
    const ProjectedReflectionView& view,const ProjectedReflectionSettings& settings){
    auto& slot=slots_.at(index);const auto frame=view.pack(inputs.sourceExtent,inputs.traceExtent,settings);
    require(outputs.extent.width==inputs.traceExtent.width&&outputs.extent.height==inputs.traceExtent.height,"Projected reflection output extent differs");
    require(inputs.tiles&&inputs.tileBytes>=4&&inputs.tileBytes%4==0&&inputs.tileCount<=65535&&std::uint64_t(inputs.tileCount)*4<=inputs.tileBytes,"Invalid projected reflection worklist");
    const auto capacity=std::uint64_t((inputs.traceExtent.width+7)/8)*((inputs.traceExtent.height+7)/8);
    require(inputs.tileCount<=capacity,"Projected reflection tile count exceeds trace extent");
    const std::array<VkImageView,6> sampled{{inputs.previousColor,inputs.previousDepth,inputs.currentDepth,inputs.motion,inputs.surfaceMask,inputs.normalRoughness}};
    const std::array<VkImageView,3> writable{{outputs.color,outputs.footprint,outputs.confidence}};
    for(const auto image:sampled)require(image!=VK_NULL_HANDLE,"Missing projected reflection sampled view");
    for(unsigned i=0;i<writable.size();++i){require(writable[i]!=VK_NULL_HANDLE,"Missing projected reflection output view");
        for(unsigned j=0;j<i;++j)require(writable[i]!=writable[j],"Projected reflection outputs alias");
        for(const auto image:sampled)require(image!=writable[i],"Projected reflection input aliases output");}
    if(!inputs.tileCount)return;
    std::memcpy(slot.parameters.mapped,&frame,sizeof(frame));backend_.allocator().flush(slot.parameters,0,sizeof(frame));
    backend_.writeDescriptorBuffer({slot.set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,inputs.tiles,VkDeviceSize(inputs.tileCount)*4});
    for(unsigned i=0;i<sampled.size();++i)backend_.writeDescriptorImage({slot.set,2+i,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,sampled[i],VK_NULL_HANDLE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    for(unsigned i=0;i<writable.size();++i)backend_.writeDescriptorImage({slot.set,9+i,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,writable[i],VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    commands.bindComputePipeline(pipeline_);commands.bindComputeDescriptorSet(layout_,slot.set);commands.dispatch(inputs.tileCount,1,1);
}
}
