#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <sstream>
#include <nlohmann/json.hpp>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(const std::string& shaders,bool bindless){
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
    context.renderExtent={256,192};context.bindlessTextures=bindless;context.computeSkinning=true;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const std::string root=AZURERENDER_TEST_SOURCE_DIR,path=root+"/assets_public/scene_inspector/assets/cube.gltf";
    context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
    context.scene.resources.push_back({"mesh",path});
    scene::SceneNodeDesc node;node.id="first";node.resourceId="mesh";context.scene.nodes.push_back(node);
    node.id="second";node.translation={2,0,0};context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;context.renderSettings=&settings;
    renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=256;frame.swapchainHeight=192;
    auto prepare=[&](unsigned slot){
        frame.currentFrame=slot;context.currentFrame=slot;renderer.updateFrame(frame);RenderGraph graph;
        SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
        renderer.registerPasses(graph,resources,context);std::string error;require(graph.compile(error),error.c_str());return graph;
    };
    const auto record=[&](const RenderGraph& graph){
        const auto pass=std::find_if(graph.passes().begin(),graph.passes().end(),[](const auto& p){return p.name=="character-skinning";});
        require(pass!=graph.passes().end(),"Compute producer pass must retain graph dependencies");
        rhi::NullCommandRecorder recorder;pass->recordCommands(recorder);
        return std::count_if(recorder.calls.begin(),recorder.calls.end(),[](const auto& call){return call.name=="dispatch";});
    };
    auto graph=prepare(0);
    const auto* preparedState=renderer.sceneState();
    require(preparedState&&preparedState->pickables.size()==2,"Both static instances must expose current picking poses");
    require(preparedState->pickables[0].pose&&preparedState->pickables[0].pose==preparedState->pickables[1].pose,
        "Instances of one static resource must share an immutable loaded pose");
    require(record(graph)>0,"First frame slot must produce static output");renderer.onFrameSubmitted(1);
    graph=prepare(1);require(record(graph)>0,"Second frame slot owns independent output");renderer.onFrameSubmitted(2);
    graph=prepare(0);require(record(graph)==0,"Retired first slot reuses submitted output");renderer.onFrameSubmitted(3);
    {std::ostringstream text;renderer.appendCaptureManifestFields(text);const auto state=nlohmann::json::parse("{"+text.str()+"\"end\":true}");
     require(state.at("staticSkinningReusedInstances")==2&&state.at("computeSkinningPreparedDispatches")==0&&state.at("staticSkinningReusedVertices").get<unsigned>()>0,
         "Capture metadata must report prepared work and actual content-reuse decisions");
     require(state.at("bindPoseCacheEntries")==1&&state.at("bindPoseSharedInstances")==2,
         "Static instances share immutable loaded bind poses");
     require(state.at("loadedResourceIdentityEntries")==1,"Loaded identity map must describe current resource ids");}
    settings.morphWeights={-0.F,0};graph=prepare(0);
    require(record(graph)>0,"Signed-zero legacy weights require recomputation to preserve exact vertex words");renderer.onFrameAbandoned();
    settings.morphWeights={.5F,0};graph=prepare(0);require(record(graph)>0,"Dynamic morph inputs bypass static reuse");renderer.onFrameAbandoned();
    settings.morphWeights={0,0};graph=prepare(0);renderer.onFrameSubmitted(4);
    graph=prepare(0);require(record(graph)>0,"Unrecorded or abandoned preparation cannot become valid");renderer.onFrameSubmitted(5);
    graph=prepare(0);require(record(graph)==0,"Recomputed static output becomes reusable");renderer.onFrameSubmitted(6);
    renderer.onSwapchainRecreate(context);graph=prepare(0);require(record(graph)==0,"Graphics pipeline recreation preserves unchanged compute output");renderer.onFrameSubmitted(7);
    auto grown=std::make_shared<scene::SceneDescription>(context.scene);
    for(unsigned i=0;i<32;++i){auto extra=grown->nodes.front();extra.id="spawn-"+std::to_string(i);grown->nodes.push_back(extra);}
    frame.sceneSnapshot=grown;graph=prepare(0);require(record(graph)>0,"Output-buffer growth invalidates prior content");renderer.onFrameSubmitted(8);
    // A graph recorded from one slot cannot commit the current other slot.
    auto old=prepare(0);prepare(1);record(old);renderer.onFrameSubmitted(9);
    graph=prepare(1);require(record(graph)>0,"Only the slot in the recorded producer ticket may commit");renderer.onFrameSubmitted(10);
    // Concurrent recording is valid; overlapping preparation versions are
    // conservatively invalidated without relying on callback completion order.
    settings.morphWeights={.25F,0};old=prepare(0);settings.morphWeights={0,0};graph=prepare(0);
    std::thread worker([&]{record(old);});record(graph);worker.join();renderer.onFrameSubmitted(11);
    graph=prepare(0);require(record(graph)>0,"Old and current graph versions must invalidate ambiguous output");renderer.onFrameSubmitted(12);
    graph=prepare(0);require(record(graph)==0,"Fresh production after a version conflict is reusable");renderer.onFrameSubmitted(13);
    renderer.onUnload(context);
    {std::ostringstream text;renderer.appendCaptureManifestFields(text);const auto state=nlohmann::json::parse("{"+text.str()+"\"end\":true}");
     require(state.at("bindPoseCacheEntries")==0&&state.at("bindPoseSharedInstances")==0&&state.at("loadedResourceIdentityEntries")==0,"Unloading releases bind-pose content and resource identities");}
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Static output lifecycle must release every resource");
    frame.sceneSnapshot.reset();auto alias=context.scene.resources.front();alias.id="alias-mesh";
    context.scene.resources.push_back(alias);context.scene.nodes.front().resourceId=alias.id;
    renderer.onLoad(context);graph=prepare(0);
    {std::ostringstream text;renderer.appendCaptureManifestFields(text);const auto state=nlohmann::json::parse("{"+text.str()+"\"end\":true}");
     require(state.at("bindPoseCacheEntries")==1&&state.at("bindPoseSharedInstances")==2&&state.at("loadedResourceIdentityEntries")==2,"Reloading resolves aliases into their shared mesh and retains both physical instances");}
    require(record(graph)>0,"Reloaded GPU output is cold independently of CPU bind-pose content");
    const auto* reloadedBindPose=renderer.sceneState()->pickables.at(1).pose;
    const auto immutableNodes=reloadedBindPose->nodeWorldMatrices;
    renderer.onFrameAbandoned();frame.animations.push_back({});frame.animations.back().node="first";graph=prepare(0);
    require(renderer.sceneState()->pickables.at(0).pose!=renderer.sceneState()->pickables.at(1).pose,
        "Presented instances must own their evaluated pose independently of static replicas");
    require(renderer.sceneState()->pickables.at(1).pose==reloadedBindPose&&reloadedBindPose->nodeWorldMatrices==immutableNodes,
        "Dynamic evaluation must preserve immutable static inputs");
    {std::ostringstream text;renderer.appendCaptureManifestFields(text);const auto state=nlohmann::json::parse("{"+text.str()+"\"end\":true}");
     require(state.at("bindPoseSharedInstances")==1,"Presented animation inputs must use their own evaluated pose");}
    renderer.onFrameAbandoned();frame.animations.clear();graph=prepare(0);
    require(renderer.sceneState()->pickables.at(0).pose==renderer.sceneState()->pickables.at(1).pose,
        "Returning to static input rejoins the loaded immutable pose");
    {std::ostringstream text;renderer.appendCaptureManifestFields(text);const auto state=nlohmann::json::parse("{"+text.str()+"\"end\":true}");
     require(state.at("bindPoseSharedInstances")==2,"Returning to static input restores shared immutable bind poses");}
    renderer.onFrameAbandoned();context.scene.resources[0].id="reused-mesh";context.scene.resources[1].id="reused-alias";
    context.scene.nodes[0].resourceId="reused-alias";context.scene.nodes[1].resourceId="reused-mesh";
    require(renderer.reuseScene(context),"Same mesh keys must support new scene resource identities without GPU reload");
    graph=prepare(0);
    {std::ostringstream text;renderer.appendCaptureManifestFields(text);const auto state=nlohmann::json::parse("{"+text.str()+"\"end\":true}");
     require(state.at("loadedResourceIdentityEntries")==2&&state.at("bindPoseSharedInstances")==2,"Reused scenes must resolve their current resource identities into both existing mesh instances");}
    require(renderer.sceneState()&&renderer.sceneState()->pickables.size()==2,"Scene reuse must preserve both current pickable instances");
    renderer.onFrameAbandoned();renderer.onUnload(context);
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Reloaded renderer releases every resource");
}
void verifyMorphOwnership(const std::string& shaders,bool bindless){
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
    context.renderExtent={128,96};context.bindlessTextures=bindless;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const std::string root=AZURERENDER_TEST_SOURCE_DIR,path=root+"/assets_public/test_model.gltf";
    context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
    auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));asset->animations.clear();
    require(!asset->morphRanges.empty(),"Morph ownership fixture needs actual targets");
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,asset}};context.preparedMeshes=&prepared;
    context.scene.resources.push_back({"morph",path});
    for(const auto* id:{"one","two"}){scene::SceneNodeDesc node;node.id=id;node.resourceId="morph";context.scene.nodes.push_back(node);}
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;context.renderSettings=&settings;
    renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;
    renderer.updateFrame(frame);const auto* bind=renderer.sceneState()->pickables.at(0).pose;
    require(bind&&bind==renderer.sceneState()->pickables.at(1).pose,"Default morph content shares an immutable pose");
    const auto defaultWeights=bind->morphWeights;
    settings.morphWeights={.5F,0};renderer.updateFrame(frame);
    const auto* first=renderer.sceneState()->pickables.at(0).pose;const auto* second=renderer.sceneState()->pickables.at(1).pose;
    require(first!=bind&&second!=bind&&first!=second,"Legacy morph edits own separate instance values");
    require(bind->morphWeights==defaultWeights,"Writable morph values must not modify the loaded pose");
    require(std::any_of(first->morphWeights.begin(),first->morphWeights.end(),[](const auto& weights){return !weights.empty()&&weights[0]==.5F;}),
        "Legacy morph weights reach the evaluated instance pose");
    settings.morphWeights={0,0};renderer.updateFrame(frame);
    require(renderer.sceneState()->pickables.at(0).pose==bind&&renderer.sceneState()->pickables.at(1).pose==bind,
        "Reset morph inputs restore the same immutable loaded content");
    require(bind->morphWeights==defaultWeights,"Reset content preserves original morph bytes");
    renderer.onUnload(context);
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Morph ownership unload releases all GPU resources");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");for(bool indexed:{false,true}){verify(argv[1],indexed);verifyMorphOwnership(argv[1],indexed);}
    std::cout<<"Static compute output recording: slots, commit/cancel, unrecorded frames, resize, old graphs, concurrent recording and release passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
