#include "render/SampledTextureCache.hpp"
#include "foundation/ImmutableBytes.hpp"

#include <cmath>
#include <chrono>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace azurerender {
namespace {
std::uint32_t contentBucket(const std::vector<std::uint8_t>& pixels) {
    // This is only a bucket selector. Exact bytes and every effective upload
    // parameter are compared before a binding can be reused.
    return immutableBytesBucket(pixels);
}
struct TimedSection {
    double& elapsed;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    ~TimedSection() {elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
};
bool sameSampler(const rhi::SamplerDesc& a,const rhi::SamplerDesc& b) {
    return a.filter==b.filter&&a.effectiveMagFilter()==b.effectiveMagFilter()&&a.addressU==b.addressU&&a.addressV==b.addressV
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
struct ChannelPacking {
    unsigned count=4;
    std::array<unsigned,4> sources{};
    VkComponentMapping mapping{};
};
ChannelPacking channelPacking(const std::vector<std::uint8_t>& pixels) {
    // Candidate relations only disappear as further mip texels are inspected.
    // A short prefix rejects ordinary RGB/RGBA data without scanning its tail.
    std::array<unsigned,4> candidates{3,7,15,31};
    const auto layout=[&] {
        ChannelPacking result;result.count=0;
        std::array<VkComponentSwizzle,4> swizzles{};
        constexpr std::array<VkComponentSwizzle,4> stored{VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A};
        for(unsigned c=0;c<4;++c) {
            if(candidates[c]&1U)swizzles[c]=VK_COMPONENT_SWIZZLE_ZERO;
            else if(candidates[c]&2U)swizzles[c]=VK_COMPONENT_SWIZZLE_ONE;
            else {
                unsigned prior=0;
                for(;prior<c;++prior)if(candidates[c]&(4U<<prior))break;
                if(prior<c)swizzles[c]=swizzles[prior];
                else {result.sources[result.count]=c;swizzles[c]=stored[result.count++];}
            }
        }
        // Constant images still retain their extent and mip count in R8.
        if(result.count==0){result.count=1;result.sources[0]=0;}
        result.mapping={swizzles[0],swizzles[1],swizzles[2],swizzles[3]};return result;
    };
    for(std::size_t at=0;at<pixels.size();at+=4) {
        for(unsigned c=0;c<4;++c) {
            const auto value=pixels[at+c];
            if(value!=0)candidates[c]&=~1U;
            if(value!=255)candidates[c]&=~2U;
            for(unsigned prior=0;prior<c;++prior)
                if(value!=pixels[at+prior])candidates[c]&=~(4U<<prior);
        }
        if(at==252&&layout().count>2)return {};
    }
    auto result=layout();return result.count<=2?result:ChannelPacking{};
}
bool channelAddressMode(VkSamplerAddressMode mode) {
    return mode==VK_SAMPLER_ADDRESS_MODE_REPEAT||mode==VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
        ||mode==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}
}

struct SampledTextureCache::Entry {
    rhi::IGpuAllocator& allocator;
    rhi::IRhi& rhi;
    SampledTextureUpload upload;
    std::vector<std::uint8_t> pixels;
    std::shared_ptr<const ImmutableBytes> prepared;
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

SampledTextureCache::SampledTextureCache(rhi::IGpuAllocator& allocator, rhi::IRhi& rhi,bool compactLinearChannels)
    : allocator_(allocator), rhi_(rhi),compactLinearChannels_(compactLinearChannels) {}
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
                                                     std::vector<std::uint8_t>* consumed,
                                                     std::shared_ptr<const ImmutableBytes> prepared) {
    if(sealed_)throw std::logic_error("Sampled texture cache acquisition has finished");
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
        ||(request.sampler.filter!=VK_FILTER_NEAREST&&request.sampler.filter!=VK_FILTER_LINEAR)
        ||(request.sampler.effectiveMagFilter()!=VK_FILTER_NEAREST&&request.sampler.effectiveMagFilter()!=VK_FILTER_LINEAR))
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
    if(!rhi_.supportsSampledImage(request.format,request.sampler.filter==VK_FILTER_LINEAR||request.sampler.effectiveMagFilter()==VK_FILTER_LINEAR))
        throw std::runtime_error("Device does not support the decoded texture sampling format");
    auto effective=request;
    effective.sampler.maxLod=std::min(effective.sampler.maxLod,float(levels-1));
    std::uint32_t bucket;
    {TimedSection timer{statistics_.hashMs};bucket=prepared?prepared->bucket():contentBucket(pixels);}
    if(!prepared)statistics_.hashedPixelBytes+=pixels.size();
    {
    TimedSection timer{statistics_.matchMs};
    const auto matches=entries_.equal_range(bucket);
    for(auto at=matches.first;at!=matches.second;++at) {
        const auto& found=*at->second;
        if(!sameUpload(found.upload,effective))continue;
        const bool sameIdentity=found.prepared?(prepared?found.prepared->equals(*prepared):found.prepared->equals(pixels))
            :found.pixels==pixels;
        if(sameIdentity) {
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
    ChannelPacking packing;
    VkFormat physicalFormat=request.format;
    std::vector<std::uint8_t> packed;
    if(compactLinearChannels_&&suppliedMips&&request.format==VK_FORMAT_R8G8B8A8_UNORM
        &&rhi_.supportsSampledImageViewMapping()&&channelAddressMode(effective.sampler.addressU)
        &&channelAddressMode(effective.sampler.addressV)&&channelAddressMode(effective.sampler.addressW)) {
        packing=channelPacking(*storage);
        if(packing.count<4) {
            const auto format=packing.count==1?VK_FORMAT_R8_UNORM:VK_FORMAT_R8G8_UNORM;
            if(rhi_.supportsSampledImage(format,effective.sampler.filter==VK_FILTER_LINEAR||effective.sampler.effectiveMagFilter()==VK_FILTER_LINEAR)) {
                packed.resize(storage->size()/4*packing.count);
                for(std::size_t p=0;p<storage->size()/4;++p)for(unsigned c=0;c<packing.count;++c)
                    packed[p*packing.count+c]=(*storage)[p*4+packing.sources[c]];
                storage=&packed;physicalFormat=format;
            }else packing=ChannelPacking{};
        }
    }
    const auto physicalBytes=residentBytes/4*packing.count;
    {
    TimedSection timer{statistics_.uploadMs};
    StagingBuffer staging{allocator_,allocator_.createBuffer(storage->size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true)};
    if(!staging.value.mapped)throw std::runtime_error("Texture staging buffer is not mapped");
    std::memcpy(staging.value.mapped,storage->data(),storage->size());
    entry->image=allocator_.createImage2D(request.width,request.height,physicalFormat,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT
            |(levels>1?VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0),levels);
    rhi_.transitionImageLayout(entry->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,levels);
    rhi_.copyBufferToImage(staging.value,entry->image,request.width,request.height,suppliedMips?levels:1,packing.count);
    if(levels>1&&!suppliedMips)rhi_.generateMipmaps(entry->image,request.format,request.width,request.height,levels);
    else rhi_.transitionImageLayout(entry->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,levels);
    entry->view=packing.count<4?rhi_.createMappedSampledImageView(entry->image.image,physicalFormat,levels,packing.mapping)
        :rhi_.createImageView(entry->image.image,request.format,VK_IMAGE_ASPECT_COLOR_BIT,levels);
    entry->sampler=rhi_.createSampler(effective.sampler);
    }
    const SampledTextureBinding result{entry->view,entry->sampler};
    const auto inputBytes=pixels.size();
    if(prepared)entry->prepared=std::move(prepared);
    else if(!consumed)entry->pixels=pixels;
    const auto inserted=entries_.emplace(bucket,std::move(entry));
    if(consumed) {
        inserted->second->pixels.swap(*consumed);
        statistics_.transferredPixelBytes+=inputBytes;
    }else if(inserted->second->prepared)statistics_.referencedPixelBytes+=inputBytes;
    else statistics_.copiedPixelBytes+=inputBytes;
    ++statistics_.requests;++statistics_.uniqueTextures;
    statistics_.requestedBytes+=residentBytes;statistics_.residentBytes+=physicalBytes;
    statistics_.channelSavedBytes+=residentBytes-physicalBytes;
    if(packing.count<4)++statistics_.compactedTextures;
    statistics_.retainedPixelBytes+=inputBytes;
    statistics_.retainedIdentityStorageBytes+=inserted->second->prepared?inserted->second->prepared->storedBytes():inserted->second->pixels.capacity();
    return result;
}

void SampledTextureCache::seal() noexcept {
    if(sealed_)return;
    for(auto& item:entries_){std::vector<std::uint8_t>{}.swap(item.second->pixels);item.second->prepared.reset();}
    statistics_.releasedIdentityBytes+=statistics_.retainedPixelBytes;
    statistics_.releasedIdentityStorageBytes+=statistics_.retainedIdentityStorageBytes;
    statistics_.retainedPixelBytes=0;
    statistics_.retainedIdentityStorageBytes=0;
    sealed_=true;
}

SampledTextureBinding SampledTextureCache::acquirePrepared(std::shared_ptr<const ImmutableBytes> prepared,
                                                 const SampledTextureUpload& request) {
    if(sealed_)throw std::logic_error("Sampled texture cache acquisition has finished");
    if(!prepared)throw std::invalid_argument("Immutable texture payload is required");
    auto pixels=prepared->read();
    return acquireImpl(pixels,request,nullptr,std::move(prepared));
}
void SampledTextureCache::clear() noexcept { entries_.clear(); statistics_ = {}; sealed_=false; }
}
