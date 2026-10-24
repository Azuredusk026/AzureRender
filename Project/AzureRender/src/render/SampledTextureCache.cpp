#include "render/SampledTextureCache.hpp"

#include <cmath>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace azurerender {
namespace {
std::uint32_t contentBucket(const std::vector<std::uint8_t>& pixels) {
    // This is only a bucket selector. Exact bytes and every effective upload
    // parameter are compared before a binding can be reused.
    std::uint32_t hash=2166136261U;
    // The extent is already validated. Pointer traversal avoids checked
    // iterator bookkeeping for each byte in development configurations.
    const auto* end=pixels.data()+pixels.size();
    for(const auto* at=pixels.data();at!=end;++at)hash=(hash^*at)*16777619U;
    return hash;
}
struct TimedSection {
    double& elapsed;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    ~TimedSection() {elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
};
bool sameSampler(const rhi::SamplerDesc& a,const rhi::SamplerDesc& b) {
    return a.filter==b.filter&&a.addressU==b.addressU&&a.addressV==b.addressV
        &&a.addressW==b.addressW&&a.maxLod==b.maxLod
        &&a.mipmapLinear==b.mipmapLinear&&a.maxAnisotropy==b.maxAnisotropy&&a.mipLodBias==b.mipLodBias;
}
bool sameUpload(const SampledTextureUpload& a,const SampledTextureUpload& b) {
    return a.width==b.width&&a.height==b.height&&a.format==b.format
        &&a.mipLevels==b.mipLevels&&a.semantic==b.semantic
        &&a.alphaCutoff==b.alphaCutoff&&sameSampler(a.sampler,b.sampler);
}
std::uint64_t byteCount(unsigned width,unsigned height,unsigned levels) {
    std::uint64_t total=0;
    for(unsigned i=0;i<levels;++i) {
        const auto bytes=std::uint64_t(width)*height*4;
        if(total>std::numeric_limits<std::uint64_t>::max()-bytes)
            throw std::invalid_argument("Texture mip extent overflows its byte budget");
        total+=bytes;width=std::max(1U,width/2);height=std::max(1U,height/2);
    }
    return total;
}
struct StagingBuffer {
    rhi::IGpuAllocator& allocator;
    rhi::GpuBuffer value;
    ~StagingBuffer() { allocator.destroyBuffer(value); }
};
}

struct SampledTextureCache::Entry {
    rhi::IGpuAllocator& allocator;
    rhi::IRhi& rhi;
    SampledTextureUpload upload;
    std::vector<std::uint8_t> pixels;
    rhi::GpuImage image;
    VkImageView view=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;

    Entry(rhi::IGpuAllocator& a,rhi::IRhi& r):allocator(a),rhi(r) {}
    ~Entry() {
        if(sampler)rhi.destroySampler(sampler);
        if(view)rhi.destroyImageView(view);
        allocator.destroyImage(image);
    }
};

SampledTextureCache::SampledTextureCache(rhi::IGpuAllocator& allocator, rhi::IRhi& rhi)
    : allocator_(allocator), rhi_(rhi) {}
SampledTextureCache::~SampledTextureCache() { clear(); }

SampledTextureBinding SampledTextureCache::acquire(const std::vector<std::uint8_t>& pixels,
                                                 const SampledTextureUpload& request) {
    return acquireImpl(pixels,request,nullptr);
}
SampledTextureBinding SampledTextureCache::acquire(std::vector<std::uint8_t>&& pixels,
                                                 const SampledTextureUpload& request) {
    return acquireImpl(pixels,request,&pixels);
}
SampledTextureBinding SampledTextureCache::acquireImpl(const std::vector<std::uint8_t>& pixels,
                                                     const SampledTextureUpload& request,
                                                     std::vector<std::uint8_t>* consumed) {
    if(!request.width||!request.height
        ||request.width>std::numeric_limits<std::size_t>::max()/request.height/4)
        throw std::invalid_argument("RGBA8 texture extent is invalid");
    if(request.format!=VK_FORMAT_R8G8B8A8_UNORM&&request.format!=VK_FORMAT_R8G8B8A8_SRGB)
        throw std::invalid_argument("Sampled texture cache requires an RGBA8 format");
    if(!std::isfinite(request.alphaCutoff)||request.alphaCutoff<0||request.alphaCutoff>1)
        throw std::invalid_argument("Texture alpha cutoff is invalid");
    if(!std::isfinite(request.sampler.maxLod)||request.sampler.maxLod<0
        ||!std::isfinite(request.sampler.mipLodBias)
        ||!std::isfinite(request.sampler.maxAnisotropy)||request.sampler.maxAnisotropy<1
        ||(request.sampler.filter!=VK_FILTER_NEAREST&&request.sampler.filter!=VK_FILTER_LINEAR))
        throw std::invalid_argument("Texture sampler settings are invalid");
    switch(request.semantic) {
    case TextureFilterSemantic::Linear:case TextureFilterSemantic::Srgb:
    case TextureFilterSemantic::Normal:case TextureFilterSemantic::PackedNormal:break;
    default:throw std::invalid_argument("Texture filter semantic is invalid");
    }
    unsigned fullLevels=1;
    for(unsigned w=request.width,h=request.height;w>1||h>1;++fullLevels) {
        w=std::max(1U,w/2);h=std::max(1U,h/2);
    }
    const unsigned levels=request.mipLevels?request.mipLevels:fullLevels;
    if(levels>fullLevels)throw std::invalid_argument("Texture mip count exceeds its extent");
    const auto baseBytes=byteCount(request.width,request.height,1);
    const auto residentBytes=byteCount(request.width,request.height,levels);
    if((!request.mipLevels&&pixels.size()!=baseBytes)
        ||(request.mipLevels==1&&pixels.size()<baseBytes)
        ||(request.mipLevels>1&&pixels.size()!=baseBytes&&pixels.size()!=residentBytes))
        throw std::invalid_argument("Texture pixel storage does not match its mip policy");
    if(!rhi_.supportsSampledImage(request.format,request.sampler.filter==VK_FILTER_LINEAR))
        throw std::runtime_error("Device does not support the decoded texture sampling format");
    auto effective=request;
    effective.sampler.maxLod=std::min(effective.sampler.maxLod,float(levels-1));
    std::uint32_t bucket;
    {TimedSection timer{statistics_.hashMs};bucket=contentBucket(pixels);}
    statistics_.hashedPixelBytes+=pixels.size();
    {
    TimedSection timer{statistics_.matchMs};
    const auto matches=entries_.equal_range(bucket);
    for(auto at=matches.first;at!=matches.second;++at) {
        const auto& found=*at->second;
        if(sameUpload(found.upload,effective)&&found.pixels==pixels) {
            ++statistics_.requests;++statistics_.hits;statistics_.requestedBytes+=residentBytes;
            if(consumed) {
                statistics_.discardedPixelBytes+=consumed->size();
                std::vector<std::uint8_t>().swap(*consumed);
            }
            return {found.view,found.sampler};
        }
    }
    }

    auto entry=std::make_unique<Entry>(allocator_,rhi_);
    entry->upload=effective;
    std::vector<std::uint8_t> filtered;
    const std::vector<std::uint8_t>* storage=&pixels;
    {
    TimedSection timer{statistics_.mipMs};
    if(!request.mipLevels) {
        auto chain=buildTextureMipChain(pixels,request.width,request.height,request.semantic,request.alphaCutoff,request.sampler.filter==VK_FILTER_LINEAR);
        filtered=std::move(chain.pixels);storage=&filtered;
    }else if(request.mipLevels==1&&pixels.size()>baseBytes) {
        filtered.assign(pixels.begin(),pixels.begin()+static_cast<std::size_t>(baseBytes));storage=&filtered;
    }
    }
    const bool suppliedMips=levels==1||storage->size()==residentBytes;
    {
    TimedSection timer{statistics_.uploadMs};
    StagingBuffer staging{allocator_,allocator_.createBuffer(storage->size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true)};
    if(!staging.value.mapped)throw std::runtime_error("Texture staging buffer is not mapped");
    std::memcpy(staging.value.mapped,storage->data(),storage->size());
    entry->image=allocator_.createImage2D(request.width,request.height,request.format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT
            |(levels>1?VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0),levels);
    rhi_.transitionImageLayout(entry->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,levels);
    rhi_.copyBufferToImage(staging.value,entry->image,request.width,request.height,suppliedMips?levels:1,4);
    if(levels>1&&!suppliedMips)rhi_.generateMipmaps(entry->image,request.format,request.width,request.height,levels);
    else rhi_.transitionImageLayout(entry->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,levels);
    entry->view=rhi_.createImageView(entry->image.image,request.format,VK_IMAGE_ASPECT_COLOR_BIT,levels);
    entry->sampler=rhi_.createSampler(effective.sampler);
    }
    const SampledTextureBinding result{entry->view,entry->sampler};
    const auto inputBytes=pixels.size();
    if(!consumed)entry->pixels=pixels;
    const auto inserted=entries_.emplace(bucket,std::move(entry));
    if(consumed) {
        inserted->second->pixels.swap(*consumed);
        statistics_.transferredPixelBytes+=inputBytes;
    }else statistics_.copiedPixelBytes+=inputBytes;
    ++statistics_.requests;++statistics_.uniqueTextures;
    statistics_.requestedBytes+=residentBytes;statistics_.residentBytes+=residentBytes;
    statistics_.retainedPixelBytes+=inputBytes;
    return result;
}

void SampledTextureCache::clear() noexcept { entries_.clear(); statistics_ = {}; }
}
