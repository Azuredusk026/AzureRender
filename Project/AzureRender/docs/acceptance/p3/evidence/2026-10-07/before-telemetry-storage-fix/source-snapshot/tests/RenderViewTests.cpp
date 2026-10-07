#include "render/RenderViewService.hpp"
#include "rhi/NullRhi.hpp"
#include "scenes/SampleSceneRenderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid view use must be rejected");}
int main(int argc,char** argv){try{
    require(argc==2,"Expected compiled shader directory");
    rhi::NullRhi rhi;rhi::NullCommandRecorder commands;RenderContext base;
    base.rhi=&rhi;base.allocator=&rhi.allocator();base.device=reinterpret_cast<VkDevice>(1);
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.commands=&commands;
    base.shaderDirectory=argv[1];
    RenderViewService service(base,[](const std::string&){return std::make_unique<SampleSceneRenderer>();},[]{});
    RenderViewDescriptor descriptor;descriptor.rendererId="sample";descriptor.extent={256,128};
    descriptor.cameraPosition={2,1,3};const auto a=service.create(descriptor);
    descriptor.extent={128,256};descriptor.cameraPosition={-2,2,1};const auto b=service.create(descriptor);
    const auto c=service.create(descriptor);reject([&]{service.create(descriptor);});
    require(service.describe(a).at("cameraPosition")==nlohmann::json({2,1,3}),"Each view preserves its camera");
    require(service.describe(b).at("extent")==nlohmann::json({128,256}),"Each view preserves its extent");
    service.request(a);service.request(a);service.request(b);
    RenderGraph graph;SceneFrameData frame;service.schedule(graph,frame,base,1);
    std::string error;require(graph.compile(error),"Independent views must form a valid graph");graph.execute(&commands);
    require(service.describe(a).at("renderCount")==1,"Repeated requests coalesce within one frame");
    require(service.describe(c).at("renderCount")==0,"A dormant view remains unscheduled");
    require(service.describe(a).at("colorImage")!=service.describe(b).at("colorImage"),"Independent views own different attachments");
    reject([&]{service.readPixels(a);});
    service.complete(1);
    require(service.readPixels(a).size()==131072,"A 256 by 128 view exposes tightly packed RGBA pixels after completion");
    require(service.describe(a).at("outputFormat")=="rgba8-linear","UI samples a resolved linear LDR output");
    require(service.describe(a).at("resolvedImage")!=service.describe(a).at("colorImage"),"HDR rendering and UI output have independent images");
    reject([&]{service.sample(c,2);});
    RenderGraph idle;service.schedule(idle,frame,base,2);require(idle.passes().empty(),"A completed static result causes no new render work");
    service.sample(a,2);const auto live=rhi.allocator().statistics().liveImages;
    service.release(a);reject([&]{service.request(a);});service.complete(1);
    require(rhi.allocator().statistics().liveImages==live,"A sampled cached view survives until its use completes");
    service.complete(2);require(rhi.allocator().statistics().liveImages<live,"Completed sampling permits retirement");
    const auto replacement=service.create(descriptor);require(replacement!=a,"Reused slots carry a new generation");
    reject([&]{service.resize(b,{0,128});});service.resize(b,{320,180});service.request(b);
    RenderGraph resized;service.schedule(resized,frame,base,3);require(resized.compile(error),"Resized view graph is valid");resized.execute(&commands);
    service.complete(3);require(service.describe(b).at("extent")==nlohmann::json({320,180}),"Only the requested view is resized");
    service.sample(b,4);service.resize(b,{400,200});
    require(service.sample(b,5)==VK_NULL_HANDLE,"Pending resize suspends sampling so old GPU uses can retire");
    RenderGraph pending;service.schedule(pending,frame,base,4);require(pending.passes().empty(),"In-flight resize preserves the current resources");
    service.complete(4);RenderGraph finished;service.schedule(finished,frame,base,5);
    require(finished.compile(error),"Completed resize builds a valid graph");finished.execute(&commands);service.complete(5);
    require(service.describe(b).at("extent")==nlohmann::json({400,200}),"Suspended sampling allows a requested resize to finish");
    service.release(b);service.release(c);service.release(replacement);service.complete(5);
    require(rhi.allocator().statistics().liveImages==0,"Released views leave no image allocations");
    RenderViewService another(base,[](const std::string&){return std::make_unique<SampleSceneRenderer>();},[]{});
    const auto fresh=another.create(descriptor);
    require(fresh!=a && fresh!=replacement,"Service replacement must preserve global handle invalidation");
    reject([&]{another.describe(a);});another.release(fresh);
    const auto beforeFailure=rhi.allocator().statistics().liveImages;
    RenderViewService broken(base,[](const std::string&)->std::unique_ptr<ISceneRenderer>{throw std::runtime_error("Renderer load failure");},[]{});
    reject([&]{broken.create(descriptor);});
    require(rhi.allocator().statistics().liveImages==beforeFailure,"Partial renderer creation releases every attachment");
    std::cout<<"View isolation, demand scheduling, generation and sampled-resource retirement passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
