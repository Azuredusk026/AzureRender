#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
nlohmann::json manifest(const CharacterSceneRenderer& renderer){std::ostringstream text;text<<'{';renderer.appendCaptureManifestFields(text);text<<"\"fixture\":true}";return nlohmann::json::parse(text.str());}
void requireOffset(const nlohmann::json& data,float x,float y,const char* message){
    const auto& offset=data.at("screenReflectionMotionUvOffset");
    require(std::fabs(offset.at(0).get<float>()-x)<1e-8F&&std::fabs(offset.at(1).get<float>()-y)<1e-8F,message);
}
enum class HistoryMode {Owned,Shared,Multisample,StorageLimited};
void fixture(const char* shaders,bool bindless,bool fallback,HistoryMode mode){
    rhi::NullRhi backend;if(fallback)backend.unavailableStorageFormats.insert(VK_FORMAT_R16G16_UNORM);
    backend.supportedSamples=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;backend.sampleShadingSupported=true;
    CharacterSceneRenderer renderer;RenderContext context;context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
    context.renderExtent={128,96};context.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;context.depthFormat=VK_FORMAT_D32_SFLOAT;context.bindlessTextures=bindless;context.computeShaders=true;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;limits.maxPerStageResources=4096;
    const bool temporal=mode==HistoryMode::Owned||mode==HistoryMode::Shared;
    if(mode==HistoryMode::StorageLimited)limits.maxPerStageDescriptorStorageBuffers=5;
    if(mode==HistoryMode::Multisample)context.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;
    std::unique_ptr<MotionVectorTarget> shared; rhi::GpuImage sharedDepth{};VkImageView sharedDepthView{};
    if(mode==HistoryMode::Shared){
        sharedDepth=backend.allocator().createImage2D(128,96,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        sharedDepthView=backend.createImageView(sharedDepth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
        shared=std::make_unique<MotionVectorTarget>(backend,backend.allocator(),context.renderExtent,context.depthFormat,std::vector<VkImageView>{sharedDepthView});
        context.motionOutput=shared.get();
    }
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);const auto path=(source/"assets_public/scene_inspector/assets/cube.gltf").generic_string();
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,asset}};context.preparedMeshes=&prepared;context.scene.resources.push_back({"cube",path});
    scene::SceneNodeDesc node;node.id="cube";node.resourceId="cube";context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;settings.sceneLightingEnabled=true;
    if(mode==HistoryMode::Shared)settings.antiAliasing=3;
    settings.screenSpaceReflection.enabled=true;settings.screenSpaceReflection.algorithm=ScreenSpaceReflectionAlgorithm::Projected;context.renderSettings=&settings;
    if(mode==HistoryMode::StorageLimited){
        bool rejected=false;
        try{renderer.onLoad(context);}catch(const std::runtime_error& error){
            rejected=std::string(error.what()).find("Scene storage buffers cannot fit")!=std::string::npos;
        }
        require(rejected,"A device below the scene layout minimum must reject loading before history preparation");
        renderer.onUnload(context);
        require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Rejected scene loading must release its resources");
        return;
    }
    renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;
    frame.cameraOverride=true;frame.cameraPosition[1]=2;frame.cameraPosition[2]=5;
    for(unsigned slot=0;slot<2;++slot){frame.currentFrame=context.currentFrame=slot;renderer.updateFrame(frame);rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);
        const auto data=manifest(renderer);require(data.at("screenReflectionComputePrepared")&&data.at("screenReflectionComputeActive"),"Scene must record the selected compute producer");
        require(data.at("screenReflectionAlgorithm")== (fallback?"linear":"projected"),"Scene algorithm diagnostic differs");
        if(!fallback){
            require(data.at("screenReflectionTemporalPrepared")==temporal,"Projected history preparation must respect motion and geometry support");
            require(data.at("screenReflectionHistoryValid")==bool(temporal&&slot),"Projected history must follow the successfully submitted previous slot");
            require(data.at("screenReflectionProducer")== (temporal&&slot?"projected-history":"projected-current-frame"),"Projected producer must report the actual current or submitted-history input");
            require(data.at("screenReflectionHistoryInputSubmission")== (temporal?slot:0),"Recording must select the last successful reflection submission");
            require(data.at("screenReflectionOwnsMotion")== (mode==HistoryMode::Owned),"Projected history must preserve caller ownership of shared motion");
            if(mode==HistoryMode::Shared&&slot)requireOffset(data,-.1953125F/128,(1.F/6)/96,"Shared temporal motion must correct from the submitted zero-jitter source to the actual sampling projection");
            else requireOffset(data,0,0,"Current-frame and unjittered history must use zero sampling correction");
            if(!temporal)require(data.at("screenReflectionTemporalPreparation")== (mode==HistoryMode::Multisample?"multisample-motion-unavailable":"geometry-storage-limits"),"Unavailable history needs a specific observable fallback reason");
        }
        else require(!data.at("screenReflectionFallback").get<std::string>().empty(),"Format fallback needs an observable reason");
        require(std::any_of(commands.calls.begin(),commands.calls.end(),[](const auto& call){return call.name=="dispatch";}),"Direct recording must dispatch reflection compute work");
        renderer.onFrameAbandoned();
        require(manifest(renderer).at("screenReflectionHistorySubmission")== (temporal?slot:0),"Abandoned recording must preserve the published history identity");
        renderer.updateFrame(frame);renderer.recordScene(context);renderer.onFrameSubmitted(slot+1);
        require(manifest(renderer).at("screenReflectionHistorySubmission")== (temporal?slot+1:0),"Only a successful history-capable recording may publish its source");
    }
    if(!fallback&&temporal){
        const auto record=[&](unsigned slot){frame.currentFrame=context.currentFrame=slot;renderer.updateFrame(frame);
            rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);return manifest(renderer);};
        settings.screenSpaceReflection.enabled=false;record(0);renderer.onFrameSubmitted(3);
        settings.screenSpaceReflection.enabled=true;auto gap=record(1);
        require(!gap.at("screenReflectionHistoryValid")&&gap.at("screenReflectionHistoryInputSubmission")==0,"A submitted frame without reflection must invalidate the old reflection source");
        renderer.onFrameSubmitted(4);auto restored=record(0);
        require(restored.at("screenReflectionHistoryValid")&&restored.at("screenReflectionHistoryInputSubmission")==4,"Current-frame fallback must establish the next successful history source");
        renderer.onFrameSubmitted(5);auto same=record(0);
        require(!same.at("screenReflectionHistoryValid")&&same.at("screenReflectionProducer")=="projected-current-frame","A slot must not sample its own depth as previous history");renderer.onFrameAbandoned();
        auto afterCancel=record(1);require(afterCancel.at("screenReflectionHistoryValid")&&afterCancel.at("screenReflectionHistoryInputSubmission")==5,"Cancelled slot reuse must preserve the submitted previous depth identity");renderer.onFrameAbandoned();
        if(mode==HistoryMode::Shared)requireOffset(afterCancel,-.25F/128,(-5.F/9)/96,"Cancelled sampling jitter must not replace the successfully submitted history jitter");
        frame.camera.source=RenderCameraSource::SceneView;auto cut=record(1);
        require(!cut.at("screenReflectionHistoryValid"),"Changing the view camera source must invalidate projected history");renderer.onFrameSubmitted(6);
        frame.camera.source=RenderCameraSource::Fallback;auto cancelledCut=record(0);
        require(!cancelledCut.at("screenReflectionHistoryValid"),"A pending camera-source change must not reuse the committed source");renderer.onFrameAbandoned();
        frame.camera.source=RenderCameraSource::SceneView;auto cameraRetry=record(0);
        require(cameraRetry.at("screenReflectionHistoryValid")&&cameraRetry.at("screenReflectionHistoryInputSubmission")==6,"Abandoned camera-source changes must preserve the submitted sampling view");renderer.onFrameAbandoned();
    }
    if(!fallback){
        frame.currentFrame=context.currentFrame=0;settings.cameraView=CameraViewDescriptor{};settings.cameraView->projection=CameraProjectionKind::Orthographic;
        renderer.updateFrame(frame);rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);auto data=manifest(renderer);
        require(!data.at("screenReflectionComputeActive")&&data.at("screenReflectionProducer")=="fragment"&&!data.at("screenReflectionFallback").get<std::string>().empty(),"Orthographic projection needs an observable fragment fallback");renderer.onFrameAbandoned();
        settings.cameraView.reset();settings.screenSpaceReflection.algorithm=ScreenSpaceReflectionAlgorithm::Linear;renderer.updateFrame(frame);renderer.recordScene(context);data=manifest(renderer);
        require(!data.at("screenReflectionComputeActive")&&data.at("screenReflectionProducer")=="fragment"&&data.at("screenReflectionFallback")=="resource-rebuild-required","Algorithm changes must respect the view resource boundary");renderer.onFrameAbandoned();
    }
    renderer.onUnload(context);shared.reset();if(sharedDepthView)backend.destroyImageView(sharedDepthView);backend.allocator().destroyImage(sharedDepth);
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Projected scene and caller ownership leaked resources");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");for(bool bindless:{false,true})for(bool fallback:{false,true})
        for(const auto mode:{HistoryMode::Owned,HistoryMode::Shared,HistoryMode::Multisample,HistoryMode::StorageLimited})fixture(argv[1],bindless,fallback,mode);
    std::cout<<"PASS: projected scene direct recording, descriptor modes, format fallback, cancel/retry and retirement\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
