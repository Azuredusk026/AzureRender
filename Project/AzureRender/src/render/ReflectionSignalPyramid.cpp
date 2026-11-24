#include "render/ReflectionSignalPyramid.hpp"
#include "render/ComputePass.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
namespace {
unsigned mipCount(unsigned width,unsigned height){
    if(!width||!height||width>8192||height>8192)throw std::invalid_argument("Reflection pyramid extent must be within 1..8192");
    unsigned count=1;for(auto side=std::max(width,height);side>1;side/=2)++count;return count;
}
ReflectionFilterParameters filterParameters(const ReflectionPyramidParameters& p,unsigned maximum){
    ReflectionFilterParameters result;result.maxLod=float(maximum);result.depthFalloff=p.depthFalloff;
    result.minimumWeight=p.minimumWeight;result.depthDecode=p.depthDecode;result.reciprocalDepth=p.reciprocalDepth;return result;
}
constexpr VkImageUsageFlags pyramidUsage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
}
ReflectionFilterFrame ReflectionPyramidParameters::frame(unsigned width,unsigned height,unsigned mip,unsigned depthLevels) const {
    const auto full=mipCount(width,height);
    if(!mip||mip>=full)throw std::invalid_argument("Reflection pyramid target mip is outside its extent");
    (void)filterParameters(*this,mip).frame(width,height,depthLevels,full);
    const auto sourceWidth=std::max(1U,width>>(mip-1)),sourceHeight=std::max(1U,height>>(mip-1));
    const auto targetWidth=std::max(1U,width>>mip),targetHeight=std::max(1U,height>>mip);
    return {{{1.F/sourceWidth,1.F/sourceHeight,float(mip),minimumWeight},
        {1.F/targetWidth,1.F/targetHeight,depthFalloff,0},{depthDecode[0],depthDecode[1],reciprocalDepth?1.F:0.F,0}}};
}
ReflectionSignalPyramidGpu::ReflectionSignalPyramidGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned levels,unsigned frames,
    ReflectionPyramidPrecision precision,const ReflectionPyramidShaders& shaders):backend_(backend),extent_(extent),levels_(levels){
    const auto full=mipCount(extent.width,extent.height);
    if(!levels||levels>full||!frames||frames>8)throw std::invalid_argument("Reflection pyramid frame or mip count is invalid");
    if(precision!=ReflectionPyramidPrecision::PreferPacked&&precision!=ReflectionPyramidPrecision::RequirePacked&&precision!=ReflectionPyramidPrecision::Float32)
        throw std::invalid_argument("Reflection pyramid precision is invalid");
    const auto supported=[&](VkFormat format){return backend_.supportsStorageImage(format)&&backend_.supportsSampledImage(format,true)
        &&(backend_.imageSampleCounts(format,pyramidUsage)&VK_SAMPLE_COUNT_1_BIT);};
    packed_=precision!=ReflectionPyramidPrecision::Float32&&supported(VK_FORMAT_B10G11R11_UFLOAT_PACK32)&&supported(VK_FORMAT_R8_UNORM);
    if(precision==ReflectionPyramidPrecision::RequirePacked&&!packed_)throw std::runtime_error("Packed reflection pyramid storage is unavailable");
    colorFormat_=packed_?VK_FORMAT_B10G11R11_UFLOAT_PACK32:VK_FORMAT_R32G32B32A32_SFLOAT;
    confidenceFormat_=packed_?VK_FORMAT_R8_UNORM:VK_FORMAT_R32_SFLOAT;
    if(!packed_&&(!supported(colorFormat_)||!supported(confidenceFormat_)))throw std::runtime_error("Float reflection pyramid storage is unavailable");
    std::uint64_t pixels=0;for(unsigned mip=0;mip<levels;++mip)pixels+=std::uint64_t(std::max(1U,extent.width>>mip))*std::max(1U,extent.height>>mip);
    if(pixels*(packed_?5:20)*frames>512ULL*1024*1024)throw std::invalid_argument("Reflection pyramid exceeds its output budget");
    const auto& code=packed_?shaders.packed:shaders.wide;std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    if(code.size()<20||code.size()%4||code.size()>1024*1024||magic!=0x07230203U)throw std::invalid_argument("Reflection pyramid requires bounded SPIR-V");
    try{
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.mipmapLinear=false;sampler.maxLod=float(levels-1);
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;point_=backend_.createSampler(sampler);
        std::vector<rhi::DescriptorBindingDesc> bindings;
        for(unsigned i=0;i<4;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({4,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
        bindings.push_back({5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});setLayout_=backend_.createDescriptorSetLayout(bindings);
        layout_=backend_.createPipelineLayout(setLayout_,nullptr);shader_=backend_.createShaderModule(code);pipeline_=backend_.createComputePipeline({shader_,layout_});
        const unsigned count=(levels-1)*frames;std::vector<VkDescriptorSet> sets;
        if(count){rhi::DescriptorPoolDesc desc;desc.maxSets=count;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,4*count},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,count},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,count}};
            pool_=backend_.createDescriptorPool(desc);sets=backend_.allocateDescriptorSets(pool_,setLayout_,count);}
        slots_.resize(frames);
        for(unsigned frame=0;frame<frames;++frame){
            auto& slot=slots_[frame];slot.mips.resize(levels);
            slot.color=backend_.allocator().createImage2D(extent.width,extent.height,colorFormat_,pyramidUsage,levels);
            slot.confidence=backend_.allocator().createImage2D(extent.width,extent.height,confidenceFormat_,pyramidUsage,levels);
            slot.allColor=backend_.createImageView(slot.color.image,colorFormat_,VK_IMAGE_ASPECT_COLOR_BIT,levels);
            slot.allConfidence=backend_.createImageView(slot.confidence.image,confidenceFormat_,VK_IMAGE_ASPECT_COLOR_BIT,levels);
            for(unsigned mip=0;mip<levels;++mip){
                auto& target=slot.mips[mip];target.color=backend_.createImageView(slot.color.image,colorFormat_,VK_IMAGE_ASPECT_COLOR_BIT,1,mip);
                target.confidence=backend_.createImageView(slot.confidence.image,confidenceFormat_,VK_IMAGE_ASPECT_COLOR_BIT,1,mip);
                if(mip){target.set=sets.at(frame*(levels-1)+mip-1);
                    target.parameters=backend_.allocator().createBuffer(sizeof(ReflectionFilterFrame),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
                    backend_.writeDescriptorBuffer({target.set,5,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,target.parameters.buffer,target.parameters.size});}
            }
        }
    }catch(...){release();throw;}
}
ReflectionSignalPyramidGpu::~ReflectionSignalPyramidGpu(){release();}
void ReflectionSignalPyramidGpu::release() noexcept {
    if(pool_)backend_.destroyDescriptorPool(pool_);pool_=VK_NULL_HANDLE;
    for(auto& slot:slots_){for(auto& mip:slot.mips){if(mip.color)backend_.destroyImageView(mip.color);if(mip.confidence)backend_.destroyImageView(mip.confidence);backend_.allocator().destroyBuffer(mip.parameters);}
        if(slot.allColor)backend_.destroyImageView(slot.allColor);if(slot.allConfidence)backend_.destroyImageView(slot.allConfidence);
        backend_.allocator().destroyImage(slot.color);backend_.allocator().destroyImage(slot.confidence);}
    slots_.clear();if(pipeline_)backend_.destroyPipeline(pipeline_);pipeline_=VK_NULL_HANDLE;
    if(shader_)backend_.destroyShaderModule(shader_);shader_=VK_NULL_HANDLE;if(layout_)backend_.destroyPipelineLayout(layout_);layout_=VK_NULL_HANDLE;
    if(setLayout_)backend_.destroyDescriptorSetLayout(setLayout_);setLayout_=VK_NULL_HANDLE;if(point_)backend_.destroySampler(point_);point_=VK_NULL_HANDLE;
}
ReflectionPyramidViews ReflectionSignalPyramidGpu::base(unsigned index) const {
    const auto& slot=slots_.at(index);return {slot.color.image,slot.confidence.image,slot.mips.front().color,slot.mips.front().confidence,
        colorFormat_,confidenceFormat_,extent_.width,extent_.height,1};
}
ReflectionPyramidViews ReflectionSignalPyramidGpu::output(unsigned index) const {
    const auto& slot=slots_.at(index);return {slot.color.image,slot.confidence.image,slot.allColor,slot.allConfidence,
        colorFormat_,confidenceFormat_,extent_.width,extent_.height,levels_};
}
void ReflectionSignalPyramidGpu::beginBaseWrite(rhi::ICommandRecorder& commands,unsigned index){
    auto& slot=slots_.at(index);if(slot.writing)throw std::logic_error("Reflection pyramid base write is already active");
    for(const auto image:{slot.color.image,slot.confidence.image})commands.imageBarrier({image,VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,levels_,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT});
    slot.writing=true;
}
void ReflectionSignalPyramidGpu::abandonBaseWrite(unsigned index){slots_.at(index).writing=false;}
void ReflectionSignalPyramidGpu::record(rhi::ICommandRecorder& commands,unsigned index,VkImageView depth,unsigned depthLevels,const ReflectionPyramidParameters& parameters){
    auto& slot=slots_.at(index);if(!slot.writing)throw std::logic_error("Reflection pyramid requires a completed base-write producer");
    if(!depth)throw std::invalid_argument("Reflection pyramid requires a depth view");
    for(const auto& candidate:slots_){if(depth==candidate.allColor||depth==candidate.allConfidence)throw std::invalid_argument("Reflection depth cannot alias signal outputs");
        for(const auto& mip:candidate.mips)if(depth==mip.color||depth==mip.confidence)throw std::invalid_argument("Reflection depth cannot alias signal mips");}
    (void)filterParameters(parameters,levels_-1).frame(extent_.width,extent_.height,depthLevels,levels_);
    for(unsigned mip=1;mip<levels_;++mip){
        auto& target=slot.mips[mip];const auto& previous=slot.mips[mip-1];const auto frame=parameters.frame(extent_.width,extent_.height,mip,depthLevels);
        std::memcpy(target.parameters.mapped,&frame,sizeof(frame));backend_.allocator().flush(target.parameters,0,sizeof(frame));
        const VkImageView views[]={previous.color,target.color,previous.confidence,target.confidence};
        for(unsigned binding=0;binding<4;++binding)backend_.writeDescriptorImage({target.set,binding,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,views[binding],VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});
        backend_.writeDescriptorImage({target.set,4,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,depth,point_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }
    for(unsigned mip=1;mip<levels_;++mip){
        for(const auto image:{slot.color.image,slot.confidence.image})commands.imageBarrier({image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,1,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_COLOR_BIT,mip-1});
        ComputePass({std::max(1U,extent_.width>>mip),std::max(1U,extent_.height>>mip),8,8,1,true}).record(commands,pipeline_,layout_,slot.mips[mip].set);
    }
    for(const auto image:{slot.color.image,slot.confidence.image})commands.imageBarrier({image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,levels_,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    slot.writing=false;
}
}
