#include "render/ProjectedScreenReflectionGpu.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
namespace {
struct Profile {VkExtent2D extent;unsigned levels;std::uint64_t bytes;};
void require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
Profile profile(VkExtent2D source,unsigned frames,unsigned maximumLevels,unsigned divisor){
    require(source.width&&source.height&&source.width<=8192&&source.height<=8192&&frames>=1&&frames<=8&&maximumLevels>=1&&maximumLevels<=14&&(divisor==1||divisor==2),"Invalid projected composition extent, slots, levels or divisor");
    Profile result{{std::max(1U,source.width/divisor),std::max(1U,source.height/divisor)},1,0};
    for(auto side=std::max(result.extent.width,result.extent.height);side>1;side/=2)++result.levels;result.levels=std::min(result.levels,maximumLevels);
    const auto base=std::uint64_t(result.extent.width)*result.extent.height;
    require(base*frames*ProjectedReflectionPreparationGpu::kBytesPerPixel<=ProjectedReflectionPreparationGpu::kMaximumBytes,"Projected composition preparation exceeds budget");
    require(std::uint64_t((result.extent.width+7)/8)*((result.extent.height+7)/8)<=65535,"Projected composition exceeds 65535 tiles");
    std::uint64_t pixels=0;for(unsigned mip=0;mip<result.levels;++mip)pixels+=std::uint64_t(std::max(1U,result.extent.width>>mip))*std::max(1U,result.extent.height>>mip);
    result.bytes=(pixels*9+base*41)*frames+16;require(result.bytes<=512ULL*1024*1024,"Projected composition exceeds 512 MiB");return result;
}
}
bool ProjectedScreenReflectionGpu::supported(rhi::IRhi& backend,VkExtent2D source,unsigned frames,unsigned maximumLevels,unsigned divisor){
    try{(void)profile(source,frames,maximumLevels,divisor);}catch(const std::invalid_argument&){return false;}
    constexpr auto usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    for(auto format:{VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R8_UNORM,VK_FORMAT_R16G16_UNORM,VK_FORMAT_B10G11R11_UFLOAT_PACK32}){
        const bool linear=format==VK_FORMAT_R8_UNORM||format==VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        if(!backend.supportsStorageImage(format)||!backend.supportsSampledImage(format,linear)||!(backend.imageSampleCounts(format,usage)&VK_SAMPLE_COUNT_1_BIT))return false;
    }
    return true;
}
ProjectedScreenReflectionShaders ProjectedScreenReflectionShaders::load(const std::string& directory){
    return {readBinaryFile(directory+"/projected_reflection_prepare.comp.spv"),readBinaryFile(directory+"/projected_reflection_trace.comp.spv"),
        readBinaryFile(directory+"/depth_pyramid.comp.spv"),readBinaryFile(directory+"/reflection_spatial_filter.comp.spv"),
        {readBinaryFile(directory+"/reflection_signal_pyramid_packed.comp.spv"),readBinaryFile(directory+"/reflection_signal_pyramid_float.comp.spv")}};
}
ProjectedScreenReflectionGpu::ProjectedScreenReflectionGpu(rhi::IRhi& backend,VkExtent2D source,unsigned frames,const ProjectedScreenReflectionShaders& shaders,unsigned maximumLevels,unsigned divisor)
    :backend_(backend),source_(source),frames_(frames){
    const auto properties=profile(source,frames,maximumLevels,divisor);extent_=properties.extent;levels_=properties.levels;imageBytes_=properties.bytes;
    if(!supported(backend,source,frames,maximumLevels,divisor))throw std::runtime_error("Projected composition sampled storage formats unavailable");
    prepare_=std::make_unique<ProjectedReflectionPreparationGpu>(backend,source,frames,shaders.prepare,divisor);
    ray_=std::make_unique<ProjectedReflectionRayGpu>(backend,frames,shaders.ray);
    depth_=std::make_unique<DepthPyramidGpu>(backend,extent_,levels_,frames,shaders.depth);
    signals_=std::make_unique<ReflectionSignalPyramidGpu>(backend,extent_,levels_,frames,ReflectionPyramidPrecision::RequirePacked,shaders.signal);
    filter_=std::make_unique<ReflectionSpatialFilterGpu>(backend,extent_,frames,shaders.filter,ReflectionFilterOutputAccess::StorageRead);
    rhi::GpuBuffer staging{};
    try{
        staging=backend.allocator().createBuffer(16,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);std::memset(staging.mapped,0,16);backend.allocator().flush(staging,0,16);
        neutral_=backend.allocator().createImage2D(1,1,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        backend.transitionImageLayout(neutral_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);backend.copyBufferToImage(staging,neutral_,1,1,1,16);
        backend.executeOneShot([&](rhi::ICommandRecorder& commands){commands.imageBarrier({neutral_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});});
        backend.allocator().destroyBuffer(staging);neutralView_=backend.createImageView(neutral_.image,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
    }catch(...){backend.allocator().destroyBuffer(staging);if(neutralView_)backend.destroyImageView(neutralView_);backend.allocator().destroyImage(neutral_);throw;}
}
ProjectedScreenReflectionGpu::~ProjectedScreenReflectionGpu(){if(neutralView_)backend_.destroyImageView(neutralView_);backend_.allocator().destroyImage(neutral_);}
void ProjectedScreenReflectionGpu::record(rhi::ICommandRecorder& commands,unsigned slot,const ProjectedScreenReflectionInputs& inputs,const ReflectionTraceView& camera,
    const ProjectedScreenReflectionParameters& parameters,const ProjectedScreenReflectionHistory* history){
    require(slot<frames_&&inputs.surface&&inputs.color&&inputs.extent.width==source_.width&&inputs.extent.height==source_.height,"Projected composition requires matching complete snapshot inputs");
    if(history)require(history->slot<frames_&&history->slot!=slot&&history->color&&inputs.motion,"Projected history requires another submitted slot, color and motion");
    const auto view=projectedReflectionView(camera,history?&history->view:nullptr);
    require(view.depthReciprocal[1]>0,"Projected composition requires a finite far plane");
    ProjectedReflectionSettings ray;ray.maxSteps=parameters.maxSteps;ray.jitterSeed=parameters.jitterSeed;ray.edgeFade=parameters.edgeFade;
    ray.motionEncoding=ProjectedReflectionMotionEncoding::LinearUv;ray.motionBlend=history?0.F:1.F;ray.maximumFootprintLevel=float(std::max(1U,levels_-1));
    (void)view.pack(source_,extent_,ray);
    const ProjectedReflectionPreparationParameters surface{parameters.maxRoughness,parameters.strength};(void)surface.frame(source_,extent_,view.depthReciprocal);
    for(unsigned i=0;i<frames_;++i){const auto p=prepare_->output(i);const auto base=signals_->base(i),all=signals_->output(i);
        for(const auto own:{p.depth,p.normalRoughness,p.mask,p.footprint,base.color,base.confidence,all.color,all.confidence,depth_->output(i).view,filter_->output(i).view,neutralView_})
            for(const auto borrowed:{inputs.surface,inputs.color,inputs.motion,history?history->color:VK_NULL_HANDLE})
                require(!borrowed||borrowed!=own,"Projected composition snapshot or history aliases owned resources");
    }
    const auto p=prepare_->output(slot);
    try{
        signals_->beginBaseWrite(commands,slot);const auto base=signals_->base(slot);
        prepare_->record(commands,slot,inputs.surface,{base.confidenceImage,base.confidence,extent_},view.depthReciprocal,surface);
        ProjectedReflectionInputs sampled;sampled.previousColor=history?history->color:inputs.color;sampled.currentDepth=p.depth;
        sampled.previousDepth=history?prepare_->output(history->slot).depth:p.depth;sampled.motion=inputs.motion?inputs.motion:neutralView_;
        sampled.surfaceMask=p.mask;sampled.normalRoughness=p.normalRoughness;sampled.sourceExtent=source_;sampled.traceExtent=extent_;sampled.tiles=p.tiles;sampled.tileBytes=p.tileBytes;sampled.tileCount=p.tileCount;
        ray_->record(commands,slot,sampled,{base.color,p.footprint,base.confidence,extent_},view,ray);
        commands.imageBarrier({p.footprintImage,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        // Prepared depth is finite, including invalid/background texels. Reverse depth reduces by maximum.
        depth_->record(commands,slot,p.depth,DepthReduction::Maximum);const auto depth=depth_->output(slot);
        ReflectionPyramidParameters pyramid;pyramid.depthDecode=view.depthReciprocal;pyramid.reciprocalDepth=true;
        signals_->record(commands,slot,depth.view,levels_,pyramid);const auto signal=signals_->output(slot);
        ReflectionFilterInputViews filtered{depth.view,p.footprint,signal.color,extent_.width,extent_.height,levels_,levels_};
        filtered.confidence=signal.confidence;filtered.confidenceLevels=levels_;filtered.roughnessChannel=1;
        ReflectionFilterParameters filter;filter.maxLod=float(levels_-1);filter.depthDecode=view.depthReciprocal;filter.reciprocalDepth=true;
        filter_->record(commands,slot,filtered,filter);
    }catch(...){signals_->abandonBaseWrite(slot);throw;}
}
}
