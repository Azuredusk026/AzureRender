#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <fstream>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=3)throw std::invalid_argument("Expected surface shader and showcase catalog");
    std::ifstream input(argv[2]);nlohmann::json catalog;input>>catalog;
    const float contrast=catalog.at("looks").at(1).at("grade").at("contrast");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);
    });
    output=host.allocator.createBuffer(24*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memset(output.mapped,0xCD,output.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,sizeof(float)};
    layout=backend.createPipelineLayout(setLayout,&range);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
        commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&contrast,sizeof(contrast));commands.dispatch(24,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto values=static_cast<const float*>(output.mapped);
    const double pi=3.141592653589793;
    const double expected[]={0,.04,.04/(4*pi*std::pow(.2,4)),.04/(4*pi*std::pow(.8,4)),0,0, 1,.875,.8125, 1,.875,.8125, 1,1,1, 1,1,1, 1,1,1, 1,1,1};
    bool passed=true;
    for(unsigned i=0;i<24;++i){
        std::cout<<"Surface probe "<<i<<": "<<values[i]<<" (expected "<<expected[i]<<")\n";
        passed&=std::isfinite(values[i]) && std::abs(values[i]-expected[i])<std::max(1e-6,std::abs(expected[i])*1e-4);
    }
    if(!passed)throw std::runtime_error("Black point or microfacet material energy contract failed");
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
