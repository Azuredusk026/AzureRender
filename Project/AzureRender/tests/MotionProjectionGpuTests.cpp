#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
using namespace azurerender;
struct MotionProbe {std::array<float,4> current,previous,flags,expected;};
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected motion probe shader");
    const std::array<float,4> c{.2F,-.4F,.5F,1},valid{1,0,0,0},zero{};
    const float infinity=std::numeric_limits<float>::infinity(),nan=std::numeric_limits<float>::quiet_NaN();
    const std::array<MotionProbe,18> probes{{
        {c,c,valid,{0,0,.5F,1}},
        {c,{-.2F,-.2F,.3F,1},valid,{.2F,-.1F,.3F,1}},
        {{.4F,-.8F,1,2},{-.6F,-.6F,.9F,3},valid,{.2F,-.1F,.3F,1}},
        {c,{0,0,.5F,-1},valid,zero},{c,{0,0,.5F,0},valid,zero},
        {c,{0,0,-.1F,1},valid,zero},{c,{1.1F,0,.5F,1},valid,zero},
        {{0,0,.5F,0},c,valid,zero},{{0,0,-.1F,1},c,valid,zero},
        {c,{infinity,0,.5F,1},valid,zero},{c,{0,0,0,1},valid,{.1F,-.2F,0,1}},
        {c,{0,0,1,1},valid,{.1F,-.2F,1,1}},
        {c,{0,0,5e-9F,1e-8F},valid,{.1F,-.2F,.5F,1}},
        {c,{nan,0,.5F,1},valid,zero},{c,{-1,1,.5F,1},valid,{.6F,-.7F,.5F,1}},
        {{1.1F,0,.5F,1},c,valid,zero},{{0,0,1.1F,1},c,valid,zero},{c,c,zero,zero}}};
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer input{},output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);host.allocator.destroyBuffer(input);
    });
    input=host.allocator.createBuffer(probes.size()*48,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(probes.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    for(std::size_t i=0;i<probes.size();++i)std::memcpy(static_cast<char*>(input.mapped)+i*48,&probes[i],48);
    std::memset(output.mapped,0xcd,static_cast<std::size_t>(output.size));host.allocator.flush(input,0,input.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};pool=backend.createDescriptorPool(desc);
    const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(setLayout,{});shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    backend.executeOneShot([&](rhi::ICommandRecorder& commands){
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
        commands.dispatch(static_cast<std::uint32_t>(probes.size()),1,1);
        rhi::BufferBarrierDesc barrier{};barrier.buffer=output.buffer;barrier.size=output.size;
        barrier.srcStageMask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
        barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(barrier);
    });
    host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);
    bool passed=true;for(std::size_t i=0;i<probes.size();++i)for(std::size_t axis=0;axis<4;++axis){
        const auto value=values[i*4+axis];const bool good=std::isfinite(value)&&std::abs(value-probes[i].expected[axis])<1e-6F;
        if(!good)std::cerr<<"Motion probe "<<i<<" axis "<<axis<<": "<<value<<" expected "<<probes[i].expected[axis]<<'\n';passed&=good;
    }
    if(!passed||host.errors.load())throw std::runtime_error("Motion GPU projection contract failed");
    std::cout<<"Motion projection: 18 cases, 72 scalar checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
