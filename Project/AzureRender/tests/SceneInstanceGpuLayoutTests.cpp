#include "render/SceneInstanceGpuData.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cstring>
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected instance layout shader");
    static_assert(sizeof(SceneInstanceGpuData)==496);
    static_assert(SceneInstanceGpuData::kLayoutVersion==2);
    static_assert(offsetof(SceneInstanceGpuData,objectShadowIndex)==424);
    static_assert(offsetof(SceneInstanceGpuData,instanceFlags)==428);
    constexpr unsigned words=sizeof(SceneInstanceGpuData)/sizeof(std::uint32_t),count=words*2;
    std::array<std::uint32_t,count> expected{};
    for(unsigned i=0;i<count;++i){
        const auto field=i%words;
        if((field>=96&&field<100)||field==106||field==107)expected[i]=0xD1230000U+i;
        else{const float value=float(i)+.125F;std::memcpy(&expected[i],&value,sizeof(value));}
    }
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer input{},output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);
    });
    input=host.allocator.createBuffer(sizeof(expected),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(sizeof(expected),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(input.mapped,expected.data(),sizeof(expected));std::memset(output.mapped,0xCD,output.size);
    host.allocator.flush(input,0,input.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bufferBarrier({input.buffer,0,input.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(count,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto* actual=static_cast<const std::uint32_t*>(output.mapped);
    for(unsigned i=0;i<count;++i)if(actual[i]!=expected[i])throw std::runtime_error("Instance GPU layout mismatch at word "+std::to_string(i));
    if(host.errors.load())throw std::runtime_error("Instance layout Vulkan validation errors");
    std::cout<<"Instance layout: two records, "<<count<<" exact words, matrices and integer metadata passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
