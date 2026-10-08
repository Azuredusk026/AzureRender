#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <cmath>
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=2)throw std::invalid_argument("Expected shadow comparison shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);
    });
    output=host.allocator.createBuffer(16*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memset(output.mapped,0xCD,output.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(16,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto values=static_cast<const float*>(output.mapped);
    for(unsigned i=0;i<15;++i)if(std::abs(values[i]-float(i%5)/4)>1e-6F)throw std::runtime_error("Depth step must preserve fractional shadow visibility");
    if(std::abs(values[15]-1.F)>1e-6F)throw std::runtime_error("An unoccluded sloped receiver must remain fully visible across the filter footprint");
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    std::cout<<"15 GPU depth-step samples passed at three receiver depths\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
