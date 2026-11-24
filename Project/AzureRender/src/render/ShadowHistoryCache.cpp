#include "ShadowHistoryCache.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
bool ShadowHistoryCache::fits(VkExtent2D extent,unsigned slots,unsigned workflows,std::uint64_t limit) noexcept{
    if(!extent.width||!extent.height||slots<2||slots>8||!workflows||workflows>2)return false;
    return std::uint64_t(extent.width)*extent.height<=std::min(limit,kMaximumBytes)/(8ULL*slots*workflows);
}
struct ShadowHistoryCache::Impl {
    struct Frame {rhi::GpuBuffer buffer{};VkDescriptorSet set{};std::uint64_t generation=0;bool recorded=true;};
    rhi::IRhi& rhi;unsigned slots=0,workflows=0;std::uint64_t bytes=0;std::vector<Frame> frames;
    VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkPipeline pipeline{};
    explicit Impl(rhi::IRhi& backend):rhi(backend){}
    ~Impl(){
        if(pipeline)rhi.destroyPipeline(pipeline);if(layout)rhi.destroyPipelineLayout(layout);
        if(pool)rhi.destroyDescriptorPool(pool);if(descriptors)rhi.destroyDescriptorSetLayout(descriptors);
        for(auto& frame:frames)rhi.allocator().destroyBuffer(frame.buffer);
    }
    Frame& frame(unsigned slot,unsigned workflow){
        if(slot>=slots||workflow>=workflows)throw std::out_of_range("Shadow history slot or workflow is out of range");
        return frames.at(workflow*slots+slot);
    }
    void initialize(VkExtent2D extent,unsigned frameSlots,unsigned paths,const std::vector<char>& shader,std::uint64_t limit){
        if(!ShadowHistoryCache::fits(extent,frameSlots,paths,limit))throw std::invalid_argument("Shadow history exceeds extent, slot, workflow or pool bounds");
        std::uint32_t magic=0;if(shader.size()>=4)std::memcpy(&magic,shader.data(),4);
        if(shader.size()<20||shader.size()>1024*1024||shader.size()%4||magic!=0x07230203U)throw std::invalid_argument("Shadow history clear requires bounded SPIR-V");
        slots=frameSlots;workflows=paths;
        const auto frameBytes=std::uint64_t(extent.width)*extent.height*8;bytes=frameBytes*slots*workflows;
        descriptors=rhi.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        layout=rhi.createPipelineLayout(descriptors,nullptr);const auto module=rhi.createShaderModule(shader);
        try{pipeline=rhi.createComputePipeline({module,layout});}catch(...){rhi.destroyShaderModule(module);throw;}
        rhi.destroyShaderModule(module);
        const auto count=slots*workflows;rhi::DescriptorPoolDesc desc;desc.maxSets=count;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,count}};
        pool=rhi.createDescriptorPool(desc);const auto sets=rhi.allocateDescriptorSets(pool,descriptors,count);frames.resize(count);
        for(unsigned i=0;i<count;++i){auto& frame=frames[i];frame.set=sets.at(i);
            frame.buffer=rhi.allocator().createBuffer(frameBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
            rhi.writeDescriptorBuffer({frame.set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frame.buffer.buffer,frameBytes});
        }
    }
};
ShadowHistoryCache::ShadowHistoryCache(rhi::IRhi& rhi,VkExtent2D extent,unsigned slots,unsigned workflows,const std::vector<char>& shader,std::uint64_t limit)
    :impl_(std::make_unique<Impl>(rhi)){impl_->initialize(extent,slots,workflows,shader,limit);}
ShadowHistoryCache::~ShadowHistoryCache()=default;
ShadowHistoryCache::Ticket ShadowHistoryCache::prepareClear(unsigned slot,unsigned workflow){
    auto& frame=impl_->frame(slot,workflow);
    if(frame.generation==UINT64_MAX)throw std::overflow_error("Shadow history clear generation exhausted");
    frame.recorded=false;++frame.generation;return {slot,workflow,frame.generation,impl_.get()};
}
void ShadowHistoryCache::recordClear(rhi::ICommandRecorder& commands,Ticket ticket){
    if(ticket.owner!=impl_.get())throw std::logic_error("Shadow history ticket belongs to another owner");
    auto& frame=impl_->frame(ticket.slot,ticket.workflow);
    if(!ticket.generation||ticket.generation!=frame.generation||frame.recorded)throw std::logic_error("Shadow history clear ticket is stale or consumed");
    frame.recorded=true;
    commands.bufferBarrier({frame.buffer.buffer,0,frame.buffer.size,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
    commands.bindComputePipeline(impl_->pipeline);commands.bindComputeDescriptorSet(impl_->layout,frame.set);
    const auto groups=std::min<std::uint64_t>(65535,(frame.buffer.size/4+63)/64);commands.dispatch(static_cast<std::uint32_t>(groups),1,1);
    commands.bufferBarrier({frame.buffer.buffer,0,frame.buffer.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
}
const rhi::GpuBuffer& ShadowHistoryCache::buffer(unsigned slot,unsigned workflow)const{return impl_->frame(slot,workflow).buffer;}
std::uint64_t ShadowHistoryCache::bytes()const noexcept{return impl_->bytes;}
}
