#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "assets/MaterialOutputPrecision.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
using namespace azurerender;
namespace {
using Words=std::array<std::uint32_t,4>;
std::uint32_t bits(float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;}
float number(std::uint32_t word){float value;std::memcpy(&value,&word,4);return value;}
float reference(float value,unsigned mantissa){
    if(std::isnan(value))return number(0x7fc00000U);
    if(value<=0)return 0;
    if(std::isinf(value))return value;
    const double maximum=std::ldexp(2.-std::ldexp(1.,-int(mantissa)),15);
    const double bounded=std::min(double(value),maximum);int exponent;
    std::frexp(bounded,&exponent);
    const double quantum=std::ldexp(1.,std::max(exponent-1,-14)-int(mantissa));
    return float(std::floor(bounded/quantum)*quantum);
}
std::uint32_t half(unsigned word){
    const auto sign=word>>15,exponent=(word>>10)&31,mantissa=word&1023;
    if(exponent==31)return (sign<<31)|0x7f800000U|(mantissa?0x00400000U|(mantissa<<13):0);
    const double value=exponent?std::ldexp(1.+mantissa/1024.,int(exponent)-15):std::ldexp(double(mantissa),-24);
    return bits(float(sign?-value:value));
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected output precision test shader");
    std::vector<Words> inputs;
    const auto append=[&](std::uint32_t word){inputs.push_back({word,word,word,word^0xdeadbeefU});};
    for(unsigned word=0;word<65536;++word)append(half(word));
    for(unsigned exponent=0;exponent<256;++exponent)
        for(unsigned mantissa:{0U,1U,0x1ffffU,0x20000U,0x3fffffU,0x7dffffU,0x7ffffeU,0x7fffffU})
            for(unsigned sign=0;sign<2;++sign)append((sign<<31)|(exponent<<23)|mantissa);
    std::uint32_t random=0x53a14907U;
    for(unsigned i=0;i<32768;++i){random=random*1664525U+1013904223U;append(random);}
    const auto count=static_cast<unsigned>(inputs.size());
    std::vector<Words> expected;expected.reserve(count);
    for(const auto& words:inputs){
        const std::array<float,4> source{number(words[0]),number(words[1]),number(words[2]),number(words[3])};
        const auto actual=quantizeMaterialOutputColor(source);Words referenceWords{bits(reference(source[0],6)),bits(reference(source[1],6)),bits(reference(source[2],5)),words[3]};
        for(unsigned component=0;component<4;++component)if(bits(actual[component])!=referenceWords[component])
            throw std::runtime_error("CPU output precision differs from independent reference");
        expected.push_back(referenceWords);
    }
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer source{},destination{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(destination);host.allocator.destroyBuffer(source);
    });
    const auto bytes=inputs.size()*sizeof(Words);
    source=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    destination=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(source.mapped,inputs.data(),bytes);host.allocator.flush(source,0,source.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,source.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,destination.buffer,destination.size});
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,8};layout=backend.createPipelineLayout(setLayout,&range);
    shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
    for(unsigned mode=0;mode<2;++mode){
        std::memset(destination.mapped,0xCD,bytes);host.allocator.flush(destination,0,destination.size);
        const std::array<unsigned,2> parameters{count,mode};
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            commands.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
            commands.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
            commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,parameters.data(),8);commands.dispatch((count+63)/64,1,1);
            commands.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(destination,0,destination.size);const auto values=static_cast<const Words*>(destination.mapped);
        const auto& oracle=mode?expected:inputs;
        for(unsigned i=0;i<count;++i)if(values[i]!=oracle[i]){
            std::cerr<<"mode="<<mode<<" input="<<i<<" bits="<<std::hex<<inputs[i][0]<<std::dec<<'\n';
            throw std::runtime_error("GPU output precision must match all four components bitwise");
        }
    }
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    cleanup.reset();
    std::cout<<"Output precision passed "<<count<<" inputs, "<<count*8<<" GPU component comparisons; all half values, signed F32 boundaries, random bits and alpha identity\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
