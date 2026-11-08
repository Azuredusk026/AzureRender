#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
using namespace azurerender;
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void verify(const std::string& shaders,bool indexed,bool compute,bool identity,bool format=true,bool quad=true){
 rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;RenderSettings settings;
 context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
 context.renderExtent={128,96};context.bindlessTextures=indexed;context.computeSkinning=compute;context.fragmentPrimitiveIdentity=identity;context.fragmentQuadControl=quad;context.cullingEnabled=false;
 auto& limits=context.descriptorLimits;limits.maxColorAttachments=6;limits.maxImageDimension2D=4096;
 limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
 limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;limits.maxPerStageResources=4096;
 const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
 const auto path=(source/"assets_public/scene_inspector/assets/cube.gltf").generic_string();
 context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();
 auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));for(auto& m:asset->materials){m.materialClass=AssetMaterialClass::Generic;m.materialFeatures=MaterialFeatureNeutralFallback;m.alphaMode=AssetAlphaMode::Opaque;}
 std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,asset}};context.preparedMeshes=&prepared;
 context.scene.resources.push_back({"cube",path});scene::SceneNodeDesc node;node.id="cube";node.resourceId="cube";context.scene.nodes.push_back(node);
 settings.sceneLightingEnabled=true;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;settings.characterPresentation.backgroundEnabled=false;context.renderSettings=&settings;
 if(!format)backend.unavailableSampledFormats.insert(VK_FORMAT_R32G32B32A32_UINT);
 renderer.onLoad(context);const bool enabled=identity&&format&&quad;
 std::ostringstream fields;renderer.appendCaptureManifestFields(fields);auto manifest=nlohmann::json::parse("{"+fields.str()+"\"fixture\":true}");
 require(manifest.at("surfaceRadiancePrepared")==enabled,"Capability and format select the full fallback");
 require(manifest.at("surfaceRadianceBytes")==std::uint64_t(enabled?128*96*64*2:0),"View storage has exact accounting");
 unsigned visibilityBindings=0;for(const auto& write:backend.descriptorImageWrites)if(write.binding>=38&&write.binding<=41){require(write.view!=VK_NULL_HANDLE,"Every cache descriptor owns a view");++visibilityBindings;}
 require((visibilityBindings>0)==enabled,"Descriptors follow actual cache selection");
 SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;frame.cameraPosition[2]=5;
 renderer.updateFrame(frame);rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);
 RenderGraph graph;SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};renderer.registerPasses(graph,resources,context);
 const auto prepass=std::find_if(graph.passes().begin(),graph.passes().end(),[](const auto& p){return p.name=="character-opaque-depth";});require(prepass!=graph.passes().end(),"Snapshot pass registered");
 require(prepass->renderPass.clearValues.size()==(enabled?7U:3U),"Snapshot clears every owned attachment");
 if(enabled){require(prepass->renderPass.clearValues[6].color.uint32[0]==UINT32_MAX,"Identity begins invalid");
  bool wrote=false;for(const auto& call:backend.calls)wrote|=call.name=="createGraphicsPipeline"&&call.detail.find("masks=15/15/15/15/15/15/")!=std::string::npos;require(wrote,"Ordinary snapshot enables all cache outputs");}
 std::string error;require(graph.compile(error),error.c_str());graph.execute(&commands);
 renderer.onSwapchainRecreate(context);renderer.onUnload(context);
 require(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Recreation and unload release view storage");
}
}
int main(int argc,char** argv)try{require(argc==2,"Expected shaders");for(bool indexed:{false,true})for(bool compute:{false,true})for(bool identity:{false,true})verify(argv[1],indexed,compute,identity);verify(argv[1],false,false,true,false);verify(argv[1],false,false,true,true,false);std::cout<<"Surface radiance recording, fixed/indexed bindings, fallback and lifecycle passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
