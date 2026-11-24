#include "GpuPredicateDrawSelection.hpp"
#include "rhi/IGpuAllocator.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
namespace azurerender {
struct GpuPredicateDrawSelection::Impl {
    struct Frame {rhi::GpuBuffer output{};VkDescriptorSet set{};std::uint64_t generation=0;std::uint32_t count=0;bool valid=false,recorded=false,permitted=false;GpuPredicateSpan predicateSpan{};};
    rhi::IRhi& rhi;std::uint32_t capacity=0;std::uint64_t bytes=0;std::vector<Frame> frames;
    VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkPipeline pipeline{};
    explicit Impl(rhi::IRhi& backend):rhi(backend){}
    ~Impl(){
        if(pipeline)rhi.destroyPipeline(pipeline);if(layout)rhi.destroyPipelineLayout(layout);
        if(pool)rhi.destroyDescriptorPool(pool);if(descriptors)rhi.destroyDescriptorSetLayout(descriptors);
        for(auto& f:frames)rhi.allocator().destroyBuffer(f.output);
    }
    void initialize(std::uint32_t count,unsigned slots,const std::vector<char>& shader){
        const auto size=std::uint64_t(slots)*count*40;
        if(!count||count>1048576||!slots||slots>8||size>kMaximumBytes)throw std::invalid_argument("Invalid predicate draw capacity or pool budget");
        std::uint32_t magic=0;if(shader.size()>=4)std::memcpy(&magic,shader.data(),4);
        if(shader.size()<20||shader.size()>1024*1024||shader.size()%4||magic!=0x07230203U)throw std::invalid_argument("Draw selection requires bounded SPIR-V");
        capacity=count;bytes=size;
        descriptors=rhi.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,28};layout=rhi.createPipelineLayout(descriptors,&range);
        const auto module=rhi.createShaderModule(shader);
        try{pipeline=rhi.createComputePipeline({module,layout});}catch(...){rhi.destroyShaderModule(module);throw;}
        rhi.destroyShaderModule(module);rhi::DescriptorPoolDesc description;description.maxSets=slots;
        description.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,slots*3}};pool=rhi.createDescriptorPool(description);
        const auto sets=rhi.allocateDescriptorSets(pool,descriptors,slots);frames.resize(slots);
        for(unsigned slot=0;slot<slots;++slot){auto& f=frames[slot];f.set=sets.at(slot);
            f.output=rhi.allocator().createBuffer(VkDeviceSize(count)*40,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
            rhi.writeDescriptorBuffer({f.set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,f.output.buffer,f.output.size});
        }
    }
};
GpuPredicateDrawSelection::GpuPredicateDrawSelection(rhi::IRhi& rhi,std::uint32_t capacity,unsigned slots,const std::vector<char>& shader)
    :impl_(std::make_unique<Impl>(rhi)){impl_->initialize(capacity,slots,shader);}
GpuPredicateDrawSelection::~GpuPredicateDrawSelection()=default;
GpuPredicateDrawSelection::Ticket GpuPredicateDrawSelection::prepare(unsigned slot,const rhi::GpuBuffer& source,
    const rhi::GpuBuffer& predicate,std::uint32_t count,bool permitted,GpuPredicateSpan predicateSpan){
    auto& p=*impl_;auto& f=p.frames.at(slot);f.valid=false;
    if(count>p.capacity)throw std::length_error("Predicate draw capacity exceeded");
    if(!predicateSpan.rejectMask||(predicateSpan.rejectMask&predicateSpan.requiredMask)||(predicateSpan.strideWords&&predicateSpan.firstWord==0))
        throw std::invalid_argument("Predicate range requires a rejection mask and a separate global word");
    const auto lastWord=std::uint64_t(predicateSpan.firstWord)+(count?std::uint64_t(count-1)*predicateSpan.strideWords:0);
    if(lastWord>=kMaximumBytes/4)throw std::length_error("Predicate word range exceeds its pool budget");
    const auto predicateBytes=(lastWord+1)*4;
    if(count){
        if(!source.buffer||source.size<VkDeviceSize(count)*20||!predicate.buffer||predicate.size<predicateBytes)
            throw std::invalid_argument("Predicate draw input range is too small");
        if(source.buffer==predicate.buffer)throw std::invalid_argument("Draw commands alias the predicate");
        for(const auto& owned:p.frames)if(source.buffer==owned.output.buffer||predicate.buffer==owned.output.buffer)
            throw std::invalid_argument("Draw selection input aliases owned output");
    }
    if(f.generation==UINT64_MAX)throw std::overflow_error("Predicate draw generation exhausted");
    if(count){
        p.rhi.writeDescriptorBuffer({f.set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,VkDeviceSize(count)*20});
        p.rhi.writeDescriptorBuffer({f.set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,predicate.buffer,predicateBytes});
    }
    f.count=count;f.permitted=permitted;f.predicateSpan=predicateSpan;f.recorded=false;f.valid=true;++f.generation;
    return {slot,f.generation,count,&p};
}
void GpuPredicateDrawSelection::record(rhi::ICommandRecorder& commands,Ticket ticket){
    auto& p=*impl_;auto& f=p.frames.at(ticket.slot);
    if(ticket.owner!=&p||!f.valid||f.recorded||f.generation!=ticket.generation||f.count!=ticket.count)throw std::logic_error("Predicate draw ticket is stale or consumed");
    f.recorded=true;if(!ticket.count)return;
    const std::array<std::uint32_t,7> settings{ticket.count,p.capacity,f.permitted?1U:0U,f.predicateSpan.firstWord,f.predicateSpan.strideWords,f.predicateSpan.rejectMask,f.predicateSpan.requiredMask};
    commands.bindComputePipeline(p.pipeline);commands.bindComputeDescriptorSet(p.layout,f.set);
    commands.pushConstants(p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,settings.data(),28);commands.dispatch((ticket.count+63)/64,1,1);
    commands.bufferBarrier({f.output.buffer,0,f.output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
}
const rhi::GpuBuffer& GpuPredicateDrawSelection::output(unsigned slot)const{return impl_->frames.at(slot).output;}
VkDeviceSize GpuPredicateDrawSelection::fastOffset()const noexcept{return VkDeviceSize(impl_->capacity)*20;}
std::uint64_t GpuPredicateDrawSelection::bytes()const noexcept{return impl_->bytes;}
}
