#include "render/RenderViewService.hpp"
#include "support/MultisampleSceneProbe.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <iostream>
using namespace azurerender;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shaders");test::VulkanComputeFixture host(true,true);
    if(!host.backend->supportsSampleShading()||!host.properties.limits.standardSampleLocations)return 77;
    RenderContext base;base.rhi=host.backend.get();base.allocator=&host.allocator;base.device=host.device;base.physicalDevice=host.physical;
    base.graphicsQueue=host.queue;base.commandPool=host.pool;base.graphicsQueueFamily=host.family;base.descriptorLimits=host.properties.limits;
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.shadowMapSize=32;base.shaderDirectory=argv[1];base.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;
    std::vector<std::shared_ptr<test::MultisampleSceneObservation>> observations;unsigned checked=0;
    {
        RenderViewService service(base,[&](const std::string& id){auto observation=std::make_shared<test::MultisampleSceneObservation>();observation->supported=id!="unsupported";observations.push_back(observation);return std::make_unique<test::MultisampleSceneProbe>(observation);},[&]{vkDeviceWaitIdle(host.device);});
        RenderViewDescriptor descriptor;descriptor.extent={32,24};descriptor.settings.grade.toneMappingEnabled=false;descriptor.settings.antiAliasing=4;
        const auto multisample=service.create(descriptor);descriptor.settings.antiAliasing=0;const auto reference=service.create(descriptor);
        descriptor.settings.antiAliasing=4;descriptor.rendererId="unsupported";const auto fallback=service.create(descriptor);
        require(service.describe(multisample).at("rasterizationSamples")==4&&service.describe(multisample).at("temporalBytes")==0,"Actual GPU auxiliary target must activate pure four-sample mode");
        require(service.describe(reference).at("rasterizationSamples")==1&&service.describe(fallback).at("rasterizationSamples")==1,"Reference and fallback remain independent single-sample views");
        SceneFrameData frame;std::string error;
        for(unsigned phase=0;phase<3;++phase){
            if(phase==2)for(auto view:{multisample,reference,fallback})service.resize(view,{40,32});
            base.currentFrame=phase%2;for(auto view:{multisample,reference,fallback})service.request(view);
            RenderGraph graph;service.schedule(graph,frame,base,phase+1);require(graph.compile(error),error.c_str());
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});require(service.submitted(phase+1),"Actual auxiliary GPU recording must commit");service.complete(phase+1);
            const auto expected=service.readPixels(reference);require(!expected.empty()&&expected[2]>expected[0],"Readback must contain the probe color");
            for(auto view:{multisample,fallback}){const auto actual=service.readPixels(view);require(actual==expected,"Shared four-sample and fallback output must match uniform single-sample reference after grading");checked+=static_cast<unsigned>(actual.size());}
        }
        require(observations[0]->framebuffers.size()==2&&observations[0]->framebuffers[0]!=observations[0]->framebuffers[1],"GPU submissions use distinct in-flight source framebuffers");
        service.clear();service.complete(3);
    }
    require(host.errors.load()==0&&!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"GPU view validation or retirement failed");
    std::cout<<"Multisample auxiliary GPU: "<<checked<<" output bytes, independent quality, spatial fallback, two source slots, resize and retirement passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
