#include "SurfaceRadianceCache.hpp"
#include <stdexcept>
#include <algorithm>
namespace azurerender {
namespace {constexpr VkImageUsageFlags usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;}
bool SurfaceRadianceCache::supported(rhi::IRhi& backend,VkExtent2D extent,unsigned slots,std::uint64_t byteLimit){
    if(!extent.width||!extent.height||!slots||slots>8)return false;
    const auto pixels=std::uint64_t(extent.width)*extent.height;
    if(pixels>byteLimit/(64ULL*slots))return false;
    for(auto format:{VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32G32B32A32_UINT})
        if(!backend.supportsSampledImage(format,false)||!(backend.imageSampleCounts(format,usage)&VK_SAMPLE_COUNT_1_BIT))return false;
    return true;
}
SurfaceRadianceCache::SurfaceRadianceCache(rhi::IRhi& backend,VkExtent2D extent,unsigned slots,std::uint64_t byteLimit,bool diagnostics,bool selection,unsigned drawCapacity,unsigned diagnosticRecordCapacity):rhi_(backend),diagnosticRecordCapacity_(diagnosticRecordCapacity),drawCapacity_(drawCapacity){
    if(diagnosticRecordCapacity>16||(!diagnostics&&diagnosticRecordCapacity))throw std::invalid_argument("Capture requires diagnostics and at most sixteen records");
    statisticsFrameBytes_=diagnosticRecordCapacity?4ULL*(7+diagnosticRecordCapacity*12):24;
    diagnosticRecords_.reserve(diagnosticRecordCapacity);
    if(drawCapacity>65536||(!selection&&drawCapacity))throw std::invalid_argument("Draw predicates require selection and a capacity within 65536");
    if(!supported(backend,extent,slots,byteLimit))throw std::invalid_argument("Surface radiance cache exceeds format or byte capabilities");
    try{
        textures_.resize(slots);
        for(auto& frame:textures_)for(unsigned kind=0;kind<kAttachments;++kind){auto& t=frame[kind];
            t.image=backend.allocator().createImage2D(extent.width,extent.height,format(kind),usage);
            t.view=backend.createImageView(t.image.image,format(kind),VK_IMAGE_ASPECT_COLOR_BIT,1);
        }
        rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;sampler.mipmapLinear=false;
        sampler.addressU=sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler_=backend.createSampler(sampler);
        bytes_=std::uint64_t(extent.width)*extent.height*64*slots;
        if(diagnostics){
            statisticsFrames_.resize(slots);
            for(auto& frame:statisticsFrames_){
                frame.buffer=backend.allocator().createBuffer(statisticsFrameBytes_,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
                if(!frame.buffer.mapped)throw std::runtime_error("Radiance statistics require mapped frame storage");
                std::memset(frame.buffer.mapped,0,static_cast<std::size_t>(frame.buffer.size));backend.allocator().flush(frame.buffer,0,frame.buffer.size);
            }
        }
        if(selection){
            selectionFrames_.resize(slots);const std::uint32_t fallback=1;
            for(auto& frame:selectionFrames_){
                frame.buffer=backend.allocator().createBuffer(4ULL*(drawCapacity?drawCapacity+6:1),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
                if(!frame.buffer.mapped)throw std::runtime_error("Radiance selection requires mapped frame predicates");
                std::memset(frame.buffer.mapped,0,static_cast<std::size_t>(frame.buffer.size));
                std::memcpy(frame.buffer.mapped,&fallback,4);backend.allocator().flush(frame.buffer,0,frame.buffer.size);
            }
        }
    }catch(...){clear();throw;}
}
SurfaceRadianceCache::~SurfaceRadianceCache(){clear();}
void SurfaceRadianceCache::beginStatisticsFrame(unsigned slot){
    if(!statisticsEnabled())return;
    auto& frame=statisticsFrames_.at(slot);
    if(frame.recorded){
        rhi_.allocator().invalidate(frame.buffer,0,frame.buffer.size);std::array<std::uint32_t,6> words{};
        std::memcpy(words.data(),frame.buffer.mapped,24);
        if(words[5]&2U){
            statistics_.producerStores+=words[0];statistics_.consumerVisits+=words[1];statistics_.eligibleRequests+=words[2];
            statistics_.pixelMatches+=words[3];statistics_.quadMatches+=words[4];++statistics_.frames;
            statistics_.overflow|=(words[5]&1U)!=0;
            if(diagnosticRecordCapacity_){
                std::memcpy(&lastDiagnosticAttempts_,static_cast<const char*>(frame.buffer.mapped)+24,4);
                diagnosticRecords_.resize(std::min(lastDiagnosticAttempts_,diagnosticRecordCapacity_));
                if(!diagnosticRecords_.empty())std::memcpy(diagnosticRecords_.data(),static_cast<const char*>(frame.buffer.mapped)+28,diagnosticRecords_.size()*48);
            }
        }
    }
    std::memset(frame.buffer.mapped,0,static_cast<std::size_t>(frame.buffer.size));rhi_.allocator().flush(frame.buffer,0,frame.buffer.size);frame.recorded=false;
}
void SurfaceRadianceCache::recordStatisticsReady(rhi::ICommandRecorder& commands,unsigned slot){
    if(!statisticsEnabled())return;
    auto& frame=statisticsFrames_.at(slot);
    commands.bufferBarrier({frame.buffer.buffer,0,frame.buffer.size,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});frame.recorded=true;
}
void SurfaceRadianceCache::beginSelectionFrame(unsigned slot,bool permitted,unsigned drawCount){
    if(drawCount>drawCapacity_)throw std::length_error("Selection draw count exceeds predicate capacity");
    if(!selectionEnabled())return;auto& frame=selectionFrames_.at(slot);
    if(frame.recorded){
        rhi_.allocator().invalidate(frame.buffer,0,drawCapacity_?24:4);std::uint32_t fallback=1;std::memcpy(&fallback,frame.buffer.mapped,4);
        if(drawCapacity_&&(fallback&2U)){std::array<std::uint32_t,6> words{};std::memcpy(words.data(),frame.buffer.mapped,24);
            selectionStatistics_.lastGlobalReasons=fallback&~3U;selectionStatistics_.lastFastDraws=words[2];selectionStatistics_.lastFullDraws=words[3];
            selectionStatistics_.fastDraws+=words[2];selectionStatistics_.fullDraws+=words[3];
            selectionStatistics_.lastFastWitnesses=words[4];selectionStatistics_.lastFullWitnesses=words[5];
            selectionStatistics_.fastWitnesses+=words[4];selectionStatistics_.fullWitnesses+=words[5];}
        if(fallback&2U){++selectionStatistics_.frames;selectionStatistics_.lastFallback=(fallback&1U)!=0;
            if(fallback&1U)++selectionStatistics_.fullFrames;else ++selectionStatistics_.fastFrames;}
    }
    const std::uint32_t fallback=permitted?0U:1U;
    const auto resetBytes=drawCapacity_?4ULL*(6+drawCount):4ULL;
    std::memset(frame.buffer.mapped,0,static_cast<std::size_t>(resetBytes));std::memcpy(frame.buffer.mapped,&fallback,4);
    if(drawCapacity_)std::memcpy(static_cast<char*>(frame.buffer.mapped)+4,&drawCount,4);
    rhi_.allocator().flush(frame.buffer,0,resetBytes);frame.recorded=false;
}
void SurfaceRadianceCache::recordSelectionReady(rhi::ICommandRecorder& commands,unsigned slot){
    if(!selectionEnabled())return;auto& frame=selectionFrames_.at(slot);
    commands.bufferBarrier({frame.buffer.buffer,0,drawCapacity_?24ULL:4ULL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});frame.recorded=true;
}
void SurfaceRadianceCache::clear() noexcept {
    for(auto& frame:selectionFrames_)rhi_.allocator().destroyBuffer(frame.buffer);
    selectionFrames_.clear();selectionStatistics_={};
    for(auto& frame:statisticsFrames_)rhi_.allocator().destroyBuffer(frame.buffer);
    statisticsFrames_.clear();statistics_={};diagnosticRecords_.clear();lastDiagnosticAttempts_=0;
    if(sampler_)rhi_.destroySampler(sampler_);sampler_=VK_NULL_HANDLE;
    for(auto& frame:textures_)for(auto& texture:frame){if(texture.view)rhi_.destroyImageView(texture.view);rhi_.allocator().destroyImage(texture.image);texture={};}
    textures_.clear();bytes_=0;
}
}
