#include "render/ScreenReflectionGpu.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <utility>
namespace azurerender {
namespace {
VkExtent2D traceExtent(VkExtent2D source,unsigned divisor){
    if(!source.width||!source.height||source.width>8192||source.height>8192||(divisor!=1&&divisor!=2))
        throw std::invalid_argument("Reflection source requires bounded dimensions and a resolution divisor of one or two");
    return {std::max(1U,source.width/divisor),std::max(1U,source.height/divisor)};
}
std::pair<unsigned,std::uint64_t> profile(VkExtent2D extent,unsigned frames,unsigned maximumLevels,bool temporal){
    if(!extent.width||!extent.height||extent.width>8192||extent.height>8192||!frames||frames>8||!maximumLevels||maximumLevels>14)
        throw std::invalid_argument("Screen reflection requires bounded dimensions, frames and levels");
    unsigned full=1;for(auto side=std::max(extent.width,extent.height);side>1;side/=2)++full;
    const auto levels=std::min(full,maximumLevels);std::uint64_t pixels=0;
    for(unsigned mip=0;mip<levels;++mip)pixels+=std::uint64_t(std::max(1U,extent.width>>mip))*std::max(1U,extent.height>>mip);
    const auto bytes=(pixels*24+std::uint64_t(extent.width)*extent.height*(temporal?36:16))*frames;
    if(bytes>512ULL*1024*1024)throw std::invalid_argument("Combined reflection resources exceed 512 MiB");
    return {levels,bytes};
}
}
bool ScreenReflectionGpu::supported(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,unsigned maximumLevels,ReflectionPyramidPrecision precision,unsigned resolutionDivisor,bool temporal){
    try{(void)profile(traceExtent(extent,resolutionDivisor),frames,maximumLevels,temporal);}catch(const std::invalid_argument&){return false;}
    const auto signal=[&](VkFormat format){return backend.supportsStorageImage(format)&&backend.supportsSampledImage(format,true)
        &&(backend.imageSampleCounts(format,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT)&VK_SAMPLE_COUNT_1_BIT);};
    const auto common=[&](VkFormat format){return backend.supportsStorageImage(format)&&backend.supportsSampledImage(format,false)
        &&(backend.imageSampleCounts(format,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT)&VK_SAMPLE_COUNT_1_BIT);};
    if(!common(VK_FORMAT_R32_SFLOAT)||!common(VK_FORMAT_R32G32B32A32_SFLOAT))return false;
    const bool packed=signal(VK_FORMAT_B10G11R11_UFLOAT_PACK32)&&signal(VK_FORMAT_R8_UNORM);
    const bool wide=signal(VK_FORMAT_R32G32B32A32_SFLOAT)&&signal(VK_FORMAT_R32_SFLOAT);
    return precision==ReflectionPyramidPrecision::PreferPacked?(packed||wide):precision==ReflectionPyramidPrecision::RequirePacked?packed:precision==ReflectionPyramidPrecision::Float32?wide:false;
}

ScreenReflectionShaders ScreenReflectionShaders::load(const std::string& directory){
    return {readBinaryFile(directory+"/depth_pyramid.comp.spv"),readBinaryFile(directory+"/reflection_spatial_filter.comp.spv"),
        {readBinaryFile(directory+"/reflection_trace_packed.comp.spv"),readBinaryFile(directory+"/reflection_trace_float.comp.spv")},
        {readBinaryFile(directory+"/reflection_signal_pyramid_packed.comp.spv"),readBinaryFile(directory+"/reflection_signal_pyramid_float.comp.spv")},
        {readBinaryFile(directory+"/reflection_temporal_packed.comp.spv"),readBinaryFile(directory+"/reflection_temporal_float.comp.spv")}};
}
ScreenReflectionGpu::ScreenReflectionGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,const ScreenReflectionShaders& shaders,unsigned maximumLevels,ReflectionPyramidPrecision precision,unsigned resolutionDivisor,bool temporal)
    :extent_(traceExtent(extent,resolutionDivisor)),sourceExtent_(extent),frames_(frames){
    const auto properties=profile(extent_,frames,maximumLevels,temporal);levels_=properties.first;imageBytes_=properties.second;
    if(precision!=ReflectionPyramidPrecision::PreferPacked&&precision!=ReflectionPyramidPrecision::RequirePacked&&precision!=ReflectionPyramidPrecision::Float32)throw std::invalid_argument("Invalid reflection precision");
    depth_=std::make_unique<DepthPyramidGpu>(backend,extent_,levels_,frames,shaders.depth);
    signals_=std::make_unique<ReflectionSignalPyramidGpu>(backend,extent_,levels_,frames,precision,shaders.signal);
    const auto formats=signals_->base(0);
    rays_=std::make_unique<ReflectionRayGpu>(backend,extent_,frames,formats.colorFormat,formats.confidenceFormat,shaders.ray);
    filter_=std::make_unique<ReflectionSpatialFilterGpu>(backend,extent_,frames,shaders.filter,ReflectionFilterOutputAccess::StorageRead);
    if(temporal){
        current_=std::make_unique<ReflectionSignalPyramidGpu>(backend,extent_,1,frames,precision,shaders.signal);
        temporal_=std::make_unique<ReflectionTemporalGpu>(backend,extent_,frames,formats.colorFormat,formats.confidenceFormat,shaders.temporal);
    }
}
void ScreenReflectionGpu::record(rhi::ICommandRecorder& commands,unsigned slot,const ReflectionTraceInputs& inputs,const ReflectionTraceView& view,const ScreenSpaceReflectionSettings& settings,const ScreenReflectionHistoryInputs* history){
    if(slot>=frames_||!inputs.surface||!inputs.color||inputs.width!=sourceExtent_.width||inputs.height!=sourceExtent_.height)
        throw std::invalid_argument("Screen reflection input or slot mismatch");
    for(unsigned i=0;i<frames_;++i){
        const auto signal=signals_->base(i);
        for(auto own:{output(i).view,signal.color,signal.confidence})
            if(inputs.surface==own||inputs.color==own)throw std::invalid_argument("Screen reflection inputs alias its outputs");
    }
    (void)view.pack(extent_.width,extent_.height,settings,ReflectionColorEncoding::MaxComponent);
    if(history){
        if(!temporal_)throw std::invalid_argument("Temporal reflection resources were not requested");
        (void)history->parameters.frame(extent_.width,extent_.height);
        if(!history->motion||!history->acceptance||(history->parameters.historyValid&&(!history->previousColor||!history->previousConfidence)))
            throw std::invalid_argument("Temporal reflection requires motion, acceptance and valid history views");
        const auto target=signals_->base(slot);
        for(auto source:{history->motion,history->acceptance,history->previousColor,history->previousConfidence})
            if(source&&(source==target.color||source==target.confidence))throw std::invalid_argument("Temporal reflection history aliases its current output");
    }
    depth_->record(commands,slot,inputs.surface,DepthReduction::Minimum,sourceExtent_);
    const auto depth=depth_->output(slot);
    if(history){
        current_->beginBaseWrite(commands,slot);
        const auto raw=current_->base(slot);
        rays_->record(commands,slot,inputs,raw,view,settings);
        current_->record(commands,slot,depth.view,1,{});
        const ReflectionTemporalInputs temporalInputs{raw.color,raw.confidence,
            history->parameters.historyValid?history->previousColor:raw.color,
            history->parameters.historyValid?history->previousConfidence:raw.confidence,
            history->motion,history->acceptance,extent_.width,extent_.height};
        signals_->beginBaseWrite(commands,slot);
        temporal_->record(commands,slot,temporalInputs,signals_->base(slot),history->parameters);
    }else{
        signals_->beginBaseWrite(commands,slot);
        rays_->record(commands,slot,inputs,signals_->base(slot),view,settings);
    }
    signals_->record(commands,slot,depth.view,levels_,{});const auto signal=signals_->output(slot);
    ReflectionFilterInputViews filtered{depth.view,inputs.surface,signal.color,extent_.width,extent_.height,levels_,levels_};
    filtered.confidence=signal.confidence;filtered.confidenceLevels=levels_;filtered.roughnessChannel=3;
    ReflectionFilterParameters parameters;parameters.maxLod=float(levels_-1);
    filter_->record(commands,slot,filtered,parameters);
}
}
