#include "support/VulkanComputeFixture.hpp"
#include "render/ColorGradingLutGpu.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=2)throw std::invalid_argument("Expected color grading LUT shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    const std::array<std::array<float,4>,11> probes{{
        {{0,0,0,1}},{{1,1,1,1}},{{.13F,.7F,.55F,1}},{{-1,.5F,2,1}},{{.2F,.3F,.33333334F,1}},
        {{.18F,.18F,.18F,1}},{{.00001F,.001F,.01F,1}},{{2,4,8,1}},{{10000,1,.1F,1}},{{.9F,.1F,.99999F,1}},{{0,-.009F,-1,1}}}};
    rhi::GpuBuffer input{},output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);
    });
    input=host.allocator.createBuffer(sizeof(probes),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    output=host.allocator.createBuffer(sizeof(probes)*3,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(input.mapped,probes.data(),sizeof(probes));host.allocator.flush(input,0,input.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
    backend.writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,16};
    layout=backend.createPipelineLayout(setLayout,&range);shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
    unsigned checked=0;float maximumIdealError=0,maximumIdealBound=0,maximumQuantizedReferenceError=0,maximumUnfilteredReferenceError=0;
    for(unsigned profile=0;profile<2;++profile){
        auto lut=makeIdentityColorGradingLut(4);
        if(profile)for(std::size_t i=0;i<lut.image.rgba32.size();i+=4){
            const float r=lut.image.rgba32[i],g=lut.image.rgba32[i+1],b=lut.image.rgba32[i+2];
            lut.image.rgba32[i]=r*.7F+g*.3F;lut.image.rgba32[i+1]=1-b;lut.image.rgba32[i+2]=r*g*b;
        }
        ColorGradingLutGpu resource(backend,lut);
        std::array<float,3> redGradient{},greenGradient{};
        for(unsigned g=0;g<4;++g)for(unsigned b=0;b<4;++b)for(unsigned r=0;r<4;++r){
            const auto offset=(std::size_t(g)*16+b*4+r)*4;
            for(unsigned channel=0;channel<3;++channel){
                if(r<3)redGradient[channel]=std::max(redGradient[channel],std::abs(lut.image.rgba32[offset+4+channel]-lut.image.rgba32[offset+channel]));
                if(g<3)greenGradient[channel]=std::max(greenGradient[channel],std::abs(lut.image.rgba32[offset+64+channel]-lut.image.rgba32[offset+channel]));
            }
        }
        backend.writeDescriptorImage({set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,resource.view(),resource.sampler(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        for(unsigned domain=0;domain<2;++domain)for(unsigned encode=0;encode<2;++encode){
            lut.inputDomain=static_cast<ColorGradingInputDomain>(domain);
            const std::array<float,4> parameters{4,1.7F,float(domain),float(encode)};
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
                commands.bufferBarrier({input.buffer,0,input.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
                commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,parameters.data(),sizeof(parameters));commands.dispatch(static_cast<unsigned>(probes.size()),1,1);
                commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            });
            host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);
            for(std::size_t i=0;i<probes.size();++i){
                const auto expected=sampleColorGradingLut(lut,{probes[i][0],probes[i][1],probes[i][2]},1.7F,static_cast<ColorGradingOutputEncoding>(encode));
                std::array<float,3> coordinates{};
                for(unsigned axis=0;axis<3;++axis)coordinates[axis]=colorGradingInputCoordinate(probes[i][axis]*1.7F,lut.inputDomain);
                auto quantized=coordinates;
                const float filterScale=std::ldexp(1.F,static_cast<int>(host.properties.limits.subTexelPrecisionBits));
                for(unsigned axis=0;axis<2;++axis){
                    const float lattice=coordinates[axis]*3,lower=std::floor(lattice);
                    quantized[axis]=(lower+std::round((lattice-lower)*filterScale)/filterScale)/3;
                }
                auto linearReference=lut;linearReference.inputDomain=ColorGradingInputDomain::Linear;
                const auto filtered=sampleColorGradingLut(linearReference,quantized,1,static_cast<ColorGradingOutputEncoding>(encode));
                for(unsigned channel=0;channel<3;++channel){
                    const auto actual=values[i*12+channel];
                    const auto idealError=std::abs(actual-expected[channel]);maximumIdealError=std::max(maximumIdealError,idealError);
                    const float idealBound=.5F/filterScale*(redGradient[channel]+greenGradient[channel])*(encode?12.92F:1.F)+2e-5F;
                    maximumIdealBound=std::max(maximumIdealBound,idealBound);
                    if(idealError>idealBound)throw std::runtime_error("GPU LUT filtering exceeds the device-precision interpolation error bound");
                    if(!std::isfinite(actual))throw std::runtime_error("GPU LUT sampler returned a non-finite value");
                    maximumQuantizedReferenceError=std::max(maximumQuantizedReferenceError,std::abs(actual-filtered[channel]));
                    ++checked;
                    if(!std::isfinite(values[i*12+4+channel])||std::abs(values[i*12+4+channel]-coordinates[channel])>2e-6F)
                        throw std::runtime_error("GPU color grading input-domain formula mismatch");
                    ++checked;
                    const auto reference=values[i*12+8+channel];
                    maximumUnfilteredReferenceError=std::max(maximumUnfilteredReferenceError,std::abs(reference-expected[channel]));
                    if(!std::isfinite(reference)||std::abs(reference-expected[channel])>2e-5F)
                        throw std::runtime_error("GPU texel-fetch interpolation reference disagrees with CPU LUT sampling");
                    ++checked;
                }
            }
        }
    }
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    std::cout<<"Public LUT upload and shared production GLSL passed "<<checked<<" numeric probes on "<<host.properties.deviceName
        <<"; subTexelPrecisionBits="<<host.properties.limits.subTexelPrecisionBits<<"; maximum ideal sampler error="<<maximumIdealError
        <<"; conservative ideal bound="<<maximumIdealBound<<"; maximum quantized reference residual="<<maximumQuantizedReferenceError
        <<"; maximum texel-fetch reference error="<<maximumUnfilteredReferenceError<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
