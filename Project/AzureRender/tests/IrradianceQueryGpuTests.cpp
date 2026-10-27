#include "render/IrradianceQuery.hpp"
#include "support/IrradianceQueryFixture.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected query shader");
    auto cases=std::vector<std::array<float,16>>(test::nativeIrradianceQueries.begin(),test::nativeIrradianceQueries.end());
    cases.push_back({2,-3,4,0,0,1,0,0,0,0,0,0,2,-3,4,-1});
    cases.push_back({2,-3,4,.25F,0,1,0,0,0,0,0,0,2,-2.75F,4,-1});
    cases.push_back({2,-3,4,0,0,1,0,.2F,0,0,0,0,1.8F,-3.2F,3.8F,-1});
    cases.push_back(test::nativeIrradianceQueries[0]);cases.back()[8]+=.75F;cases.back()[9]+=.5F;cases.back()[10]+=.875F;
    std::vector<IrradianceQueryFrame> frames(cases.size());
    for(auto row:test::nativeIrradianceQueries){
        row[0]=-row[0];row[4]=-row[4];row[9]=1439-row[9];row[11]=7;row[12]=-row[12];
        cases.push_back(row);frames.push_back({1440,{-1,0,0,0,1,0,0,0,1}});
    }
    const float diagonal=std::sqrt(.5F);
    cases.push_back({2,-3,4,0,0,1,0,.2F,0,0,0,5,2,-3.2F,4-.2F*std::sqrt(2.F),-1});
    frames.push_back({0,{diagonal,0,diagonal,0,1,0,-diagonal,0,diagonal}});
    std::vector<std::array<float,28>> inputs(cases.size());
    for(std::size_t i=0;i<cases.size();++i){
        std::copy_n(cases[i].begin(),12,inputs[i].begin());
        for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column)
            inputs[i][12+row*4+column]=frames[i].worldToField[row*3+column];
        inputs[i][24]=float(frames[i].viewportHeight);
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
    input=host.allocator.createBuffer(inputs.size()*sizeof(inputs[0]),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(cases.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(input.mapped,inputs.data(),input.size);host.allocator.flush(input,0,input.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=1;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    pool=backend.createDescriptorPool(poolDesc);auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
        commands.dispatch(static_cast<unsigned>(cases.size()),1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);float maximum=0;
    for(std::size_t i=0;i<cases.size();++i){
        for(unsigned axis=0;axis<4;++axis){
            if(!std::isfinite(values[i*4+axis]))throw std::runtime_error("Nonfinite query result");
            maximum=std::max(maximum,std::abs(values[i*4+axis]-cases[i][12+axis]));
        }
        if(values[i*4+3]!=cases[i][15])throw std::runtime_error("Native signed noise differs");
    }
    if(maximum>2e-6F)throw std::runtime_error("Receiver GPU result differs: "+std::to_string(maximum));
    cleanup.reset();
    if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)
        throw std::runtime_error("Receiver query validation or lifetime failure");
    std::cout<<"Query cases="<<cases.size()<<", native noise exact, maximum position error="<<maximum<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
