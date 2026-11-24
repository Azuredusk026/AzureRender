#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(const std::filesystem::path& shaders,bool indexed,bool compute,bool exact){
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    backend.exactDepthBiasSupported=exact;
    context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders.string();
    context.renderExtent={256,192};context.bindlessTextures=indexed;context.computeSkinning=compute;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;
    limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const auto root=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
    context.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    context.scene.resources.push_back({"mesh",(root/"assets_public/scene_inspector/assets/cube.gltf").string()});
    for(unsigned i=0;i<2;++i){
        scene::SceneNodeDesc node;node.id="object-"+std::to_string(i);node.resourceId="mesh";
        node.translation={float(i)*3,0,0};node.scale=i?std::array<float,3>{-2,1,1}:std::array<float,3>{1,1,1};
        node.objectShadow=ecs::ObjectShadowComponent{};node.objectShadow->enabled=false;node.objectShadow->atlasPage=6+i;
        context.scene.nodes.push_back(node);
    }
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;settings.silhouetteOutlineEnabled=false;
    context.renderSettings=&settings;
    renderer.onLoad(context);
    require(std::any_of(backend.calls.begin(),backend.calls.end(),[exact](const auto& call){
        return call.name=="createGraphicsPipeline"&&call.detail.find("depthBias=8.000000/0.000000")!=std::string::npos
            &&(call.detail.find("exactDepthBias=1")!=std::string::npos)==exact;
    }),"Object shadows must select exact depth units only on capable backends");
    SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=256;frame.swapchainHeight=192;
    renderer.updateFrame(frame);const auto before=backend.allocator().statistics().liveImages;
    auto scene=std::make_shared<scene::SceneDescription>(context.scene);
    for(auto& node:scene->nodes)node.objectShadow->enabled=true;
    frame.sceneSnapshot=scene;renderer.updateFrame(frame);
    require(backend.allocator().statistics().liveImages==before+1,"Object atlas must allocate once on first use");
    const auto countAtlasWrites=[&]{return std::count_if(backend.descriptorImageWrites.begin(),backend.descriptorImageWrites.end(),
        [](const auto& write){return write.binding==29;});};
    const auto written=countAtlasWrites();renderer.updateFrame(frame);
    require(countAtlasWrites()==written,"Stable object atlas bindings must avoid per-material descriptor writes each frame");
    RenderGraph graph;SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
    renderer.registerPasses(graph,resources,context);std::string error;require(graph.compile(error),error.c_str());
    const auto objectPass=std::find_if(graph.passes().begin(),graph.passes().end(),[](const auto& pass){return pass.name=="object-shadow-atlas";});
    require(objectPass!=graph.passes().end(),"Enabled object components must register their bounded depth producer");
    rhi::NullCommandRecorder recorder;graph.execute(&recorder);
    for(const auto* rect:{"1022x1022@2049,1025","1022x1022@3073,1025"})
        require(std::any_of(recorder.calls.begin(),recorder.calls.end(),[&](const auto& call){return call.name=="setScissor"&&call.detail==rect;}),"Object pages must retain an independent one-texel clear border");
    std::ostringstream enabled;renderer.appendCaptureManifestFields(enabled);
    require(enabled.str().find("\"objectShadowPages\": 2")!=std::string::npos,"Capture state must report both objects");
    auto disabled=std::make_shared<scene::SceneDescription>(*scene);
    for(auto& node:disabled->nodes)node.objectShadow->enabled=false;
    frame.sceneSnapshot=disabled;frame.currentFrame=1;context.currentFrame=1;renderer.updateFrame(frame);
    std::ostringstream off;renderer.appendCaptureManifestFields(off);
    require(off.str().find("\"objectShadowPages\": 0")!=std::string::npos,"Disabling all components must clear the current record count");
    require(backend.allocator().statistics().liveImages==before+1,"Disabled objects retain a reusable atlas without additional allocation");
    rhi::NullCommandRecorder frozen;graph.execute(&frozen);
    require(std::count_if(frozen.calls.begin(),frozen.calls.end(),[](const auto& call){return call.name=="setScissor"&&call.detail=="1022x1022@2049,1025";})==1,
        "Prepared recording must retain its immutable object pages after a different frame update");
    rhi::NullCommandRecorder direct;context.commands=&direct;renderer.recordScene(context);
    require(std::none_of(direct.calls.begin(),direct.calls.end(),[](const auto& call){return call.name=="setScissor"&&call.detail.find("1022x1022")!=std::string::npos;}),
        "Disabled objects must stop their direct-recording draw work");
    renderer.onUnload(context);
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Object shadows must release all owned resources");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");
    for(bool exact:{false,true})for(bool indexed:{false,true})for(bool compute:{false,true})verify(argv[1],indexed,compute,exact);
    std::cout<<"Object shadow recording: lazy allocation, two mirrored/scaled instances, immutable graph, disable and release passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
