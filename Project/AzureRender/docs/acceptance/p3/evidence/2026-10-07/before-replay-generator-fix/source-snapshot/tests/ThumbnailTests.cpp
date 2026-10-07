#include "editor/preview/AssetThumbnailService.hpp"
#include "rhi/NullRhi.hpp"
#include "scenes/SampleSceneRenderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    require(argc==2,"Expected shader directory");rhi::NullRhi rhi;RenderContext base;
    base.rhi=&rhi;base.allocator=&rhi.allocator();base.device=reinterpret_cast<VkDevice>(1);
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.shaderDirectory=argv[1];
    RenderViewService views(base,[](const auto&){return std::make_unique<SampleSceneRenderer>();},[]{});
    AssetThumbnailService thumbnails(views);RenderViewDescriptor descriptor;descriptor.rendererId="sample";
    const auto a=thumbnails.request({"mesh","fingerprint-a","settings-a"},descriptor);
    require(a!=0,"A content request creates a render view");
    require(thumbnails.request({"mesh","fingerprint-a","settings-a"},descriptor)==a,"Same fingerprint reuses its cache");
    RenderGraph graph;rhi::NullCommandRecorder recorder;base.commands=&recorder;
    views.schedule(graph,{},base,1);std::string error;require(graph.compile(error),"Thumbnail graph compiles");graph.execute(&recorder);views.complete(1);
    thumbnails.request({"mesh","fingerprint-a","settings-a"},descriptor);
    RenderGraph dormant;views.schedule(dormant,{},base,2);require(dormant.passes().empty(),"Cached thumbnails stop rendering");
    const auto b=thumbnails.request({"mesh","fingerprint-b","settings-a"},descriptor);
    require(a!=b,"Source changes replace the cache generation");
    thumbnails.clear();views.complete(2);
    require(rhi.allocator().statistics().liveImages==0,"Cache clearing releases completed view images");
    std::cout<<"Thumbnail fingerprint reuse, static suspension and invalidation passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
