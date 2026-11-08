#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(const char* shaders,bool indexed,AssetAlphaMode mode,unsigned features,float opacity,bool multisample){
    rhi::NullRhi backend;backend.supportedSamples=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;backend.sampleShadingSupported=true;
    CharacterSceneRenderer renderer;RenderContext context;context.rhi=&backend;context.allocator=&backend.allocator();
    context.shaderDirectory=shaders;context.renderExtent={128,96};context.bindlessTextures=indexed;context.cullingEnabled=false;
    context.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;context.depthFormat=VK_FORMAT_D32_SFLOAT;
    if(multisample)context.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const std::string root=AZURERENDER_TEST_SOURCE_DIR,path=root+"/assets_public/scene_inspector/assets/cube.gltf";
    context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
    auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    for(auto& node:asset->nodes)node.opacity=opacity;
    for(auto& material:asset->materials){material.alphaMode=mode;material.materialFeatures=features;}
    std::map<std::string,std::shared_ptr<const LoadedAsset>> meshes{{path,asset}};context.preparedMeshes=&meshes;
    context.scene.resources.push_back({"mesh",path});scene::SceneNodeDesc node;node.id="object";node.resourceId="mesh";context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;
    settings.screenSpaceReflection.enabled=true;settings.sceneLightingEnabled=true;context.renderSettings=&settings;
    renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;
    std::set<std::string> allCovered,allEarlyModules;
    for(unsigned generation=0;generation<2;++generation){
        if(generation)renderer.onSwapchainRecreate(context);
        renderer.updateFrame(frame);RenderGraph graph;
        SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
        renderer.registerPasses(graph,resources,context);std::string error;require(graph.compile(error),error.c_str());
        std::set<std::string> prefill,covered,ordinaryCovered,classCovered,earlyModules;
        for(const auto& call:backend.calls)if(call.name=="createShaderModule"&&call.detail.find(" earlyFragmentTests=1")!=std::string::npos)
            earlyModules.insert(call.detail.substr(0,call.detail.find(' ')));
        allEarlyModules.insert(earlyModules.begin(),earlyModules.end());
        for(const auto& call:backend.calls)if(call.name=="createGraphicsPipeline"){
            const auto at=call.detail.find(" fragmentModule=");if(at==std::string::npos)continue;
            const auto first=at+16;const auto module=call.detail.substr(first,call.detail.find(' ',first)-first);
            if(!earlyModules.count(module))continue;
            const bool ordinaryProgram=call.detail.find("fragmentSpec=0:1/")!=std::string::npos;
            require(ordinaryProgram||call.detail.find("fragmentSpec=1:0/2:")!=std::string::npos,"Early-depth programs require ordinary or fixed material-class specialization");
            const auto handle=call.detail.substr(0,call.detail.find(' '));
            (ordinaryProgram?ordinaryCovered:classCovered).insert(handle);allCovered.insert(handle);
        }
        const bool ordinary=mode==AssetAlphaMode::Opaque&&features==0;
        require(ordinaryCovered.size()==(!multisample&&ordinary?8*(generation+1):0),"Ordinary covered color pipelines require explicit single-sample qualification");
        const bool fixedClass=mode==AssetAlphaMode::Opaque&&features==MaterialFeatureSqrtAlphaShadow;
        require(classCovered.size()==(!multisample&&fixedClass?8*(generation+1):0),"Fixed-class covered pipelines require opaque single-sample qualification");
        for(const auto& call:backend.calls)if(call.name=="createGraphicsPipeline"&&call.detail.find("depthBias=1.000000/0.000000")!=std::string::npos&&call.detail.find(" masks=0/0/")!=std::string::npos){
            const auto handle=call.detail.substr(0,call.detail.find(' '));prefill.insert(handle);
            if(call.detail.find("fragment=0")!=std::string::npos){covered.insert(handle);allCovered.insert(handle);}
        }
        require(covered.size()==(multisample?0:6*(generation+1)),"Each generation must own six single-sample covered pipelines");
        unsigned checked=0;
        for(const auto& pass:graph.passes())if(pass.name=="character-opaque-depth"||pass.name=="character-main"){
            rhi::NullCommandRecorder commands;pass.recordCommands(commands);std::string current;unsigned draws=0,coveredColors=0,coveredClasses=0;
            for(const auto& call:commands.calls){
                if(call.name=="bindPipeline")current=call.detail;
                if(call.name=="drawIndexed"&&prefill.count(current)){
                    ++draws;require((covered.count(current)!=0)==(!multisample&&opacity==1.F),"Covered pipeline must preserve fade and MSAA fallback");
                }
                if(call.name=="drawIndexed"&&ordinaryCovered.count(current))++coveredColors;
                if(call.name=="drawIndexed"&&classCovered.count(current))++coveredClasses;
            }
            require((draws>0)==(mode==AssetAlphaMode::Opaque&&opacity>0),"Both workflows must preserve opaque-only prefill submission");++checked;
            require((coveredColors>0)==(!multisample&&ordinary&&opacity==1.F),"Both color workflows must preserve ordinary/fade/MSAA specialization eligibility");
            require((coveredClasses>0)==(!multisample&&fixedClass&&opacity==1.F),"Fixed-class submissions require qualified material, opacity and sample count");
        }
        require(checked==2,"Snapshot and main workflows must both execute");
    }
    renderer.onUnload(context);std::map<std::string,unsigned> destroyed;
    for(const auto& call:backend.calls)if(call.name=="destroyPipeline")++destroyed[call.detail];
    for(const auto& handle:allCovered)require(destroyed[handle]==1,"Covered pipeline must release exactly once across recreation and unload");
    std::map<std::string,unsigned> destroyedModules;
    for(const auto& call:backend.calls)if(call.name=="destroyShaderModule")++destroyedModules[call.detail];
    for(const auto& handle:allEarlyModules)require(destroyedModules[handle]==1,"Early-depth program ownership must release exactly once");
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"All scene resources must release");
}
void verifyConservativeOcclusion(const char* shaders,bool supported){
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;context.rhi=&backend;context.allocator=&backend.allocator();
    context.shaderDirectory=shaders;context.renderExtent={128,96};context.cullingEnabled=false;
    context.computeShaders=supported;context.gpuCulling=supported;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const std::string root=AZURERENDER_TEST_SOURCE_DIR,path=root+"/assets_public/scene_inspector/assets/cube.gltf",fade=root+"/assets_public/test_model.gltf";
    context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
    auto opaque=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    for(auto& material:opaque->materials){material.alphaMode=AssetAlphaMode::Opaque;material.materialFeatures=0;}
    auto faded=std::make_shared<LoadedAsset>(*opaque);for(auto& node:faded->nodes)node.opacity=.5F;
    std::map<std::string,std::shared_ptr<const LoadedAsset>> meshes{{path,opaque},{fade,faded}};context.preparedMeshes=&meshes;
    context.scene.resources.push_back({"opaque",path});context.scene.resources.push_back({"fade",fade});
    scene::SceneNodeDesc node;node.resourceId="opaque";
    for(unsigned i=0;i<300;++i){node.id="opaque-"+std::to_string(i);context.scene.nodes.push_back(node);}
    node.id="fade";node.resourceId="fade";context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;settings.sceneLightingEnabled=true;context.renderSettings=&settings;
    rhi::RenderPassDesc source;source.depthAttachment=1;
    source.attachments={{VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
        {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true},
        {VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false}};
    source.externalReadDependency=true;context.scenePassDescription=std::make_shared<const rhi::RenderPassDesc>(source);
    context.sceneFramebuffer=reinterpret_cast<VkFramebuffer>(std::uintptr_t(1));
    context.sceneDepthImage=reinterpret_cast<VkImage>(std::uintptr_t(2));context.sceneDepthView=reinterpret_cast<VkImageView>(std::uintptr_t(3));
    context.sceneColorImages={reinterpret_cast<VkImage>(std::uintptr_t(4)),reinterpret_cast<VkImage>(std::uintptr_t(5)),VK_NULL_HANDLE};
    renderer.onLoad(context);SceneFrameData frame;frame.cameraPosition[2]=4;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;
    for(unsigned generation=0;generation<2;++generation){
        if(generation)renderer.onSwapchainRecreate(context);renderer.updateFrame(frame);RenderGraph graph;
        SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
        renderer.registerPasses(graph,resources,context);std::string error;require(graph.compile(error),error.c_str());
        if(!supported){
            for(const auto& pass:graph.passes())require(pass.name!="character-main-depth"&&pass.name!="character-occlusion","Absent compute or indirect capability must preserve original recording");
            continue;
        }
        unsigned depthDraws=0,indirect=0,producers=0,duplicatePrefill=0;
        std::set<std::string> prefill;
        for(const auto& call:backend.calls)if(call.name=="createGraphicsPipeline"&&call.detail.find("depthBias=1.000000/0.000000")!=std::string::npos&&call.detail.find(" masks=0/0/")!=std::string::npos)
            prefill.insert(call.detail.substr(0,call.detail.find(' ')));
        for(const auto& pass:graph.passes()){
            if(pass.name!="character-main-depth"&&pass.name!="character-depth-hierarchy"&&pass.name!="character-occlusion"&&pass.name!="character-main-depth-resume"&&pass.name!="character-main")continue;
            rhi::NullCommandRecorder recorder;pass.recordCommands(recorder);
            if(pass.name=="character-main-depth")for(const auto& call:recorder.calls)if(call.name=="drawIndexed")depthDraws+=static_cast<unsigned>(std::stoul(call.detail.substr(call.detail.find(" x")+2)));
            if(pass.name=="character-main"){std::string pipeline;for(const auto& call:recorder.calls){
                if(call.name=="bindPipeline")pipeline=call.detail;
                if(call.name=="drawIndexedIndirect")++indirect;
                if(call.name=="drawIndexed"&&prefill.count(pipeline))++duplicatePrefill;
            }}
            if(pass.name!="character-main")++producers;
        }
        require(producers==4,"Main depth, hierarchy, command and depth resume producers must be present");
        require(depthDraws==301*opaque->primitives.size(),"Main depth must preserve opaque and faded prefill exactly once");
        require(duplicatePrefill==0,"Color phase must consume prepared depth without another prefill");
        require(indirect>0,"Qualified main draws must consume conservative indirect output");
        auto incomplete=context;incomplete.sceneDepthImage=VK_NULL_HANDLE;RenderGraph fallback;
        SceneGraphResources fallbackResources{fallback.addResource("color"),fallback.addResource("depth"),fallback.addResource("normal"),fallback.addResource("shadow")};
        renderer.registerPasses(fallback,fallbackResources,incomplete);require(fallback.compile(error),error.c_str());
        for(const auto& pass:fallback.passes()){
            require(pass.name!="character-main-depth"&&pass.name!="character-occlusion","Absent borrowed target must retain the original path");
            if(pass.name=="character-main"){rhi::NullCommandRecorder recorder;pass.recordCommands(recorder);
                std::string pipeline;unsigned restoredPrefill=0;for(const auto& call:recorder.calls){
                    if(call.name=="bindPipeline")pipeline=call.detail;
                    if(call.name=="drawIndexed"&&prefill.count(pipeline))++restoredPrefill;
                }require(restoredPrefill>0,"Fallback must restore the original main prefill");}
        }
    }
    renderer.onUnload(context);require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Occlusion scene recreation must retire every resource");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");
    for(bool indexed:{false,true}){
        verify(argv[1],indexed,AssetAlphaMode::Opaque,0,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Opaque,0,.5F,false);
        verify(argv[1],indexed,AssetAlphaMode::Opaque,0,0,false);
        verify(argv[1],indexed,AssetAlphaMode::Mask,0,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Blend,0,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Opaque,MaterialFeatureSqrtAlphaShadow,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Mask,MaterialFeatureGeometryShadow,1,false);
        verify(argv[1],indexed,AssetAlphaMode::Opaque,0,1,true);
    }
    verifyConservativeOcclusion(argv[1],true);
    verifyConservativeOcclusion(argv[1],false);
    std::cout<<"Depth prefill: two workflows, descriptors, opacity, alpha modes, flags, MSAA fallback and owned recreation passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
