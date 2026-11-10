#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace azurerender;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv)try{
    require(argc==3,"Expected shader and evidence directories");
    std::unique_ptr<test::VulkanComputeFixture> host;bool quad=true;
    try{host=std::make_unique<test::VulkanComputeFixture>(false,false,false,false,true,false,true,false,true);}
    catch(const test::VulkanFeatureUnavailable&){quad=false;host=std::make_unique<test::VulkanComputeFixture>();}
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR),output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
    const auto path=(source/"assets_public/scene_inspector/assets/cube.gltf").generic_string();
    auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    for(auto& m:asset->materials){m.materialClass=AssetMaterialClass::Generic;m.materialFeatures=MaterialFeatureNeutralFallback;m.alphaMode=AssetAlphaMode::Opaque;}
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,asset}};
    nlohmann::json report={{"device",host->properties.deviceName},{"quadEnabled",quad},{"cases",nlohmann::json::array()}};
    const bool qualified=AZURE_TEST_COMPUTED_MATERIAL_VIEW&&quad&&host->properties.vendorID==0x10deU;
    for(bool depth:{true,false}){
        rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;RenderSettings settings;
        context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=argv[1];context.physicalDevice=host->physical;
        context.renderExtent={128,96};context.shadowMapSize=32;context.bindlessTextures=quad;context.fragmentQuadControl=quad;
        context.depthTestEnabled=depth;context.computeShaders=true;context.rgba16fStorageImage=true;context.cullingEnabled=false;
        context.descriptorLimits=host->properties.limits;context.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;context.depthFormat=context.shadowFormat=VK_FORMAT_D32_SFLOAT;
        context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();context.preparedMeshes=&prepared;
        context.scene.resources.push_back({"cube",path});scene::SceneNodeDesc node;node.id="cube";node.resourceId="cube";context.scene.nodes.push_back(node);
        settings.sceneLightingEnabled=true;settings.screenSpaceReflection.enabled=true;settings.faceSdfFeature=settings.showcasePlatformFeature=false;
        settings.characterPresentation={false,false};context.renderSettings=&settings;renderer.onLoad(context);
        SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;frame.cameraPosition[2]=5;
        renderer.updateFrame(frame);rhi::NullCommandRecorder commands;context.commands=&commands;renderer.recordScene(context);
        std::ostringstream fields;fields<<'{';renderer.appendCaptureManifestFields(fields);fields<<"\"end\":true}";const auto data=nlohmann::json::parse(fields.str());
        renderer.onUnload(context);const bool zero=!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers;
        report["cases"].push_back({{"depthTest",depth},{"preparedPipelines",data.at("computedMaterialViewPipelines")},{"frameEligible",data.at("computedMaterialViewFrameEligible")},{"selections",data.at("computedMaterialViewSelectionsTotal")},{"fallback",data.at("computedMaterialViewFallback")},{"reflectionPrepared",data.at("screenReflectionComputePrepared")},{"zeroAllocator",zero}});
        std::ofstream(output/"results.json")<<report.dump(2)<<'\n';require(zero,"Depth policy recording must release every owned resource");
        if(depth&&qualified){require(data.at("computedMaterialViewPipelines").get<unsigned>()>0&&data.at("computedMaterialViewFrameEligible").get<bool>(),"Qualified depth-enabled view must prepare and select its existing optimized policy");}
        else require(data.at("computedMaterialViewPipelines")==0&&!data.at("computedMaterialViewFrameEligible").get<bool>(),"Depth-disabled or unqualified view must retain reference pipelines");
    }
    require(host->errors.load()==0,"Capability fixture must have no Vulkan validation messages");
    report["status"]="passed";report["validationErrors"]=host->errors.load();std::ofstream(output/"results.json")<<report.dump(2)<<'\n';
    std::cout<<report.dump()<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
