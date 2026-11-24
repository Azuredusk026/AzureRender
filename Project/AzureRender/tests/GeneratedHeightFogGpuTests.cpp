#include "render/CameraProjection.hpp"
#include "render/FogVolumeData.hpp"
#include "render/ViewFogGpu.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>

using namespace azurerender;
namespace {
using Sample = std::array<float,4>;
constexpr std::array<unsigned,3> extent{13,7,25};
constexpr float farDepth=16.F;
constexpr float startDepth=.75F;
constexpr float tolerance=.0006F;

std::vector<Sample> samplePositions(){
    std::vector<Sample> positions;
    for(unsigned z:{0U,1U,7U,13U,24U})
        for(unsigned y:{0U,3U,6U})
            for(unsigned x:{0U,6U,12U})
                positions.push_back({(x+.5F)/extent[0],(y+.5F)/extent[1],
                    startDepth+static_cast<float>(std::expm1(std::log1p(double(farDepth)-startDepth)*z/(extent[2]-1))),0});
    positions.push_back({.25F,.75F,startDepth-.25F,0});
    positions.push_back({.75F,.25F,farDepth+4.F,0});
    return positions;
}

Sample oracle(const AnalyticFogSettings& medium,const CameraViewDescriptor& camera,const Sample& sample){
    const auto ray=cameraPickRay(camera,sample[0],sample[1],16.F/9);
    const auto forward=internal::normalize(internal::subtract(camera.target,camera.position));
    const float depth=sample[2];
    const auto end=internal::addVectors(ray.origin,internal::scaleVector(ray.direction,depth/internal::dot(ray.direction,forward)));
    const auto expected=evaluateAnalyticFog(medium,ray.origin,end,forward);
    return {expected.scattering[0],expected.scattering[1],expected.scattering[2],expected.transmittance[0]};
}
}

int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Expected generation and sampling compute programs");
    test::VulkanComputeFixture host;
    auto& backend=*host.backend;
    float maximum=0;
    const auto positions=samplePositions();
    {
        ViewFogBinding binding;
        binding.enabled=true;
        binding.analytic=AnalyticFogSettings{};
        binding.analytic->height=LayeredHeightFog{};
        auto& height=*binding.analytic->height;
        height.startViewDepth=startDepth;
        height.minimumTransmittance=.05F;
        height.transmittanceBias={.01F,.001F,0,0};
        height.layers={{.19F,.13F,1,{.19F,.29F,.48F}},{.04F,.03F,0,{.4F,.1F,.05F}}};
        binding.generatedHeightVolume=GeneratedHeightFogSettings{extent,farDepth};
        FogVolumeData identity{{1,1,1},{0,0,0,0x3c00}};
        ViewFogGpu fog(backend,binding,identity,2,readBinaryFile(argv[1]));

        rhi::GpuBuffer input{},output{};
        VkDescriptorSetLayout descriptors{};
        VkDescriptorPool pool{};
        VkPipelineLayout layout{};
        VkShaderModule shader{};
        VkPipeline pipeline{};
        const auto release=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
            if(pipeline)backend.destroyPipeline(pipeline);
            if(shader)backend.destroyShaderModule(shader);
            if(layout)backend.destroyPipelineLayout(layout);
            if(pool)backend.destroyDescriptorPool(pool);
            if(descriptors)backend.destroyDescriptorSetLayout(descriptors);
            host.allocator.destroyBuffer(input);
            host.allocator.destroyBuffer(output);
        });

        input=host.allocator.createBuffer(positions.size()*sizeof(Sample),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        output=host.allocator.createBuffer(positions.size()*sizeof(Sample),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(input.mapped,positions.data(),std::size_t(input.size));
        host.allocator.flush(input,0,input.size);
        descriptors=backend.createDescriptorSetLayout({
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {34,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {35,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc description;
        description.maxSets=2;
        description.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6}};
        pool=backend.createDescriptorPool(description);
        const auto sets=backend.allocateDescriptorSets(pool,descriptors,2);
        for(unsigned slot=0;slot<2;++slot){
            fog.writeDescriptors(sets[slot],slot,34,35);
            backend.writeDescriptorBuffer({sets[slot],1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
            backend.writeDescriptorBuffer({sets[slot],2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
        }
        layout=backend.createPipelineLayout(descriptors,nullptr);
        shader=backend.createShaderModule(readBinaryFile(argv[2]));
        pipeline=backend.createComputePipeline({shader,layout});

        CameraViewDescriptor first;
        first.position={0,1,4};
        first.target={0,0,0};
        first.projectionOffsetX=.13F;
        first.projectionOffsetY=-.11F;
        CameraViewDescriptor second=first;
        second.position={1,3,4};
        second.target={0,2,0};
        second.projection=CameraProjectionKind::Orthographic;
        second.orthoHeight=6;
        std::array<std::vector<Sample>,2> captured;
        std::array<VkImage,2> images{};
        for(unsigned iteration=0;iteration<4;++iteration){
            const unsigned slot=iteration%2;
            const unsigned view=slot^(iteration/2);
            const auto& camera=view?second:first;
            if(fog.updateFrame(slot,camera,{320,180})!=FogViewMatch::GeneratedHeight)
                throw std::runtime_error("Missing generated height fog activation");
            const auto& buffer=fog.parameters(slot);

            RenderGraph graph;
            rhi::BufferBarrierDesc hostWrite;
            hostWrite.buffer=buffer.buffer;
            hostWrite.size=buffer.size;
            hostWrite.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
            hostWrite.dstAccessMask=VK_ACCESS_HOST_WRITE_BIT;
            const auto parameters=graph.importBuffer("fog-parameters",hostWrite);
            const auto volume=fog.addGenerationPass(graph,slot,parameters);
            if(!volume)throw std::runtime_error("Missing height fog generation pass");
            images[slot]=graph.resources()[*volume].initial.image;
            if(!images[slot]||images[0]==images[1])
                throw std::runtime_error("Generated volumes require distinct in-flight images");
            const auto sample=graph.addCommandPass("fog-volume-oracle",[&](rhi::ICommandRecorder& commands){
                commands.bindComputePipeline(pipeline);
                commands.bindComputeDescriptorSet(layout,sets[slot]);
                commands.dispatch(static_cast<unsigned>(positions.size()),1,1);
                commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            });
            graph.use(sample,parameters,RenderGraphUsage::Storage,false);
            graph.use(sample,*volume,RenderGraphUsage::ComputeSampled,false);
            std::string error;
            if(!graph.compile(error))throw std::runtime_error(error);
            const auto barrier=std::find_if(graph.barriers().begin(),graph.barriers().end(),[&](const auto& entry){
                return entry.resource==*volume&&entry.pass==sample;
            });
            if(barrier==graph.barriers().end()||barrier->image.oldLayout!=VK_IMAGE_LAYOUT_GENERAL
                ||barrier->image.newLayout!=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                ||!(barrier->image.srcAccessMask&VK_ACCESS_SHADER_WRITE_BIT)
                ||!(barrier->image.dstAccessMask&VK_ACCESS_SHADER_READ_BIT))
                throw std::runtime_error("Generated height fog needs a storage-write to sampled-read barrier");
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
            host.allocator.invalidate(output,0,output.size);
            auto& expectedCapture=captured[view];
            for(std::size_t i=0;i<positions.size();++i){
                Sample actual;
                std::memcpy(actual.data(),static_cast<const char*>(output.mapped)+i*sizeof(Sample),sizeof(actual));
                const auto expected=oracle(*binding.analytic,camera,positions[i]);
                for(unsigned channel=0;channel<actual.size();++channel){
                    if(!std::isfinite(actual[channel]))throw std::runtime_error("Generated fog sample is nonfinite");
                    maximum=std::max(maximum,std::abs(actual[channel]-expected[channel]));
                }
                if(iteration<2)expectedCapture.push_back(actual);
                else if(expectedCapture[i]!=actual)throw std::runtime_error("Repeated view changed generated fog samples");
            }
        }
        bool changesWithCamera=false;
        for(std::size_t i=0;i<positions.size();++i)
            if(positions[i][2]>startDepth&&positions[i][2]<=farDepth&&captured[0][i]!=captured[1][i])
                changesWithCamera=true;
        if(!changesWithCamera)throw std::runtime_error("Generated volume must respond to camera changes");
        if(maximum>tolerance)throw std::runtime_error("Generated height fog exceeds error budget: "+std::to_string(maximum));
    }
    const auto statistics=host.allocator.statistics();
    if(host.errors.load()||statistics.liveImages||statistics.liveBuffers)
        throw std::runtime_error("Generated height fog has validation errors or live allocations");
    std::cout<<std::setprecision(10)<<host.properties.deviceName<<": "<<positions.size()*4
        <<" generated height fog samples, maximum error "<<maximum<<", allocations: 0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
