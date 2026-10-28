#include "render/SampledTextureCache.hpp"
#include "rhi/NullRhi.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace azurerender;
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
template<class F> void reject(F operation) {
    bool rejected=false;try { operation(); }catch(const std::exception&) { rejected=true; }
    require(rejected,"Invalid or failed texture request must be rejected");
}
std::vector<std::uint8_t> checker() {
    return {0,0,0,255,255,255,255,255,255,255,255,255,0,0,0,255};
}
SampledTextureUpload request() {
    SampledTextureUpload result;result.width=result.height=2;return result;
}

// The adapter injects a failure after image/view creation. Actual recording,
// upload storage, handles and allocation accounting remain the existing RHI.
class SamplerFailureRhi final : public rhi::IRhi {
public:
    rhi::NullRhi backend;
    bool failSampler=true;
    rhi::IGpuAllocator& allocator() override { return backend.allocator(); }
    bool supportsSampledImage(VkFormat f,bool linear)const override {return backend.supportsSampledImage(f,linear);}
    void copyBuffer(const rhi::GpuBuffer& a,const rhi::GpuBuffer& b,VkDeviceSize s)override {backend.copyBuffer(a,b,s);}
    void transitionImageLayout(const rhi::GpuImage& a,VkImageLayout b,VkImageLayout c,unsigned d)override {backend.transitionImageLayout(a,b,c,d);}
    void copyBufferToImage(const rhi::GpuBuffer& a,const rhi::GpuImage& b,unsigned w,unsigned h,unsigned l,unsigned bytes)override {backend.copyBufferToImage(a,b,w,h,l,bytes);}
    void clearImage(const rhi::GpuImage& image)override {backend.clearImage(image);}
    void generateMipmaps(const rhi::GpuImage& i,VkFormat f,unsigned w,unsigned h,unsigned l)override {backend.generateMipmaps(i,f,w,h,l);}
    VkImageView createImageView(VkImage i,VkFormat f,VkImageAspectFlags a,unsigned l,unsigned b=0,VkImageViewType t=VK_IMAGE_VIEW_TYPE_2D,unsigned layers=1)override {return backend.createImageView(i,f,a,l,b,t,layers);}
    void destroyImageView(VkImageView value)override {backend.destroyImageView(value);}
    VkSampler createSampler(const rhi::SamplerDesc& desc)override {if(failSampler)throw std::runtime_error("Injected sampler creation failure");return backend.createSampler(desc);}
    void destroySampler(VkSampler value)override {backend.destroySampler(value);}
    void executeOneShot(const std::function<void(rhi::ICommandRecorder&)>& record)override {backend.executeOneShot(record);}
#define UNUSED_RHI_RESULT(type,name,args) type name args override {throw std::logic_error("Unexpected RHI operation: " #name);}
#define UNUSED_RHI_VOID(name,args) void name args override {throw std::logic_error("Unexpected RHI operation: " #name);}
    UNUSED_RHI_RESULT(VkShaderModule,createShaderModule,(const std::vector<char>&))
    UNUSED_RHI_VOID(destroyShaderModule,(VkShaderModule))
    UNUSED_RHI_RESULT(VkPipelineLayout,createPipelineLayout,(VkDescriptorSetLayout,const rhi::PushConstantRangeDesc*))
    UNUSED_RHI_VOID(destroyPipelineLayout,(VkPipelineLayout))
    UNUSED_RHI_RESULT(VkPipeline,createComputePipeline,(const rhi::ComputePipelineDesc&))
    UNUSED_RHI_RESULT(VkPipeline,createGraphicsPipeline,(const rhi::GraphicsPipelineDesc&))
    UNUSED_RHI_VOID(destroyPipeline,(VkPipeline))
    UNUSED_RHI_RESULT(VkDescriptorSetLayout,createDescriptorSetLayout,(const std::vector<rhi::DescriptorBindingDesc>&))
    UNUSED_RHI_VOID(destroyDescriptorSetLayout,(VkDescriptorSetLayout))
    UNUSED_RHI_RESULT(VkDescriptorPool,createDescriptorPool,(const rhi::DescriptorPoolDesc&))
    UNUSED_RHI_VOID(destroyDescriptorPool,(VkDescriptorPool))
    UNUSED_RHI_RESULT(std::vector<VkDescriptorSet>,allocateDescriptorSets,(VkDescriptorPool,VkDescriptorSetLayout,unsigned))
    UNUSED_RHI_VOID(writeDescriptorImage,(const rhi::DescriptorImageWrite&))
    UNUSED_RHI_VOID(writeDescriptorImageArray,(const rhi::DescriptorImageArrayWrite&))
    UNUSED_RHI_VOID(writeDescriptorBuffer,(const rhi::DescriptorBufferWrite&))
    UNUSED_RHI_RESULT(VkRenderPass,createRenderPass,(const rhi::RenderPassDesc&))
    UNUSED_RHI_VOID(destroyRenderPass,(VkRenderPass))
    UNUSED_RHI_RESULT(VkFramebuffer,createFramebuffer,(const rhi::FramebufferDesc&))
    UNUSED_RHI_VOID(destroyFramebuffer,(VkFramebuffer))
#undef UNUSED_RHI_RESULT
#undef UNUSED_RHI_VOID
};

void shared_resources() {
    rhi::NullRhi backend;backend.collectImageUploads=true;
    SampledTextureCache cache(backend.allocator(),backend);
    const auto meshA=checker(),meshB=checker();
    auto upload=request();upload.semantic=TextureFilterSemantic::Srgb;
    const auto a=cache.acquire(meshA,upload),b=cache.acquire(meshB,upload);
    require(a.view&&a.sampler,"A texture request must return valid descriptor values");
    require(a.view==b.view&&a.sampler==b.sampler,"Independent resources with identical texture inputs must share native objects");
    require(backend.allocator().statistics().liveImages==1,"Shared content must have one GPU image allocation");
    require(backend.imageUploads.size()==1,"Shared content must upload once");
    const auto& pixels=backend.imageUploads.front().pixels;
    require(pixels.size()==20&&std::equal(meshA.begin(),meshA.end(),pixels.begin()),"All authored base pixels must survive sharing unchanged");
    require(pixels[16]==188&&pixels[17]==188&&pixels[18]==188&&pixels[19]==255,"Shared color texture keeps its full linear-space filtered mip chain");
    const auto stats=cache.statistics();
    require(stats.requests==2&&stats.hits==1&&stats.uniqueTextures==1,"Sharing statistics must count both consumers and one owner");
    require(stats.requestedBytes==40&&stats.residentBytes==20&&stats.retainedPixelBytes==16,"Logical byte statistics separate requests, GPU residency and retained identity bytes");
    cache.clear();cache.clear();
    require(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Repeated clear must release each unique allocation exactly once");
    const auto views=std::count_if(backend.calls.begin(),backend.calls.end(),[](const auto& c){return c.name=="destroyImageView";});
    const auto samplers=std::count_if(backend.calls.begin(),backend.calls.end(),[](const auto& c){return c.name=="destroySampler";});
    require(views==1&&samplers==1,"Shared native views and samplers must be destroyed exactly once");
    require(cache.acquire(meshA,upload).view!=a.view,"Cleared cache must create fresh bindings on reuse");
}

void isolated_keys() {
    rhi::NullRhi backend;SampledTextureCache cache(backend.allocator(),backend);
    const auto pixels=checker();const auto original=request();const auto a=cache.acquire(pixels,original);
    auto check=[&](auto mutate){auto variant=original;mutate(variant);const auto b=cache.acquire(pixels,variant);require(a.view!=b.view,"Different effective texture inputs must not alias");};
    check([](auto& r){r.width=4;r.height=1;});
    check([](auto& r){r.format=VK_FORMAT_R8G8B8A8_SRGB;});
    check([](auto& r){r.mipLevels=1;});
    check([](auto& r){r.semantic=TextureFilterSemantic::Srgb;});
    check([](auto& r){r.semantic=TextureFilterSemantic::Normal;});
    check([](auto& r){r.semantic=TextureFilterSemantic::PackedNormal;});
    check([](auto& r){r.alphaCutoff=.5F;});
    check([](auto& r){r.sampler.filter=VK_FILTER_NEAREST;});
    check([](auto& r){r.sampler.addressU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;});
    check([](auto& r){r.sampler.addressV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;});
    check([](auto& r){r.sampler.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;});
    check([](auto& r){r.sampler.mipmapLinear=false;});
    check([](auto& r){r.sampler.maxAnisotropy=4;});
    check([](auto& r){r.sampler.mipLodBias=-1;});
    check([](auto& r){r.sampler.magFilter=VK_FILTER_NEAREST;});
    auto equivalent=original;equivalent.sampler.magFilter=VK_FILTER_LINEAR;
    require(cache.acquire(pixels,equivalent).view==a.view,"Equivalent effective magnification settings must share the texture binding");

    check([](auto& r){r.sampler.maxLod=0;});
    auto changed=pixels;changed[0]=1;
    require(cache.acquire(changed,original).view!=a.view,"A one-byte pixel change must never reuse stale texture content");
}

void hash_collision() {
    rhi::NullRhi backend;SampledTextureCache cache(backend.allocator(),backend);
    const std::vector<std::uint8_t> a={166,38,197,129,215,182,100,226},b={36,200,198,21,236,71,103,158};
    auto upload=request();upload.width=2;upload.height=1;upload.mipLevels=1;
    const auto first=cache.acquire(a,upload),second=cache.acquire(b,upload);
    require(first.view!=second.view,"Equal FNV-1a bucket hashes with distinct bytes must have different GPU images");
    require(cache.acquire(a,upload).view==first.view&&cache.acquire(b,upload).view==second.view,"A collision bucket must retain both exact content identities");
    require(cache.statistics().uniqueTextures==2&&cache.statistics().hits==2,"Collision buckets must count independent owners and exact hits");
}

void invalid_inputs() {
    rhi::NullRhi backend;SampledTextureCache cache(backend.allocator(),backend);auto upload=request();
    reject([&]{cache.acquire({},upload);});
    auto wrong=upload;wrong.width=0;reject([&]{cache.acquire(checker(),wrong);});
    wrong=upload;wrong.mipLevels=3;reject([&]{cache.acquire(checker(),wrong);});
    wrong=upload;wrong.alphaCutoff=2;reject([&]{cache.acquire(checker(),wrong);});
    wrong=upload;wrong.sampler.maxAnisotropy=0;reject([&]{cache.acquire(checker(),wrong);});
    wrong=upload;wrong.sampler.magFilter=static_cast<VkFilter>(999);reject([&]{cache.acquire(checker(),wrong);});
    wrong=upload;wrong.sampler.mipLodBias=std::numeric_limits<float>::quiet_NaN();reject([&]{cache.acquire(checker(),wrong);});
    backend.unavailableSampledFormats.insert(upload.format);reject([&]{cache.acquire(checker(),upload);});
    require(backend.allocator().statistics().imageAllocations==0&&backend.allocator().statistics().bufferAllocations==0,"Invalid requests must fail before native allocation");
    require(cache.statistics().requests==0,"Rejected requests must not publish cache success statistics");
    backend.unavailableSampledFormats.clear();require(cache.acquire(checker(),upload).view!=VK_NULL_HANDLE,"Input rejection leaves the cache usable");
}

void authored_mips() {
    rhi::NullRhi backend;backend.collectImageUploads=true;SampledTextureCache cache(backend.allocator(),backend);
    auto upload=request();upload.mipLevels=2;auto pixels=checker();pixels.insert(pixels.end(),{7,31,41,127});
    cache.acquire(pixels,upload);
    require(backend.imageUploads.size()==1&&backend.imageUploads[0].pixels==pixels,"Authored mip texels must retain their exact bytes");
    auto broken=pixels;broken.pop_back();reject([&]{cache.acquire(broken,upload);});
    upload.mipLevels=1;cache.acquire(pixels,upload);
    require(backend.imageUploads[1].pixels==checker(),"A single-level policy must upload exactly the authored base level");
    upload.mipLevels=2;cache.acquire(checker(),upload);
    require(backend.imageUploads[2].pixels==checker()&&backend.imageUploads[2].levels==1,"Base-only data must upload a single supplied level before RHI mip generation");
    require(std::count_if(backend.calls.begin(),backend.calls.end(),[](const auto& c){return c.name=="generateMipmaps";})==1,"Positive mip count with base-only data preserves the RHI generation path");
}

void failed_upload() {
    SamplerFailureRhi backend;SampledTextureCache cache(backend.allocator(),backend);
    reject([&]{cache.acquire(checker(),request());});
    require(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Failed sampler creation must release partial image and staging allocations");
    require(cache.statistics().requests==0&&cache.statistics().uniqueTextures==0,"Failed upload must not publish an incomplete cache entry");
    backend.failSampler=false;const auto result=cache.acquire(checker(),request());
    require(result.view&&cache.statistics().uniqueTextures==1,"A failed upload must allow the identical request to recover");
}

void transferred_inputs() {
    SamplerFailureRhi backend;backend.backend.collectImageUploads=true;
    SampledTextureCache cache(backend.allocator(),backend);
    auto pixels=checker();const auto original=pixels;
    reject([&]{cache.acquire(std::move(pixels),request());});
    require(pixels==original,"Failed ownership transfer must retain the caller's input for recovery");
    backend.failSampler=false;
    const auto first=cache.acquire(std::move(pixels),request());
    require(pixels.empty()&&pixels.capacity()==0,"Successful unique upload must release the caller's pixel allocation");
    auto duplicate=original;
    const auto second=cache.acquire(std::move(duplicate),request());
    require(duplicate.empty()&&duplicate.capacity()==0,"An exact cache hit must release the redundant caller allocation");
    require(first.view==second.view&&first.sampler==second.sampler,"Transferred inputs must retain exact shared identity");
    // Sampler failure occurs after the first upload; only the successful
    // retry uploads again. Subsequent exact hits must add no transfers.
    require(backend.backend.imageUploads.size()==2&&backend.backend.imageUploads.back().pixels.size()==20
        &&std::equal(original.begin(),original.end(),backend.backend.imageUploads.back().pixels.begin()),
        "Transferred inputs must preserve the original base pixels and full mip chain");
    require(cache.acquire(original,request()).view==first.view&&original==checker(),
        "Borrowed inputs must remain unchanged and match transferred cache identities");
    require(cache.statistics().requests==3&&cache.statistics().hits==2
        &&cache.statistics().retainedPixelBytes==16,"Transferred ownership must retain one complete identity copy");
}

int main() {
    int failed=0;
    for(const auto& test:std::vector<std::pair<const char*,std::function<void()>>>{
            {"shared multi-resource texture",shared_resources},{"complete key isolation",isolated_keys},
            {"content hash collision",hash_collision},{"invalid input recovery",invalid_inputs},
            {"authored mip bytes",authored_mips},{"partial upload recovery",failed_upload},
            {"transferred input ownership",transferred_inputs}}) {
        try {test.second();std::cout<<"PASS "<<test.first<<'\n';}
        catch(const std::exception& e) {++failed;std::cerr<<"FAIL "<<test.first<<": "<<e.what()<<'\n';}
    }
    std::cout<<7-failed<<"/7 sampled texture cache cases passed\n";return failed?1:0;
}
