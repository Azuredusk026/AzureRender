#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(const std::string& shaders,bool bindless,AssetAlphaMode mode,unsigned features,float opacity,bool expected){
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
    context.renderExtent={256,192};context.bindlessTextures=bindless;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const std::string root=AZURERENDER_TEST_SOURCE_DIR,path=root+"/assets_public/scene_inspector/assets/cube.gltf";
    context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
    auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    for(auto& node:asset->nodes)node.opacity=opacity;
    for(auto& material:asset->materials){material.alphaMode=mode;material.materialFeatures=features;}
    std::map<std::string,std::shared_ptr<const LoadedAsset>> meshes{{path,asset}};context.preparedMeshes=&meshes;
    context.scene.resources.push_back({"mesh",path});scene::SceneNodeDesc node;node.id="object";node.resourceId="mesh";
    node.objectShadow=ecs::ObjectShadowComponent{};node.objectShadow->enabled=true;context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;context.renderSettings=&settings;
    renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=256;frame.swapchainHeight=192;
    renderer.updateFrame(frame);RenderGraph graph;
    SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
    renderer.registerPasses(graph,resources,context);std::string error;require(graph.compile(error),error.c_str());
    std::set<std::string> depthOnly;
    for(const auto& call:backend.calls)if(call.name=="createGraphicsPipeline"&&call.detail.find("fragment=0")!=std::string::npos&&call.detail.find(" masks=")==std::string::npos)
        depthOnly.insert(call.detail.substr(0,call.detail.find(' ')));
    require(depthOnly.size()==2,"Renderer must own separate cascade and object depth-only pipelines");
    unsigned checked=0;
    for(const auto& pass:graph.passes())if(pass.name=="character-shadow"||pass.name=="object-shadow-atlas"){
        rhi::NullCommandRecorder recorder;pass.recordCommands(recorder);std::string current;unsigned draws=0;
        for(const auto& call:recorder.calls){
            if(call.name=="bindPipeline")current=call.detail;
            if(call.name=="drawIndexed"){
                ++draws;require((depthOnly.count(current)!=0)==expected,"Shadow draw must respect alpha/coverage/opacity fallback");
            }
        }
        require(draws>0,"Both shadow workflows must actually draw");++checked;
    }
    require(checked==2,"Cascade and object workflows must both be exercised");
    renderer.onUnload(context);
    std::set<std::string> destroyed;
    for(const auto& call:backend.calls)if(call.name=="destroyPipeline")destroyed.insert(call.detail);
    for(const auto& handle:depthOnly)require(destroyed.count(handle)!=0,"Depth-only pipeline ownership must release");
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Renderer resources must release");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");
    for(bool indexed:{false,true}){
        verify(argv[1],indexed,AssetAlphaMode::Opaque,0,1,true);
        verify(argv[1],indexed,AssetAlphaMode::Opaque,0,.5F,false);
        verify(argv[1],indexed,AssetAlphaMode::Mask,0,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Blend,0,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Opaque,MaterialFeatureSqrtAlphaShadow,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Blend,MaterialFeatureGeometryShadow|MaterialFeatureSqrtAlphaShadow,1,true);
    }
    std::cout<<"Cascade/object shadow recording: opaque, fade, mask, blend, coverage priority, two descriptor modes and owned release passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
