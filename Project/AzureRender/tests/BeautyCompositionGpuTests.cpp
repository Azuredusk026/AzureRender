#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>
using namespace azurerender;
namespace {
using Vector=std::array<float,4>;
struct Case {std::array<Vector,12> values{};};
static_assert(sizeof(Case)==192);
float orderedSum(const Case& input,unsigned channel){
    volatile float result=input.values[0][channel];
    for(unsigned i=1;i<8;++i)result=result+input.values[i][channel];
    return result;
}
float orderedReplace(float total,float added,float removed){
    volatile float difference=added-removed;
    volatile float result=total+difference;return result;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    std::array<Case,63> cases{};std::array<Vector,189> expected{};
    for(unsigned i=0;i<cases.size();++i){
        const int exponent=int(i/3)-10;const unsigned pattern=i%3;
        auto& value=cases[i].values;
        for(unsigned channel=0;channel<3;++channel){
            const float small=std::ldexp(1.F,exponent+int(channel));
            const float large=std::ldexp(small,24);
            if(pattern==0){value[0][channel]=large;value[1][channel]=small;value[2][channel]=-large;}
            else if(pattern==1){value[0][channel]=large;value[1][channel]=-large;value[2][channel]=small;}
            else {value[0][channel]=small;value[1][channel]=large;value[2][channel]=-large;}
            value[8][channel]=small;value[9][channel]=large;value[10][channel]=1.F;
            value[11][channel]=large*2.F;
            expected[i*3][channel]=orderedSum(cases[i],channel);
            expected[i*3+1][channel]=orderedReplace(small,large,1.F);
            const float mixed=large*1.5F;value[8+channel][3]=mixed;
            expected[i*3+2][channel]=orderedReplace(small,mixed,mixed);
        }
        value[11][3]=.5F;
        for(unsigned mode=0;mode<3;++mode)expected[i*3+mode][3]=float(i+1);
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
    input=host.allocator.createBuffer(sizeof(cases),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(sizeof(expected),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(input.mapped,cases.data(),sizeof(cases));std::memset(output.mapped,0xcd,sizeof(expected));
    host.allocator.flush(input,0,input.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc description;description.maxSets=1;description.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    pool=backend.createDescriptorPool(description);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=backend.createPipelineLayout(setLayout,nullptr);
    shader=backend.createShaderModule(readBinaryFile(std::string(argv[1])+"/beauty_composition_test.comp.spv"));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bufferBarrier({input.buffer,0,input.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(1,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);
    if(std::memcmp(output.mapped,expected.data(),sizeof(expected)))throw std::runtime_error("Beauty composition must preserve ordered binary32 sums, cancellation and reflection replacement");
    cleanup.reset();
    if(host.errors.load()||host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)
        throw std::runtime_error("Beauty composition validation or allocation retirement failed");
    std::cout<<"Beauty composition: "<<host.properties.deviceName<<", 63 cases, 567 ordered components, synchronization and retirement passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
