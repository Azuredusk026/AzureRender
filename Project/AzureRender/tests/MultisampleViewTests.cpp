#include "render/RenderViewService.hpp"
#include "rhi/NullRhi.hpp"
#include "support/MultisampleSceneProbe.hpp"
#include <iostream>
#include <algorithm>
using namespace azurerender;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shaders");rhi::NullRhi backend;rhi::NullCommandRecorder commands;RenderContext base;
    base.rhi=&backend;base.allocator=&backend.allocator();base.device=reinterpret_cast<VkDevice>(1);base.commands=&commands;base.shaderDirectory=argv[1];
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;base.descriptorLimits.standardSampleLocations=VK_TRUE;
    backend.supportedSamples=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;backend.sampleShadingSupported=true;
    std::vector<std::shared_ptr<test::MultisampleSceneObservation>> observations;bool rendererSupport=true;
    RenderViewService service(base,[&](const std::string&){auto observation=std::make_shared<test::MultisampleSceneObservation>();observation->supported=rendererSupport;observations.push_back(observation);return std::make_unique<test::MultisampleSceneProbe>(observation);},[]{});
    RenderViewDescriptor descriptor;descriptor.extent={64,48};descriptor.settings.antiAliasing=4;
    const auto a=service.create(descriptor);require(observations.back()->loadedSamples==VK_SAMPLE_COUNT_4_BIT,"Renderer consumes actual four-sample context");
    require(service.describe(a).at("multisampleBytes")==64*48*64*2&&service.describe(a).at("temporalBytes")==0,"Pure multisampling has bounded sources and no history");
    require(service.describe(a).at("sceneExtent")==nlohmann::json({64,48}),"Multisampling preserves scene pixel extent");
    descriptor.settings.antiAliasing=1;const auto b=service.create(descriptor);require(observations.back()->loadedSamples==VK_SAMPLE_COUNT_1_BIT,"Single-sample auxiliary view cannot inherit main sample count");
    descriptor.settings.antiAliasing=4;rendererSupport=false;const auto c=service.create(descriptor);
    require(service.describe(c).at("rasterizationSamples")==1&&service.describe(c).at("multisampleFallback")=="Renderer does not provide multisample shading","Unsupported renderer exposes spatial fallback");
    SceneFrameData frame;std::string error;
    for(unsigned slot=0;slot<2;++slot){base.currentFrame=slot;service.request(a);service.request(b);service.request(c);RenderGraph graph;service.schedule(graph,frame,base,slot+1);
        require(graph.compile(error),error.c_str());commands.calls.clear();graph.execute(&commands);require(service.submitted(slot+1),"Multisample views commit actual recording");service.complete(slot+1);
        require(std::count_if(graph.passes().begin(),graph.passes().end(),[](const auto& pass){return pass.name.find("view-multisample-")==0;})==1,"Only capable requested view resolves multisample attachments");}
    require(observations[0]->framebuffers.size()==2&&observations[0]->framebuffers[0]!=observations[0]->framebuffers[1],"Consecutive in-flight slots use distinct source framebuffers");
    rendererSupport=true;service.resize(a,{80,60});service.request(a);RenderGraph resized;service.schedule(resized,frame,base,3);require(resized.compile(error),error.c_str());resized.execute(&commands);require(service.submitted(3),"Resized sample target records");service.complete(3);
    require(service.describe(a).at("multisampleBytes")==80*60*64*2,"Resize rebuilds sample budget from actual extent");
    service.release(a);service.release(b);service.release(c);service.complete(3);require(!backend.allocator().statistics().liveImages,"Retired view sources and outputs release together");
    backend.sampleShadingSupported=false;const auto noDevice=service.create(descriptor);require(service.describe(noDevice).at("rasterizationSamples")==1&&!service.describe(noDevice).at("multisampleFallback").get<std::string>().empty(),"Disabled device capability produces explicit fallback");service.release(noDevice);
    backend.sampleShadingSupported=true;descriptor.extent={4096,4096};const auto overBudget=service.create(descriptor);
    require(service.describe(overBudget).at("rasterizationSamples")==1&&service.describe(overBudget).at("multisampleBytes")==0,"Budget fallback does not retain partial multisample allocations");service.release(overBudget);
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"All view fallback paths retire resources");
    std::cout<<"Multisample views: actual samples, independent single-sample view, renderer/device/budget fallback, frame slots, resize and retirement passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
