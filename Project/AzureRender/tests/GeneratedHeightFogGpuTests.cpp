#include "render/GeneratedHeightFogGpu.hpp"
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
constexpr float tolerance=.0006F;

std::vector<Sample> samplePositions(){
    std::vector<Sample> positions;
    for(unsigned z:{0U,1U,7U,13U,24U})
        for(unsigned y:{0U,3U,6U})
            for(unsigned x:{0U,6U,12U})
                positions.push_back({(x+.5F)/extent[0],(y+.5F)/extent[1],(z+.5F)/extent[2],0});
    return positions;
}

Sample oracle(const AnalyticFogSettings& medium,const CameraViewDescriptor& camera,const Sample& sample){
    const auto ray=cameraPickRay(camera,sample[0],sample[1],16.F/9);
    const auto forward=internal::normalize(internal::subtract(camera.target,camera.position));
    const float fraction=(sample[2]*extent[2]-.5F)/(extent[2]-1);
    const float start=medium.height->startViewDepth;
    const float depth=start+static_cast<float>(std::expm1(std::log1p(double(farDepth)-start)*fraction));
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
        height.startViewDepth=.75F;
        height.minimumTransmittance=.05F;
        height.transmittanceBias={.01F,.001F,0,0};
        height.layers={{.19F,.13F,1,{.19F,.29F,.48F}},{.04F,.03F,0,{.4F,.1F,.05F}}};
        FogVolumeData identity{{1,1,1},{0,0,0,0x3c00}};
        ViewFogGpu parameters(backend,binding,identity,2);
        std::vector<rhi::GpuBuffer> frames{parameters.parameters(0),parameters.parameters(1)};
        GeneratedHeightFogGpu generator(backend,extent,frames,readBinaryFile(argv[1]));
        if(!generator.image(0)||!generator.image(1)||generator.image(0)==generator.image(1))
            throw std::runtime_error("Generated volumes require distinct in-flight images");

        rhi::GpuBuffer input{},output{},projection{};
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
            host.allocator.destroyBuffer(projection);
        });

        input=host.allocator.createBuffer(positions.size()*sizeof(Sample),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        output=host.allocator.createBuffer(positions.size()*sizeof(Sample)*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        projection=host.allocator.createBuffer(sizeof(PackedFogViewProjection),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(input.mapped,positions.data(),std::size_t(input.size));
        host.allocator.flush(input,0,input.size);
        const auto unusedProjection=FogViewProjection{}.pack();
        std::memcpy(projection.mapped,&unusedProjection,sizeof(unusedProjection));
        host.allocator.flush(projection,0,projection.size);
        descriptors=backend.createDescriptorSetLayout({
            {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc description;
        description.maxSets=2;
        description.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6}};
        pool=backend.createDescriptorPool(description);
        const auto sets=backend.allocateDescriptorSets(pool,descriptors,2);
        for(unsigned slot=0;slot<2;++slot){
            generator.writeTexture(sets[slot],0,slot);
            backend.writeDescriptorBuffer({sets[slot],1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
            backend.writeDescriptorBuffer({sets[slot],2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
            backend.writeDescriptorBuffer({sets[slot],3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,projection.buffer,projection.size});
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
        for(unsigned iteration=0;iteration<4;++iteration){
            const unsigned slot=iteration%2;
            const unsigned view=slot^(iteration/2);
            const auto& camera=view?second:first;
            parameters.updateFrame(slot,camera,{320,180});
            const auto& buffer=frames[slot];
            PackedViewFogFrame packed{};
            std::memcpy(&packed,buffer.mapped,sizeof(packed));
            packed.rows[13][3]=farDepth;
            std::memcpy(buffer.mapped,&packed,sizeof(packed));
            host.allocator.flush(buffer,0,buffer.size);

            RenderGraph graph;
            rhi::BufferBarrierDesc hostWrite;
            hostWrite.buffer=buffer.buffer;
            hostWrite.size=buffer.size;
            hostWrite.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
            hostWrite.dstAccessMask=VK_ACCESS_HOST_WRITE_BIT;
            const auto volume=generator.addPass(graph,slot,graph.importBuffer("fog-parameters",hostWrite));
            const auto sample=graph.addCommandPass("fog-volume-oracle",[&](rhi::ICommandRecorder& commands){
                commands.bindComputePipeline(pipeline);
                commands.bindComputeDescriptorSet(layout,sets[slot]);
                commands.dispatch(static_cast<unsigned>(positions.size()),1,1);
                commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            });
            graph.use(sample,volume,RenderGraphUsage::ComputeSampled,false);
            std::string error;
            if(!graph.compile(error))throw std::runtime_error(error);
            const auto barrier=std::find_if(graph.barriers().begin(),graph.barriers().end(),[&](const auto& entry){
                return entry.resource==volume&&entry.pass==sample;
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
                std::memcpy(actual.data(),static_cast<const char*>(output.mapped)+i*sizeof(Sample)*4,sizeof(actual));
                const auto expected=oracle(*binding.analytic,camera,positions[i]);
                for(unsigned channel=0;channel<actual.size();++channel){
                    if(!std::isfinite(actual[channel]))throw std::runtime_error("Generated fog sample is nonfinite");
                    maximum=std::max(maximum,std::abs(actual[channel]-expected[channel]));
                }
                if(iteration<2)expectedCapture.push_back(actual);
                else if(expectedCapture[i]!=actual)throw std::runtime_error("Repeated view changed generated fog samples");
            }
        }
        if(captured[0]==captured[1])throw std::runtime_error("Generated volume must respond to camera changes");
        if(maximum>tolerance)throw std::runtime_error("Generated height fog exceeds error budget: "+std::to_string(maximum));
    }
    const auto statistics=host.allocator.statistics();
    if(host.errors.load()||statistics.liveImages||statistics.liveBuffers)
        throw std::runtime_error("Generated height fog has validation errors or live allocations");
    std::cout<<std::setprecision(10)<<host.properties.deviceName<<": "<<positions.size()*4
        <<" generated height fog samples, maximum error "<<maximum<<", allocations: 0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
