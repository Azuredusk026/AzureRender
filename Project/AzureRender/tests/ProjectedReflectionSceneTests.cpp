#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
nlohmann::json manifest(const CharacterSceneRenderer& renderer){std::ostringstream text;text<<'{';renderer.appendCaptureManifestFields(text);text<<"\"fixture\":true}";return nlohmann::json::parse(text.str());}
void fixture(const char* shaders,bool bindless,bool fallback){
    rhi::NullRhi backend;if(fallback)backend.unavailableStorageFormats.insert(VK_FORMAT_R16G16_UNORM);
    CharacterSceneRenderer renderer;RenderContext context;context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
    context.renderExtent={128,96};context.bindlessTextures=bindless;context.computeShaders=true;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;limits.maxPerStageResources=4096;
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);const auto path=(source/"assets_public/scene_inspector/assets/cube.gltf").generic_string();
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,asset}};context.preparedMeshes=&prepared;context.scene.resources.push_back({"cube",path});
    scene::SceneNodeDesc node;node.id="cube";node.resourceId="cube";context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;settings.sceneLightingEnabled=true;
    settings.screenSpaceReflection.enabled=true;settings.screenSpaceReflection.algorithm=ScreenSpaceReflectionAlgorithm::Projected;context.renderSettings=&settings;
    renderer.onLoad(context);SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;
    frame.cameraOverride=true;frame.cameraPosition[1]=2;frame.cameraPosition[2]=5;
    for(unsigned slot=0;slot<2;++slot){frame.currentFrame=context.currentFrame=slot;renderer.updateFrame(frame);rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);
        const auto data=manifest(renderer);require(data.at("screenReflectionComputePrepared")&&data.at("screenReflectionComputeActive"),"Scene must record the selected compute producer");
        require(data.at("screenReflectionAlgorithm")== (fallback?"linear":"projected"),"Scene algorithm diagnostic differs");
        if(!fallback){require(data.at("screenReflectionProducer")=="projected-current-frame"&&!data.at("screenReflectionHistoryValid"),"Current-frame producer must report its actual history scope");}
        else require(!data.at("screenReflectionFallback").get<std::string>().empty(),"Format fallback needs an observable reason");
        require(std::any_of(commands.calls.begin(),commands.calls.end(),[](const auto& call){return call.name=="dispatch";}),"Direct recording must dispatch reflection compute work");
        renderer.onFrameAbandoned();renderer.updateFrame(frame);renderer.recordScene(context);renderer.onFrameSubmitted(slot+1);
    }
    if(!fallback){
        frame.currentFrame=context.currentFrame=0;settings.cameraView=CameraViewDescriptor{};settings.cameraView->projection=CameraProjectionKind::Orthographic;
        renderer.updateFrame(frame);rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);auto data=manifest(renderer);
        require(!data.at("screenReflectionComputeActive")&&data.at("screenReflectionProducer")=="fragment"&&!data.at("screenReflectionFallback").get<std::string>().empty(),"Orthographic projection needs an observable fragment fallback");renderer.onFrameAbandoned();
        settings.cameraView.reset();settings.screenSpaceReflection.algorithm=ScreenSpaceReflectionAlgorithm::Linear;renderer.updateFrame(frame);renderer.recordScene(context);data=manifest(renderer);
        require(!data.at("screenReflectionComputeActive")&&data.at("screenReflectionProducer")=="fragment"&&data.at("screenReflectionFallback")=="resource-rebuild-required","Algorithm changes must respect the view resource boundary");renderer.onFrameAbandoned();
    }
    renderer.onUnload(context);require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Projected scene ownership leaked resources");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");for(bool bindless:{false,true})for(bool fallback:{false,true})fixture(argv[1],bindless,fallback);
    std::cout<<"PASS: projected scene direct recording, descriptor modes, format fallback, cancel/retry and retirement\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
