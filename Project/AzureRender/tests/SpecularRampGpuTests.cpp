#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <vector>
using namespace azurerender;
struct alignas(16) Probe {
    std::array<float,4> normal,view,camera,light,surface,ramp,f0,mainColor,weights,controls;
    std::array<float,12> expected;
};
static_assert(offsetof(Probe,expected)==160);
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected specular ramp test shader");
    std::vector<Probe> probes;
#include "fixtures/SpecularRampCases.inl"
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer input{},output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);host.allocator.destroyBuffer(input);
    });
    input=host.allocator.createBuffer(probes.size()*160,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(probes.size()*48,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    for(std::size_t i=0;i<probes.size();++i)std::memcpy(static_cast<char*>(input.mapped)+i*160,&probes[i],160);
    std::memset(output.mapped,0xcd,output.size);host.allocator.flush(input,0,input.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};pool=backend.createDescriptorPool(desc);
    const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(setLayout,{});shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
    backend.executeOneShot([&](rhi::ICommandRecorder& commands){
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(static_cast<std::uint32_t>(probes.size()),1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);bool passed=true;
    for(std::size_t i=0;i<probes.size();++i)for(unsigned axis=0;axis<12;++axis){
        const float actual=values[i*12+axis],expected=probes[i].expected[axis],tolerance=axis==3?2e-4F:1e-5F;
        if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance){passed=false;std::cerr<<std::setprecision(10)<<"Specular ramp probe "<<i<<" axis "<<axis<<": "<<actual<<" expected "<<expected<<'\n';}
    }
    cleanup.reset();
    if(!passed||host.errors.load()||host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)
        throw std::runtime_error("Specular ramp GPU contract or lifetime failed");
    std::cout<<"Four captured specular points and twelve independent controls passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
