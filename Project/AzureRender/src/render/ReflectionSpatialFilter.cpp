#include "render/ReflectionSpatialFilter.hpp"
#include "render/ComputePass.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
namespace {
unsigned levelsFor(unsigned width,unsigned height){
    if(!width||!height||width>8192||height>8192)throw std::invalid_argument("Reflection filter extent must be within 1..8192");
    unsigned levels=1;for(auto side=std::max(width,height);side>1;side/=2)++levels;return levels;
}
void validateColor(const std::array<float,3>& color){
    for(float channel:color)if(!std::isfinite(channel)||channel<0)throw std::invalid_argument("Reflection radiance requires finite nonnegative channels");
}
}
ReflectionFilterFrame ReflectionFilterParameters::frame(unsigned width,unsigned height,unsigned depthLevels,unsigned colorLevels) const {
    const auto full=levelsFor(width,height);
    if(!depthLevels||!colorLevels||depthLevels>full||colorLevels>full)
        throw std::invalid_argument("Reflection filter mip count does not match the extent");
    if(!std::isfinite(maxLod)||maxLod<0||std::floor(maxLod)!=maxLod||maxLod>=std::min(depthLevels,colorLevels))
        throw std::invalid_argument("Reflection filter maximum LOD exceeds its pyramids");
    if(!std::isfinite(sharpLod)||sharpLod<0||sharpLod>1||!std::isfinite(depthFalloff)||depthFalloff<0||depthFalloff>1000
        ||!std::isfinite(minimumWeight)||minimumWeight<=0||minimumWeight>1)
        throw std::invalid_argument("Reflection filter weight parameters are invalid");
    if(!std::isfinite(depthDecode[0])||!std::isfinite(depthDecode[1])
        ||(reciprocalDepth&&(depthDecode[1]<=0||depthDecode[0]+depthDecode[1]<=0)))
        throw std::invalid_argument("Reflection filter depth decoding is invalid");
    if(encoding!=ReflectionColorEncoding::Linear&&encoding!=ReflectionColorEncoding::MaxComponent)
        throw std::invalid_argument("Reflection filter color encoding is invalid");
    return {{{1.F/width,1.F/height,maxLod,sharpLod},
        {depthDecode[0],depthDecode[1],reciprocalDepth?1.F:0.F,depthFalloff},
        {minimumWeight,encoding==ReflectionColorEncoding::MaxComponent?1.F:0.F,float(width),float(height)}}};
}
std::array<float,3> encodeReflectionRadiance(const std::array<float,3>& color){
    validateColor(color);auto result=color;const auto maximum=*std::max_element(color.begin(),color.end());
    for(auto& channel:result)channel/=1+maximum;
    if(*std::max_element(result.begin(),result.end())>=1)
        throw std::invalid_argument("Reflection radiance exceeds the representable compressed domain");
    return result;
}
std::array<float,3> decodeReflectionRadiance(const std::array<float,3>& color){
    validateColor(color);const auto maximum=*std::max_element(color.begin(),color.end());
    if(maximum>=1)throw std::invalid_argument("Compressed reflection radiance must remain below one");
    auto result=color;for(auto& channel:result)channel/=1-maximum;return result;
}
ReflectionSpatialFilterGpu::ReflectionSpatialFilterGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,const std::vector<char>& code,ReflectionFilterOutputAccess access)
    :backend_(backend),extent_(extent){
    if(access!=ReflectionFilterOutputAccess::Sampled&&access!=ReflectionFilterOutputAccess::StorageRead)throw std::invalid_argument("Unknown reflection output access");
    outputLayout_=access==ReflectionFilterOutputAccess::StorageRead?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const auto levels=levelsFor(extent.width,extent.height);
    if(!frames||frames>8||std::uint64_t(extent.width)*extent.height*16*frames>512ULL*1024*1024)
        throw std::invalid_argument("Reflection filter outputs exceed the frame or memory budget");
    std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    if(code.size()<20||code.size()%4||code.size()>1024*1024||magic!=0x07230203U)
        throw std::invalid_argument("Reflection filter requires bounded SPIR-V");
    if(!backend.supportsSampledImage(VK_FORMAT_R32G32B32A32_SFLOAT,access==ReflectionFilterOutputAccess::Sampled))
        throw std::runtime_error("Reflection filter output sampling is unavailable");
    try{
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.mipmapLinear=false;sampler.maxLod=float(levels-1);
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend_.createSampler(sampler);
        sampler.filter=VK_FILTER_LINEAR;linear_=backend_.createSampler(sampler);
        std::vector<rhi::DescriptorBindingDesc> bindings;
        for(unsigned i=0;i<4;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({6,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        setLayout_=backend_.createDescriptorSetLayout(bindings);
        rhi::DescriptorPoolDesc pool;pool.maxSets=frames;pool.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,5*frames},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,frames},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames}};
        pool_=backend_.createDescriptorPool(pool);const auto sets=backend_.allocateDescriptorSets(pool_,setLayout_,frames);
        layout_=backend_.createPipelineLayout(setLayout_,nullptr);shader_=backend_.createShaderModule(code);
        pipeline_=backend_.createComputePipeline({shader_,layout_});slots_.resize(frames);
        for(unsigned i=0;i<frames;++i){
            auto& slot=slots_[i];slot.set=sets.at(i);
            slot.image=backend_.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|(access==ReflectionFilterOutputAccess::StorageRead?VK_IMAGE_USAGE_TRANSFER_DST_BIT:0U));
            slot.view=backend_.createImageView(slot.image.image,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            slot.parameters=backend_.allocator().createBuffer(sizeof(ReflectionFilterFrame)+16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            backend_.writeDescriptorBuffer({slot.set,5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slot.parameters.buffer,slot.parameters.size});
        }
        if(access==ReflectionFilterOutputAccess::StorageRead){
            backend_.executeOneShot([&](rhi::ICommandRecorder& commands){for(auto& slot:slots_){
                commands.imageBarrier({slot.image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT});
                const VkClearColorValue zero{};commands.clearColorImage(slot.image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,zero);
                commands.imageBarrier({slot.image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,outputLayout_,1,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
            }});
            for(auto& slot:slots_)slot.initialized=true;
        }
    }catch(...){release();throw;}
}
ReflectionSpatialFilterGpu::~ReflectionSpatialFilterGpu(){release();}
void ReflectionSpatialFilterGpu::release() noexcept {
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_=VK_NULL_HANDLE;
    for(auto& slot:slots_){if(slot.view)backend_.destroyImageView(slot.view);backend_.allocator().destroyImage(slot.image);backend_.allocator().destroyBuffer(slot.parameters);}
    slots_.clear();
    if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_=VK_NULL_HANDLE;
    if(shader_)backend_.destroyShaderModule(shader_);shader_=VK_NULL_HANDLE;
    if(layout_)backend_.destroyPipelineLayout(layout_);layout_=VK_NULL_HANDLE;
    if(setLayout_)backend_.destroyDescriptorSetLayout(setLayout_);setLayout_=VK_NULL_HANDLE;
    if(linear_)backend_.destroySampler(linear_);linear_=VK_NULL_HANDLE;
    if(point_)backend_.destroySampler(point_);point_=VK_NULL_HANDLE;
}
ReflectionFilterOutput ReflectionSpatialFilterGpu::output(unsigned slot) const {
    const auto& value=slots_.at(slot);return {value.image.image,value.view,linear_,extent_,outputLayout_};
}
void ReflectionSpatialFilterGpu::record(rhi::ICommandRecorder& commands,unsigned index,const ReflectionFilterInputViews& inputs,const ReflectionFilterParameters& parameters){
    auto& slot=slots_.at(index);
    if(!inputs.depth||!inputs.surface||!inputs.color||inputs.width!=extent_.width||inputs.height!=extent_.height)
        throw std::invalid_argument("Reflection filter requires matching complete input views");
    for(const auto& target:slots_)if(inputs.depth==target.view||inputs.surface==target.view||inputs.color==target.view||inputs.confidence==target.view)
        throw std::invalid_argument("Reflection filter inputs cannot alias its outputs");
    const auto frame=parameters.frame(inputs.width,inputs.height,inputs.depthLevels,inputs.colorLevels);
    if(inputs.roughnessChannel>3)throw std::invalid_argument("Reflection roughness channel must be within 0..3");
    if(inputs.confidence&&(inputs.confidenceLevels>levelsFor(inputs.width,inputs.height)||inputs.confidenceLevels<=parameters.maxLod))
        throw std::invalid_argument("Reflection confidence pyramid has insufficient levels");
    std::array<std::array<float,4>,4> dispatch{};
    std::copy(frame.begin(),frame.end(),dispatch.begin());
    dispatch[3]={inputs.confidence?1.F:0.F,float(inputs.roughnessChannel),inputs.quantizeConfidence?1.F:0.F,0};
    std::memcpy(slot.parameters.mapped,&dispatch,sizeof(dispatch));backend_.allocator().flush(slot.parameters,0,sizeof(dispatch));
    backend_.writeDescriptorImage({slot.set,6,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        inputs.confidence?inputs.confidence:inputs.surface,linear_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    const VkImageView views[]={inputs.depth,inputs.surface,inputs.color,inputs.color};
    for(unsigned binding=0;binding<4;++binding)backend_.writeDescriptorImage({slot.set,binding,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        views[binding],binding==2?linear_:point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend_.writeDescriptorImage({slot.set,4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slot.view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
    commands.imageBarrier({slot.image.image,slot.initialized?outputLayout_:VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,1,VkPipelineStageFlags(slot.initialized?VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,slot.initialized?VK_ACCESS_SHADER_READ_BIT:0U,VK_ACCESS_SHADER_WRITE_BIT});
    ComputePass({extent_.width,extent_.height,8,8,1,true}).record(commands,pipeline_,layout_,slot.set);
    commands.imageBarrier({slot.image.image,VK_IMAGE_LAYOUT_GENERAL,outputLayout_,1,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    slot.initialized=true;
}
}
