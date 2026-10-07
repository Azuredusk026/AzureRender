#include "render/SkinningBatch.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=2) throw std::invalid_argument("Expected skinning shader");
    test::VulkanComputeFixture host;
    auto& backend=*host.backend;
    rhi::GpuBuffer input{},matrices{},output{};
    VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    VkShaderModule shader=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*) {
        if(shader)backend.destroyShaderModule(shader);
        if(pipeline)backend.destroyPipeline(pipeline);
        if(layout)backend.destroyPipelineLayout(layout);
        if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        for(auto* buffer:{&input,&matrices,&output})host.allocator.destroyBuffer(*buffer);
    });
    std::array<std::uint32_t,27> source{};
    const auto number=[&](unsigned word,float value){std::memcpy(&source[word],&value,sizeof(value));};
    number(0,1);number(1,2);number(2,3);number(4,1);number(6,1);number(9,1);
    number(16,1);number(20,4);number(24,2);
    std::array<std::array<float,16>,2> joints{};
    for(unsigned i=0;i<2;++i) {joints[i][0]=joints[i][5]=joints[i][10]=joints[i][15]=1;
        joints[i][12]=i==0?10.F:20.F;}
    input=host.allocator.createBuffer(sizeof(source),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    matrices=host.allocator.createBuffer(sizeof(joints),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(81*sizeof(std::uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(input.mapped,source.data(),sizeof(source));std::memcpy(matrices.mapped,joints.data(),sizeof(joints));
    for(const auto* buffer:{&input,&matrices})host.allocator.flush(*buffer,0,buffer->size);
    setLayout=backend.createDescriptorSetLayout({
        {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=1;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
    pool=backend.createDescriptorPool(poolDesc);
    const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    const rhi::GpuBuffer* buffers[]={&input,&matrices,&output};
    for(unsigned i=0;i<3;++i)backend.writeDescriptorBuffer({set,i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffers[i]->buffer,buffers[i]->size});
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,sizeof(SkinningBatch)};
    layout=backend.createPipelineLayout(setLayout,&range);
    shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});backend.destroyShaderModule(shader);shader=VK_NULL_HANDLE;
    std::vector<std::uint32_t> baseline;
    for(bool batched:{false,true}) {
        std::memset(output.mapped,0xCD,static_cast<std::size_t>(output.size));
        host.allocator.flush(output,0,output.size);
        host.submit([&](rhi::ICommandRecorder& recorder,VkCommandBuffer) {
            for(const auto* buffer:buffers)recorder.bufferBarrier({buffer->buffer,0,buffer->size,
                VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,
                static_cast<VkAccessFlags>(buffer==&output?VK_ACCESS_SHADER_WRITE_BIT:VK_ACCESS_SHADER_READ_BIT)});
            recorder.bindComputePipeline(pipeline);recorder.bindComputeDescriptorSet(layout,set);
            for(unsigned slice=0;slice<(batched?1U:2U);++slice) {
                const SkinningBatch parameters{1,slice,{.25F,.5F},1+slice,1,batched?2U:1U};
                recorder.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&parameters,sizeof(parameters));
                recorder.dispatch(1,parameters.instanceCount,1);
            }
            recorder.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(output,0,output.size);
        const auto* words=static_cast<const std::uint32_t*>(output.mapped);
        for(unsigned i=0;i<27;++i)if(words[i]!=0xCDCDCDCDU)throw std::runtime_error("Skinning overwrote another output slice");
        const auto value=[&](unsigned word){float result;std::memcpy(&result,words+word,sizeof(result));return result;};
        if(value(27)!=12.F || value(54)!=22.F || value(28)!=3.F || value(55)!=3.F)
            throw std::runtime_error("Skinning did not preserve independent joints and morph output");
        if(!batched)baseline.assign(words,words+81);
        else if(std::memcmp(baseline.data(),words,output.size))throw std::runtime_error("Batched deformation differs from individual slices");
    }
    cleanup.reset();
    if(host.errors || host.allocator.statistics().liveBuffers || host.allocator.statistics().liveImages)
        throw std::runtime_error("Skinning validation or release failed");
    std::cout<<"GPU independent joints, exact batched equivalence and output isolation passed\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
