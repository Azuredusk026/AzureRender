#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected reflection surface shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    std::vector<std::array<float,4>> queries;
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z)
        if(x||y||z)queries.push_back({float(x),float(y),float(z),float(queries.size()%5)/4});
    rhi::GpuBuffer input{},output{};VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(descriptors)backend.destroyDescriptorSetLayout(descriptors);
        host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);
    });
    input=host.allocator.createBuffer(queries.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(queries.size()*32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(input.mapped,queries.data(),static_cast<std::size_t>(input.size));host.allocator.flush(input,0,input.size);
    descriptors=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    pool=backend.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}},1});
    const auto set=backend.allocateDescriptorSets(pool,descriptors,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(descriptors,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(static_cast<unsigned>(queries.size()),1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);float maximum=0;
    for(std::size_t i=0;i<queries.size();++i){const auto& q=queries[i];const auto* p=values+i*8;
        if(p[0]!=123.5F+float(i)||p[3]!=q[3]||p[7]!=q[3]||p[1]<0||p[1]>1||p[2]<0||p[2]>1)
            throw std::runtime_error("Surface packing must preserve depth, range and roughness");
        const float length=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]);
        for(unsigned c=0;c<3;++c){if(!std::isfinite(p[c+4]))throw std::runtime_error("Nonfinite decoded normal");maximum=std::max(maximum,std::abs(p[c+4]-q[c]/length));}
    }
    if(maximum>1e-6F)throw std::runtime_error("World normals must roundtrip through both hemispheres and zero components");
    cleanup.reset();if(host.errors||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)throw std::runtime_error("Surface GPU validation or resource retirement failed");
    std::cout<<"PASS: normals="<<queries.size()<<" maximumError="<<maximum<<" validation=0 resources=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
