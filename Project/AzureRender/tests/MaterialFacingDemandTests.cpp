#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(const char* shaders,unsigned sides,bool culling,bool indexed,bool ordinary){
    rhi::NullRhi backend;
    CharacterSceneRenderer renderer;RenderContext context;context.rhi=&backend;context.allocator=&backend.allocator();
    context.shaderDirectory=shaders;context.renderExtent={128,96};context.bindlessTextures=indexed;
    context.faceCullingEnabled=culling;context.cullingEnabled=false;
    context.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;context.depthFormat=VK_FORMAT_D32_SFLOAT;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;limits.maxPerStageResources=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    const std::string root=AZURERENDER_TEST_SOURCE_DIR,path=root+"/assets_public/scene_inspector/assets/cube.gltf";
    context.rampAtlasPath=root+"/assets_public/toon_ramp_atlas.ppm";
    auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    auto material=asset->materials.front();asset->materials.clear();
    for(unsigned side=0;side<2;++side)if(sides&(1U<<side)){
        material.doubleSided=side!=0;material.materialClass=ordinary?AssetMaterialClass::Generic:AssetMaterialClass::Skin;
        asset->materials.push_back(material);
    }
    // A second material group must not broaden the first group's facing set.
    material.materialClass=ordinary?AssetMaterialClass::Skin:AssetMaterialClass::Generic;
    material.doubleSided=sides==1;
    asset->materials.push_back(material);
    std::map<std::string,std::shared_ptr<const LoadedAsset>> meshes{{path,asset}};context.preparedMeshes=&meshes;
    context.scene.resources.push_back({"mesh",path});scene::SceneNodeDesc node;node.id="object";node.resourceId="mesh";context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;context.renderSettings=&settings;
    renderer.onLoad(context);
    for(unsigned generation=0;generation<2;++generation){
        const auto first=generation?backend.calls.size():0;
        if(generation)renderer.onSwapchainRecreate(context);
        unsigned count=0;std::set<unsigned> fronts;
        const auto specialization=ordinary?std::string("fragmentSpec=0:1/")
            :"fragmentSpec=1:"+std::to_string(unsigned(AssetMaterialClass::Skin))+"/";
        for(std::size_t at=first;at<backend.calls.size();++at){const auto& call=backend.calls[at];if(call.name=="createGraphicsPipeline"
            &&call.detail.find(specialization)!=std::string::npos){
            ++count;
            const auto front=call.detail.find(" front=");require(front!=std::string::npos,"Front-face state must be recorded");
            fronts.insert(unsigned(std::stoul(call.detail.substr(front+7))));
            if(culling){
                const bool noCull=call.detail.find(" cull=0 ")!=std::string::npos;
                require(sides==3||noCull==(sides==2),"Unused class cull variant was created");
            }
        }}
        require(count==(sides==3?16U:8U),"Class pipeline count must cover exactly the required facing pairs");
        require(fronts.size()==2,"Both authored front-face orientations must remain available");
    }
    renderer.onUnload(context);
    std::map<std::string,unsigned> created,destroyed;
    for(const auto& call:backend.calls){
        if(call.name=="createGraphicsPipeline"||call.name=="createComputePipeline")++created[call.detail.substr(0,call.detail.find(' '))];
        if(call.name=="destroyPipeline")++destroyed[call.detail];
    }
    require(created==destroyed,"Every created pipeline must release exactly once");
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Scene resources must release");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");
    for(bool ordinary:{false,true})for(unsigned sides:{1U,2U,3U})for(bool culling:{false,true})for(bool indexed:{false,true})verify(argv[1],sides,culling,indexed,ordinary);
    std::cout<<"Material facing demand: 24 load/recreate combinations passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
