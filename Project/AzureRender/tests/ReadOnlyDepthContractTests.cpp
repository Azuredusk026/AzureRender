#include "rhi/NullRhi.hpp"
#include "render/RenderGraph.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void checkReadDepth(bool good,const char* why){if(!good)throw std::runtime_error(why);}
template<class F>void rejectReadDepth(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}checkReadDepth(rejected,"Invalid depth pass must fail before backend creation");}
int main()try{
    rhi::NullRhi backend;rhi::RenderPassDesc pass;pass.depthReadOnly=true;pass.depthAttachment=1;
    pass.attachments={{VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
        {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,false,true,true}};
    pass.attachments[1].load=true;const auto handle=backend.createRenderPass(pass);checkReadDepth(handle!=VK_NULL_HANDLE,"Read-only depth pass is supported");
    auto invalid=pass;invalid.attachments[1].clear=true;rejectReadDepth([&]{backend.createRenderPass(invalid);});
    invalid=pass;invalid.attachments[1].load=false;rejectReadDepth([&]{backend.createRenderPass(invalid);});
    invalid=pass;invalid.attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;rejectReadDepth([&]{backend.createRenderPass(invalid);});
    invalid=pass;invalid.depthAttachment=-1;rejectReadDepth([&]{backend.createRenderPass(invalid);});
    invalid=pass;invalid.depthAttachment=99;rejectReadDepth([&]{backend.createRenderPass(invalid);});
    invalid=pass;invalid.attachments[1].isDepth=false;rejectReadDepth([&]{backend.createRenderPass(invalid);});
    unsigned creates=0;for(const auto& call:backend.calls)creates+=call.name=="createRenderPass";
    checkReadDepth(creates==1,"Invalid descriptors cannot allocate backend objects");
    RenderGraph graph;rhi::ImageBarrierDesc initial;initial.image=reinterpret_cast<VkImage>(1);initial.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
    const auto depth=graph.importImage("depth",initial),opaque=graph.addPass("opaque"),motion=graph.addPass("motion");
    graph.attachment(opaque,depth,RenderGraphUsage::DepthAttachment,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    graph.attachment(motion,depth,RenderGraphUsage::DepthReadAttachment,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    checkReadDepth(graph.passes()[motion].writes.empty()&&graph.passes()[motion].reads.size()==1,"Read-only depth is a graph read");
    rejectReadDepth([&]{graph.use(motion,depth,RenderGraphUsage::DepthReadAttachment,true);});
    std::string error;checkReadDepth(graph.compile(error),"Read-only depth graph compiles");
    bool found=false;for(const auto& b:graph.barriers())if(b.pass==motion){
        checkReadDepth(b.image.newLayout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,"Read-only depth uses its declared layout");
        checkReadDepth((b.image.srcAccessMask&VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)!=0,"Opaque depth writes become visible");
        checkReadDepth(b.image.dstAccessMask==(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
            "Read-only subpass LOAD and preserving STORE both participate in synchronization");found=true;
    }
    checkReadDepth(found,"Depth writer-to-reader transition must be explicit");backend.destroyRenderPass(handle);
    std::cout<<"Read-only depth descriptor, graph dependency and failure checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
