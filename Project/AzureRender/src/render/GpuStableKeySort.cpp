#include "GpuStableKeySort.hpp"
#include "rhi/IGpuAllocator.hpp"
#include <algorithm>
#include <stdexcept>

namespace azurerender {
bool GpuStableKeySort::supported(std::uint32_t capacity,unsigned slots,std::uint64_t limit) noexcept {
    return capacity&&capacity<=kMaximumRecords&&slots&&slots<=8
        &&std::uint64_t(capacity)*24*slots<=std::min(limit,kMaximumBytes);
}
GpuStableKeySort::GpuStableKeySort(rhi::IRhi& backend,std::uint32_t capacity,unsigned slots,
    const std::vector<char>& shader):rhi_(backend),capacity_(capacity){
    if(!supported(capacity,slots))throw std::invalid_argument("GPU key sort capacity exceeds the bounded pool");
    try{
        setLayout_=rhi_.createDescriptorSetLayout({
            {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_COMPUTE_BIT,8};
        layout_=rhi_.createPipelineLayout(setLayout_,&push);
        auto module=rhi_.createShaderModule(shader);
        try{pipeline_=rhi_.createComputePipeline({module,layout_});}
        catch(...){rhi_.destroyShaderModule(module);throw;}
        rhi_.destroyShaderModule(module);
        rhi::DescriptorPoolDesc pool;pool.maxSets=slots*3;
        pool.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,slots*9}};pool_=rhi_.createDescriptorPool(pool);
        frames_.resize(slots);
        for(auto& frame:frames_){
            for(auto& buffer:frame.scratch)buffer=rhi_.allocator().createBuffer(
                VkDeviceSize(capacity)*8,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
            frame.ranges=rhi_.allocator().createBuffer(VkDeviceSize(capacity)*8,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            if(!frame.ranges.mapped)throw std::runtime_error("GPU key sort range upload is not mapped");
            const auto sets=rhi_.allocateDescriptorSets(pool_,setLayout_,3);
            std::copy(sets.begin(),sets.end(),frame.sets.begin());
            for(unsigned set=0;set<3;++set){
                rhi_.writeDescriptorBuffer({frame.sets[set],2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frame.ranges.buffer,frame.ranges.size});
                const auto output=set==1?1U:0U;
                rhi_.writeDescriptorBuffer({frame.sets[set],1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frame.scratch[output].buffer,frame.scratch[output].size});
                if(set)rhi_.writeDescriptorBuffer({frame.sets[set],0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frame.scratch[1-output].buffer,frame.scratch[1-output].size});
            }
        }
        bytes_=std::uint64_t(capacity)*24*slots;
    }catch(...){release();throw;}
}
GpuStableKeySort::~GpuStableKeySort(){release();}
void GpuStableKeySort::release() noexcept {
    if(pipeline_)rhi_.destroyPipeline(pipeline_);
    if(layout_)rhi_.destroyPipelineLayout(layout_);
    if(pool_)rhi_.destroyDescriptorPool(pool_);
    if(setLayout_)rhi_.destroyDescriptorSetLayout(setLayout_);
    for(auto& frame:frames_){
        for(auto& buffer:frame.scratch)rhi_.allocator().destroyBuffer(buffer);
        rhi_.allocator().destroyBuffer(frame.ranges);
    }
    frames_.clear();pipeline_={};layout_={};pool_={};setLayout_={};bytes_=0;
}
GpuStableKeySort::Output GpuStableKeySort::record(rhi::ICommandRecorder& commands,unsigned slot,
    const rhi::GpuBuffer& input,const std::vector<std::uint32_t>& segmentCounts){
    auto& frame=frames_.at(slot);
    if(segmentCounts.size()>kMaximumSegments)throw std::length_error("GPU key sort has too many segments");
    std::uint64_t total=0;std::uint32_t maximum=0;
    for(auto count:segmentCounts){total+=count;maximum=std::max(maximum,count);}
    if(total>capacity_)throw std::length_error("GPU key sort input exceeds capacity");
    if(!total)return {};
    if(!input.buffer||input.size<total*8)throw std::invalid_argument("GPU key sort input is too small");
    for(const auto& owned:frames_){
        if(input.buffer==owned.ranges.buffer)throw std::invalid_argument("GPU key sort input aliases range storage");
        for(const auto& scratch:owned.scratch)if(input.buffer==scratch.buffer)
            throw std::invalid_argument("GPU key sort input aliases scratch storage");
    }
    if(frame.segments!=segmentCounts){
        auto preparedSegments=segmentCounts;
        frame.segments.clear();
        auto* ranges=static_cast<std::array<std::uint32_t,2>*>(frame.ranges.mapped);std::uint32_t base=0;
        for(auto count:segmentCounts){std::fill_n(ranges+base,count,std::array<std::uint32_t,2>{base,count});base+=count;}
        rhi_.allocator().flush(frame.ranges,0,total*8);frame.segments.swap(preparedSegments);
    }
    commands.bufferBarrier({frame.ranges.buffer,0,total*8,VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    rhi_.writeDescriptorBuffer({frame.sets[0],0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,total*8});
    commands.bindComputePipeline(pipeline_);
    unsigned output=0;bool first=true;
    for(std::uint32_t width=1;;width*=2){
        const auto set=first?0U:output==0?2U:1U;
        commands.bindComputeDescriptorSet(layout_,frame.sets[set]);
        const std::array<std::uint32_t,2> parameters{static_cast<std::uint32_t>(total),width};
        commands.pushConstants(layout_,VK_SHADER_STAGE_COMPUTE_BIT,0,parameters.data(),sizeof(parameters));
        commands.dispatch((static_cast<std::uint32_t>(total)+127)/128,1,1);
        commands.bufferBarrier({frame.scratch[output].buffer,0,total*8,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        if(width>=(maximum+1)/2)break;
        output=1-output;first=false;
    }
    return {&frame.scratch[output],static_cast<std::uint32_t>(total)};
}
}
