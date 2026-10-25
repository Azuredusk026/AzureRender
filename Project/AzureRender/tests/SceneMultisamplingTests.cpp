#include <algorithm>
#include "render/SceneMultisampling.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F> void reject(F operation){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}check(failed,"Invalid target must reject");}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shaders");
    rhi::NullRhi backend;backend.supportedSamples=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;
    SceneMultisampleDestination destination{{reinterpret_cast<VkImage>(100),reinterpret_cast<VkImage>(102),reinterpret_cast<VkImage>(104)},
        {reinterpret_cast<VkImageView>(101),reinterpret_cast<VkImageView>(103),reinterpret_cast<VkImageView>(105)}};
    const auto create=[&](VkExtent2D extent,unsigned slots=2){return std::make_unique<SceneMultisampling>(backend,backend.allocator(),extent,VK_FORMAT_D32_SFLOAT,VK_FORMAT_R8G8B8A8_UNORM,SceneSecondaryResolve::Average,slots,std::vector{destination},argv[1]);};
    reject([&]{create({0,48});});reject([&]{create({100000,100000});});reject([&]{create({64,48},0);});reject([&]{create({64,48},3);});
    check(backend.calls.empty(),"Invalid dimensions and budget reject before RHI mutation");
    backend.supportedSamples=VK_SAMPLE_COUNT_1_BIT;reject([&]{create({64,48});});
    check(backend.calls.empty(),"Unsupported formats reject before allocation");backend.supportedSamples|=VK_SAMPLE_COUNT_4_BIT;
    const auto invalidDestination=destination;destination.images[2]=destination.images[0];reject([&]{create({64,48});});destination=invalidDestination;
    destination.views[1]=VK_NULL_HANDLE;reject([&]{create({64,48});});destination=invalidDestination;
    reject([&]{SceneMultisampling target(backend,backend.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,VK_FORMAT_R8G8B8A8_UNORM,SceneSecondaryResolve::Minimum,2,{destination},argv[1]);});
    check(backend.calls.empty(),"Attachment aliases, missing views and incompatible resolve policy reject before allocation");
    {
        auto target=create({64,48});check(target->byteSize()==64*48*64*2,"Budget counts HDR, depth and normal for both four-sample slots");
        check(target->allocatedByteSize()==backend.allocator().statistics().liveImageBytes,"Actual allocator statistics remain observable");
        check(backend.allocator().statistics().liveImages==6,"Each source slot owns three images");
        check(target->sourceImage(0,0)!=target->sourceImage(1,0)&&target->framebuffer(0)!=target->framebuffer(1),"In-flight sources are distinct");
        reject([&]{target->framebuffer(2);});reject([&]{target->sourceImage(0,3);});
        RenderGraph graph;auto sources=target->importSources(graph,1,"source");std::array<RenderGraph::ResourceId,3> outputs{};
        for(unsigned i=0;i<3;++i){rhi::ImageBarrierDesc b;b.image=destination.images[i];if(i==1)b.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;outputs[i]=graph.importImage("output",b);}
        reject([&]{target->schedule(graph,1,0,sources,outputs,"missing");});check(graph.passes().empty()&&graph.resources().size()==6,"Missing producer rejects atomically");
        auto producer=graph.addPass("scene");for(unsigned i=0;i<3;++i)graph.attachment(producer,sources[i],i==1?RenderGraphUsage::DepthAttachment:RenderGraphUsage::ColorAttachment,i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        auto wrong=outputs;wrong[2]=wrong[0];reject([&]{target->schedule(graph,1,0,sources,wrong,"alias");});check(graph.passes().size()==1,"Aliases reject atomically");
        target->schedule(graph,1,0,sources,outputs,"resolve");std::string error;check(graph.compile(error),error.c_str());
        rhi::NullCommandRecorder direct;
        reject([&]{target->recordResolve(direct,2,0);});reject([&]{target->recordResolve(direct,0,1);});
        check(direct.calls.empty(),"Invalid direct resolve slots reject before recording");
        target->recordResolve(direct,1,0);
        check(direct.calls.front().name=="beginRenderPass"&&direct.calls.back().name=="endRenderPass","Direct resolve owns its pass scope");
        check(std::count_if(direct.calls.begin(),direct.calls.end(),[](const auto& c){return c.name=="draw";})==1,"Direct resolve draws once");
        rhi::NullCommandRecorder commands;graph.execute(&commands);check(graph.passes().size()==2&&graph.executionOrder().back()==1,"Resolve follows all source writers");
    }
    check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Target destruction releases allocations");
    reject([&]{SceneMultisampling broken(backend,backend.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,VK_FORMAT_R8G8B8A8_UNORM,SceneSecondaryResolve::Average,2,{destination},"missing-shaders");});
    check(backend.allocator().statistics().liveImages==0,"Shader load failure rolls back allocations");
    std::cout<<"Scene multisampling ownership, bounded budget, source slots, graph validation and failure rollback passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
