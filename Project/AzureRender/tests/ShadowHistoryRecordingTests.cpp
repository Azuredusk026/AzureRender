#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <sstream>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(const char* shaders,const std::string& root,bool indexed,unsigned mode,bool projected){
 rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
 context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders;
 context.renderExtent={128,96};context.bindlessTextures=indexed;context.cullingEnabled=false;context.qaInstanceCount=4;
 context.fragmentPrimitiveIdentity=mode!=1;context.fragmentStorageAtomics=mode!=2;
 context.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;context.depthFormat=VK_FORMAT_D32_SFLOAT;
 auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
 limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
 limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
 limits.maxPerStageDescriptorStorageBuffers=mode==3?7:64;limits.maxDescriptorSetStorageBuffers=mode==4?(projected?13:12):64;
 limits.maxStorageBufferRange=mode==5?65536:128*1024*1024;
 const auto path=root+"/assets_public/scene_inspector/assets/cube.gltf";
 context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
 auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
 for(auto& material:asset->materials){material.materialClass=AssetMaterialClass::Generic;material.materialFeatures=0;material.materialProfileVersion=2;material.outlineSurface.present=projected;material.outlineSurface.projectSurface=projected;
 material.environmentLighting.diffuse=MaterialEnvironmentSource::GlobalEnvironment;}
 std::map<std::string,std::shared_ptr<const LoadedAsset>> meshes{{path,asset}};context.preparedMeshes=&meshes;
 context.scene.resources.push_back({"mesh",path});scene::SceneNodeDesc node;node.id="object";node.resourceId="mesh";context.scene.nodes.push_back(node);
 RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;settings.sceneLightingEnabled=true;context.renderSettings=&settings;
 renderer.onLoad(context);
 std::ostringstream manifest;renderer.appendCaptureManifestFields(manifest);
 const bool prepared=manifest.str().find("\"shadowHistoryPrepared\": true")!=std::string::npos;
 require(prepared==(AZURE_EXPECT_SHADOW_HISTORY&&mode==0),"History must obey fragment identity, atomics and storage descriptor limits");
 for(unsigned generation=0;generation<2;++generation){
  if(generation)renderer.onSwapchainRecreate(context);
  for(unsigned index=0;index<4;++index){
   SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;frame.currentFrame=index%2;context.currentFrame=index%2;
   renderer.updateFrame(frame);RenderGraph graph;
   SceneGraphResources resources{graph.addResource("color"),graph.addResource("depth"),graph.addResource("normal"),graph.addResource("shadow")};
   renderer.registerPasses(graph,resources,context);std::string error;require(graph.compile(error),error.c_str());
   rhi::NullCommandRecorder commands;for(const auto& pass:graph.passes())if(pass.recordCommands)pass.recordCommands(commands);
   if(index==1)renderer.onFrameAbandoned();else renderer.onFrameSubmitted(generation*4+index+1);
  }
 }
 renderer.onUnload(context);
 require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages,"History load/recreate/abandon must release all resources");
}
int main(int argc,char** argv)try{
 require(argc==3,"Expected shaders and source root");
 for(bool projected:{false,true})for(bool indexed:{false,true})for(unsigned mode=0;mode<6;++mode)verify(argv[1],argv[2],indexed,mode,projected);
 std::cout<<"History runtime: twenty-four capability, fallback and lifecycle cases passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
