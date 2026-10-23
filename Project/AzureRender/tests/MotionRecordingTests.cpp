#include "scenes/CharacterSceneRenderer.hpp"
#include "render/MotionVectorTarget.hpp"
#include "render/TemporalViewState.hpp"
#include "render/RenderViewService.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
using namespace azurerender;
void checkMotionRecording(bool value,const char* why){if(!value)throw std::runtime_error(why);}
nlohmann::json motionManifest(const CharacterSceneRenderer& renderer){std::ostringstream text;renderer.appendCaptureManifestFields(text);return nlohmann::json::parse("{"+text.str()+"\"end\":true}");}
void verifyMotionRecording(const char* shaders,bool indexed,bool compute,bool outlined){
    rhi::NullRhi backend;RenderSettings settings;settings.antiAliasing=3;
    settings.faceSdfFeature=settings.showcasePlatformFeature=false;settings.characterPresentation.platformEnabled=false;
    settings.innerOutlineEnabled=false;settings.silhouetteOutlineEnabled=false;settings.sceneLightingEnabled=true;
    RenderContext context;context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
    context.renderExtent=context.swapchainExtent={64,48};context.depthFormat=context.shadowFormat=VK_FORMAT_D32_SFLOAT;
    context.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;context.bindlessTextures=indexed;context.computeSkinning=compute;
    context.sceneWorldCoordinates=true;context.cullingEnabled=false;context.renderSettings=&settings;
    auto& limits=context.descriptorLimits;limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;limits.maxPerStageResources=4096;
    limits.maxImageDimension2D=4096;
    const auto root=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
    context.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    auto mesh=loadGltfAsset((root/"assets_public/scene_inspector/assets/cube.gltf").string());
    mesh.primitives.resize(1);mesh.primitives[0].materialIndex=0;mesh.materials.resize(1);
    mesh.hasSkin=false;mesh.animations.clear();mesh.morphRanges.clear();mesh.morphTargetCount=0;
    auto& material=mesh.materials[0];material.materialFeatures=MaterialFeatureNeutralFallback;material.materialClass=AssetMaterialClass::Generic;
    if(outlined){material.materialFeatures|=MaterialFeatureSilhouetteOutline;material.outlineSurface.present=true;material.outlineSurface.width=.75F;}
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{"motion-fixture/cube.gltf",std::make_shared<const LoadedAsset>(mesh)}};
    context.preparedMeshes=&prepared;context.scene.resources.push_back({"cube","motion-fixture/cube.gltf"});
    for(unsigned i=0;i<2;++i){scene::SceneNodeDesc node;node.id="object-"+std::to_string(i);node.resourceId="cube";node.translation[0]=i?.3F:-.3F;context.scene.nodes.push_back(node);}
    context.sceneRenderPass=backend.createRenderPass({});context.shadowRenderPass=backend.createRenderPass({});
    {
        MotionVectorTarget target(backend,backend.allocator(),context.renderExtent,context.depthFormat,{reinterpret_cast<VkImageView>(0x1000)});
        context.motionOutput=&target;CharacterSceneRenderer renderer;
        checkMotionRecording(renderer.capabilities().supportsMotionVectors,"Generic mesh rendering declares motion support");
        renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.cameraPosition[2]=3;
        frame.swapchainWidth=64;frame.swapchainHeight=48;renderer.updateFrame(frame);
        checkMotionRecording(motionManifest(renderer).at("motionPreviousValidObjects")==0,"First-frame geometry cannot borrow a previous object");
        checkMotionRecording(renderer.sceneState()->temporalFrame&&!renderer.sceneState()->temporalFrame->previous,"First camera state is exposed to the view owner");
        RenderGraph graph;SceneGraphResources resources{graph.addResource("hdr"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
        rhi::ImageBarrierDesc initial;initial.image=target.image().image;resources.motion=graph.importImage("motion",initial);
        initial.image=target.surfaceDepthImage().image;resources.motionSurfaceDepth=graph.importImage("motion-surface-depth",initial);
        renderer.registerPasses(graph,resources,context);std::string error;checkMotionRecording(graph.compile(error),"Motion and ordinary scene passes form one valid graph");
        const auto motion=std::find_if(graph.passes().begin(),graph.passes().end(),[](const auto& pass){return pass.name=="character-motion";});
        checkMotionRecording(motion!=graph.passes().end(),"Production graph registers a motion pass");
        bool depthRead=false,vectorsWritten=false,surfaceWritten=false;
        for(const auto& use:motion->uses){depthRead|=use.resource==resources.depth&&!use.write&&use.usage==RenderGraphUsage::DepthReadAttachment;
            vectorsWritten|=use.resource==resources.motion&&use.write;surfaceWritten|=use.resource==resources.motionSurfaceDepth&&use.write;}
        checkMotionRecording(depthRead&&vectorsWritten&&surfaceWritten,"Motion writes its outputs while preserving hardware depth values");
        rhi::NullCommandRecorder commands;graph.execute(&commands);unsigned draws=0;for(const auto& call:commands.calls)draws+=call.name=="drawIndexed";
        checkMotionRecording(draws>0,"Motion recording participates in actual draw submission");
        renderer.onFrameSubmitted(3);auto scene=context.scene;std::reverse(scene.nodes.begin(),scene.nodes.end());scene.nodes[0].translation[0]+=.1F;
        frame.sceneSnapshot=std::make_shared<const scene::SceneDescription>(scene);frame.currentFrame=1;context.currentFrame=1;renderer.updateFrame(frame);
        checkMotionRecording(motionManifest(renderer).at("motionPreviousValidObjects")==2,"Reordered instances match stable previous identities");
        checkMotionRecording(renderer.sceneState()->temporalFrame->previousSubmission==3,"Motion and camera inputs use the same actual submission");
        renderer.onFrameAbandoned();scene.nodes.resize(1);frame.sceneSnapshot=std::make_shared<const scene::SceneDescription>(scene);renderer.updateFrame(frame);
        checkMotionRecording(motionManifest(renderer).at("motionPreviousValidObjects")==1,"Cancel and visibility changes preserve the correct submitted object");
        renderer.onSwapchainRecreate(context);renderer.updateFrame(frame);
        checkMotionRecording(motionManifest(renderer).at("motionPreviousValidObjects")==0,"View reconstruction invalidates object history");
        renderer.onUnload(context);checkMotionRecording(backend.allocator().statistics().liveBuffers==0&&backend.allocator().statistics().liveImages==2,
            "Mesh renderer releases motion buffers and preserves the view-owned target");
    }
    backend.destroyRenderPass(context.sceneRenderPass);backend.destroyRenderPass(context.shadowRenderPass);
    checkMotionRecording(backend.allocator().statistics().liveImages==0,"Motion view ownership retires all images");
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    for(bool indexed:{false,true})for(bool compute:{false,true})for(bool outlined:{false,true})verifyMotionRecording(argv[1],indexed,compute,outlined);
    {
        rhi::NullRhi backend;backend.unavailableSampledFormats.insert(MotionVectorTarget::kFormat);
        RenderContext base;base.rhi=&backend;base.allocator=&backend.allocator();base.shaderDirectory=argv[1];
        base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;
        base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;base.descriptorLimits.maxImageDimension2D=4096;
        const auto root=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);base.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
        RenderViewService views(base,[](const std::string&){return std::make_unique<CharacterSceneRenderer>();},[]{});
        RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={64,48};descriptor.settings.antiAliasing=3;
        descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;descriptor.settings.characterPresentation.platformEnabled=false;
        descriptor.scene.resources.push_back({"cube",(root/"assets_public/scene_inspector/assets/cube.gltf").string()});
        scene::SceneNodeDesc node;node.id="object";node.resourceId="cube";descriptor.scene.nodes.push_back(node);
        const auto view=views.create(descriptor);const auto report=views.describe(view);
        checkMotionRecording(report.at("temporalAvailable")==false&&report.at("temporalBytes")==0&&!report.at("temporalFallback").get<std::string>().empty(),
            "Unsupported motion storage format selects an observable fallback without allocating histories");
        views.request(view);SceneFrameData frame;RenderGraph graph;views.schedule(graph,frame,base,1);std::string error;
        checkMotionRecording(graph.compile(error),"Unavailable motion format preserves ordinary scene output");rhi::NullCommandRecorder commands;graph.execute(&commands);
        checkMotionRecording(views.submitted(1),"Spatial fallback submits through the normal owner boundary");views.release(view);views.complete(1);
        checkMotionRecording(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Fallback releases partial and ordinary scene resources");
    }
    std::cout<<"Motion recording: descriptors, skinning, outlines, identity, cancellation and recreation passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
