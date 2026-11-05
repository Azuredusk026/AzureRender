#include "rhi/VulkanRhi.hpp"
#include "rhi/NullRhi.hpp"
#include "render/RenderGraph.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
struct Recorded {
    VkCommandBuffer command;
    VkPipelineStageFlags source, destination;
    std::vector<VkBufferMemoryBarrier> buffers;
};
std::vector<Recorded> calls;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
}

// CPU-only seam: inspect production command conversion without loading a device.
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer command,
    VkPipelineStageFlags source,VkPipelineStageFlags destination,VkDependencyFlags flags,
    std::uint32_t memoryCount,const VkMemoryBarrier*,std::uint32_t bufferCount,
    const VkBufferMemoryBarrier* buffers,std::uint32_t imageCount,const VkImageMemoryBarrier*) {
    require(flags==0 && memoryCount==0 && imageCount==0,"Buffer batches must preserve dependency kind");
    calls.push_back({command,source,destination,{buffers,buffers+bufferCount}});
}
int main() try {
    using namespace azurerender::rhi;
    const auto command=reinterpret_cast<VkCommandBuffer>(std::uintptr_t(900));
    BufferBarrierDesc a{reinterpret_cast<VkBuffer>(std::uintptr_t(1)),16,64,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT};
    auto b=a;b.buffer=reinterpret_cast<VkBuffer>(std::uintptr_t(2));b.offset=32;b.size=VK_WHOLE_SIZE;
    b.dstAccessMask=VK_ACCESS_INDEX_READ_BIT;
    auto c=b;c.buffer=reinterpret_cast<VkBuffer>(std::uintptr_t(3));
    VulkanCommandRecorder native(command);NullCommandRecorder fallback, individual;
    const std::vector<BufferBarrierDesc> input{a,b,a,c};
    for(const auto& item:input)individual.bufferBarrier(item);
    fallback.bufferBarriers(input.data(),input.size());
    require(fallback.calls.size()==individual.calls.size(),"Fallback must preserve every barrier");
    for(std::size_t i=0;i<input.size();++i)
        require(fallback.calls[i].name==individual.calls[i].name && fallback.calls[i].detail==individual.calls[i].detail,"Fallback order and fields differ");
    native.bufferBarriers(input.data(),input.size());
    require(calls.size()==2 && calls[0].buffers.size()==2 && calls[1].buffers.size()==2,"Repeated handles must start a new batch");
    std::size_t index=0;
    for(const auto& call:calls)for(const auto& item:call.buffers) {
        const auto& expected=input[index++];
        require(call.command==command && call.source==expected.srcStageMask && call.destination==expected.dstStageMask,"Batch stages or command changed");
        require(item.sType==VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER && item.pNext==nullptr && item.buffer==expected.buffer && item.offset==expected.offset && item.size==expected.size && item.srcAccessMask==expected.srcAccessMask && item.dstAccessMask==expected.dstAccessMask,"Batch resource range or access changed");
        require(item.srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED && item.dstQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED,"Batch ownership changed");
    }
    calls.clear();auto stageChange=c;stageChange.dstStageMask=VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    const std::vector<BufferBarrierDesc> stages{a,b,stageChange,a};
    native.bufferBarriers(stages.data(),stages.size());
    require(calls.size()==3 && calls[0].buffers.size()==2 && calls[1].destination==VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT && calls[2].destination==a.dstStageMask,"Stage boundaries must preserve sequence");
    calls.clear();a.srcStageMask=0;a.dstStageMask=0;b=a;b.buffer=c.buffer;
    const BufferBarrierDesc defaults[]{a,b};native.bufferBarriers(defaults,2);
    require(calls.size()==1 && calls[0].source==VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT && calls[0].destination==VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,"Zero stages must retain single-barrier defaults");
    calls.clear();native.bufferBarriers(nullptr,0);fallback.calls.clear();fallback.bufferBarriers(nullptr,0);
    require(calls.empty() && fallback.calls.empty(),"Empty batch must emit no command");
    for(ICommandRecorder* recorder:{static_cast<ICommandRecorder*>(&native),static_cast<ICommandRecorder*>(&fallback)}) {
        bool rejected=false;try {recorder->bufferBarriers(nullptr,1);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Missing batch storage must fail before recording");
    }
    require(calls.empty() && fallback.calls.empty(),"Rejected batch must emit no command");
    for(ICommandRecorder* recorder:{static_cast<ICommandRecorder*>(&native),static_cast<ICommandRecorder*>(&fallback)}) {
        bool rejected=false;
        try {recorder->bufferBarriers(&a,std::size_t(UINT32_MAX)+1);}
        catch(const std::length_error&){rejected=true;}
        require(rejected,"Oversized batch must fail before recording");
    }
    native.bufferBarrier(a);require(calls.size()==1 && calls[0].buffers.size()==1,"Single barrier path must remain compatible");
    calls.clear();
    azurerender::RenderGraph graph;
    const auto vertices=graph.importBuffer("vertices",a),indices=graph.importBuffer("indices",b);
    const auto produce=graph.addCommandPass("produce",[&](ICommandRecorder&) {
        require(calls.size()==1 && calls.back().buffers.size()==2,"Producer dependencies must batch before commands");
    });
    graph.use(produce,vertices,azurerender::RenderGraphUsage::Storage,true);
    graph.use(produce,indices,azurerender::RenderGraphUsage::Storage,true);
    const auto consume=graph.addCommandPass("consume",[&](ICommandRecorder&) {
        require(calls.size()==2 && calls.back().buffers.size()==2,"Consumer dependencies must batch before commands");
    });
    graph.use(consume,vertices,azurerender::RenderGraphUsage::VertexBuffer,false);
    graph.use(consume,indices,azurerender::RenderGraphUsage::IndexBuffer,false);
    std::string error;require(graph.compile(error),"Buffer graph must compile");
    graph.execute(&native);require(calls.size()==2,"Two pass boundaries must emit two compatible batches");
    require(calls.back().source==VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT && calls.back().destination==VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,"Graph visibility stages changed");
    require(calls.back().buffers[0].dstAccessMask==VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT && calls.back().buffers[1].dstAccessMask==VK_ACCESS_INDEX_READ_BIT,"Per-resource graph access changed");
    calls.clear();graph.execute(&native);require(calls.size()==2,"Repeated graph execution must retain barriers");
    calls.clear();graph.execute(&native,[](auto){return true;});require(calls.size()==2,"Recorded commands must still receive primary dependencies");
    calls.clear();azurerender::RenderGraph empty;empty.addPass("empty");require(empty.compile(error),"Empty-resource pass must compile");empty.execute(&native);
    require(calls.empty(),"Empty-resource graph must emit no barrier");
    std::cout<<"Buffer batching, stage boundaries, repeated handles, fallback, graph order and command fields passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
