#include "render/ScreenReflectionGpu.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <utility>
namespace azurerender {
namespace {
std::pair<unsigned,std::uint64_t> profile(VkExtent2D extent,unsigned frames,unsigned maximumLevels){
    if(!extent.width||!extent.height||extent.width>8192||extent.height>8192||!frames||frames>8||!maximumLevels||maximumLevels>14)
        throw std::invalid_argument("Screen reflection requires bounded dimensions, frames and levels");
    unsigned full=1;for(auto side=std::max(extent.width,extent.height);side>1;side/=2)++full;
    const auto levels=std::min(full,maximumLevels);std::uint64_t pixels=0;
    for(unsigned mip=0;mip<levels;++mip)pixels+=std::uint64_t(std::max(1U,extent.width>>mip))*std::max(1U,extent.height>>mip);
    const auto bytes=(pixels*24+std::uint64_t(extent.width)*extent.height*16)*frames;
    if(bytes>512ULL*1024*1024)throw std::invalid_argument("Combined reflection resources exceed 512 MiB");
    return {levels,bytes};
}
}
bool ScreenReflectionGpu::supported(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,unsigned maximumLevels,ReflectionPyramidPrecision precision){
    try{(void)profile(extent,frames,maximumLevels);}catch(const std::invalid_argument&){return false;}
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
        {readBinaryFile(directory+"/reflection_signal_pyramid_packed.comp.spv"),readBinaryFile(directory+"/reflection_signal_pyramid_float.comp.spv")}};
}
ScreenReflectionGpu::ScreenReflectionGpu(rhi::IRhi& backend,VkExtent2D extent,unsigned frames,const ScreenReflectionShaders& shaders,unsigned maximumLevels,ReflectionPyramidPrecision precision)
    :extent_(extent),frames_(frames){
    const auto properties=profile(extent,frames,maximumLevels);levels_=properties.first;imageBytes_=properties.second;
    if(precision!=ReflectionPyramidPrecision::PreferPacked&&precision!=ReflectionPyramidPrecision::RequirePacked&&precision!=ReflectionPyramidPrecision::Float32)throw std::invalid_argument("Invalid reflection precision");
    depth_=std::make_unique<DepthPyramidGpu>(backend,extent,levels_,frames,shaders.depth);
    signals_=std::make_unique<ReflectionSignalPyramidGpu>(backend,extent,levels_,frames,precision,shaders.signal);
    const auto formats=signals_->base(0);
    rays_=std::make_unique<ReflectionRayGpu>(backend,extent,frames,formats.colorFormat,formats.confidenceFormat,shaders.ray);
    filter_=std::make_unique<ReflectionSpatialFilterGpu>(backend,extent,frames,shaders.filter,ReflectionFilterOutputAccess::StorageRead);
}
void ScreenReflectionGpu::record(rhi::ICommandRecorder& commands,unsigned slot,const ReflectionTraceInputs& inputs,const ReflectionTraceView& view,const ScreenSpaceReflectionSettings& settings){
    if(slot>=frames_||!inputs.surface||!inputs.color||inputs.width!=extent_.width||inputs.height!=extent_.height)
        throw std::invalid_argument("Screen reflection input or slot mismatch");
    for(unsigned i=0;i<frames_;++i)if(inputs.surface==output(i).view||inputs.color==output(i).view)throw std::invalid_argument("Screen reflection inputs alias its outputs");
    (void)view.pack(extent_.width,extent_.height,settings,ReflectionColorEncoding::MaxComponent);
    depth_->record(commands,slot,inputs.surface,DepthReduction::Minimum);
    signals_->beginBaseWrite(commands,slot);
    rays_->record(commands,slot,inputs,signals_->base(slot),view,settings);
    const auto depth=depth_->output(slot);
    signals_->record(commands,slot,depth.view,levels_,{});const auto signal=signals_->output(slot);
    ReflectionFilterInputViews filtered{depth.view,inputs.surface,signal.color,extent_.width,extent_.height,levels_,levels_};
    filtered.confidence=signal.confidence;filtered.confidenceLevels=levels_;filtered.roughnessChannel=3;
    ReflectionFilterParameters parameters;parameters.maxLod=float(levels_-1);
    filter_->record(commands,slot,filtered,parameters);
}
}
