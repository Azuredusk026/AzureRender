#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace azurerender;
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}

void requireReleasedPixels(const AssetMaterial& material) {
    for(const auto* pixels:{&material.baseColorPixels,&material.normalPixels,&material.metallicRoughnessPixels,
            &material.specularEmissivePixels,&material.styleMaskPixels,&material.matcapPixels,
            &material.hairDataPixels,&material.shadowColorLutPixels,&material.projectedHairRamp.parameterPixels})
        require(pixels->empty()&&pixels->capacity()==0,"Uploaded renderer materials must release CPU pixel allocations");
}

void verify(bool bindless,const std::filesystem::path& shaderDirectory,bool prepared=false) {
    rhi::NullRhi rhi;CharacterSceneRenderer renderer;RenderContext context;
    context.allocator=&rhi.allocator();context.rhi=&rhi;context.bindlessTextures=bindless;
    context.shaderDirectory=shaderDirectory.string();
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;
    context.renderSettings=&settings;
    auto& limits=context.descriptorLimits;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    limits.maxPerStageResources=4096;
    for(const auto& letter:{"a","b"}) {
        const auto id=std::string("resource-")+letter;
        context.scene.resources.push_back({id,(source/"assets_public/texture_sharing"/(std::string("material-")+letter+".gltf")).string()});
        scene::SceneNodeDesc node;node.id=std::string("instance-")+letter;node.resourceId=id;
        context.scene.nodes.push_back(node);
    }
    std::map<std::string,std::shared_ptr<const LoadedAsset>> preparedMeshes;
    if(prepared) {
        for(const auto& resource:context.scene.resources)
            preparedMeshes.emplace(std::filesystem::u8path(resource.path).lexically_normal().generic_u8string(),
                std::make_shared<const LoadedAsset>(loadGltfAsset(resource.path)));
        context.preparedMeshes=&preparedMeshes;
    }
    for(unsigned load=0;load<2;++load) {
        while(!renderer.prepareLoad(context,0)){}
        std::ostringstream fields;fields<<'{';renderer.appendCaptureManifestFields(fields);fields<<"\"end\":0}";
        const auto report=nlohmann::json::parse(fields.str());
        const auto& stats=report.at("materialTextureCache");
        // Each imported resource includes its authored material and the loader's
        // fallback. Six generated 2x2 maps and one 1x1 LUT are common; the two
        // base colors and the two ORM factor combinations remain distinct.
        require(stats.at("requests")==32,"Both resources must submit eight texture slots for authored and fallback materials");
        require(stats.at("hits")==23&&stats.at("uniqueTextures")==9,"Repeated resources and equivalent fallback slots must share exactly equal upload inputs");
        require(stats.at("requestedBytes")==576&&stats.at("residentBytes")==164,"Logical residency must retain all texels and mips while eliminating repeated uploads");
        require(stats.at("copiedPixelBytes")==0&&stats.at("transferredPixelBytes")==132&&stats.at("discardedPixelBytes")==332,
            "Unique renderer inputs transfer into the cache while redundant allocations are released without copying");
        require(report.at("sceneLoad").at("decodedMaterialPixelBytes")==464
            &&report.at("sceneLoad").at("remainingMaterialPixelBytes")==0,
            "Completed material preparation must release every decoded source allocation used by this scene");
        const auto* state=renderer.sceneState();
        require(state&&state->asset,"Loaded geometry remains available for selection and animation");
        for(const auto& material:state->asset->materials)requireReleasedPixels(material);
        for(const auto& pickable:state->pickables)
            for(const auto& material:pickable.asset->materials)requireReleasedPixels(material);
        for(const auto& [path,mesh]:preparedMeshes)
            require(!mesh->materials.front().baseColorPixels.empty(),"Borrowed prepared assets retain their decoded source pixels");
        require(renderer.reuseScene(context),"Shared material ownership preserves scene reuse");
        renderer.onUnload(context);
        require(rhi.allocator().statistics().liveImages==0&&rhi.allocator().statistics().liveBuffers==0,"Renderer unload must free shared materials and independent attachments exactly once");
    }
}

void verifyProjectedPixels(bool bindless,const std::filesystem::path& shaders) {
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
    const auto path=(source/"assets_public/texture_sharing/material-a.gltf").generic_string();
    auto prepared=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    auto& material=prepared->materials.at(0);auto& hair=material.projectedHairRamp;
    material.materialClass=AssetMaterialClass::Hair;material.materialProfileVersion=2;
    material.materialFeatures=MaterialFeatureHairProjectedRamp|MaterialFeatureHairAnisotropy;
    hair.present=true;hair.parameterWidth=hair.parameterHeight=2;
    const std::vector<std::uint8_t> sourcePixels{13,59,101,197, 23,71,113,211, 37,83,127,223, 47,97,139,239};
    hair.parameterPixels=sourcePixels;
    material.textureSettings[5].colorSpace=material.textureSettings[7].colorSpace="linear";
    std::map<std::string,std::shared_ptr<const LoadedAsset>> meshes{{path,prepared}};
    context.rhi=&backend;context.allocator=&backend.allocator();context.preparedMeshes=&meshes;
    context.shaderDirectory=shaders.string();context.bindlessTextures=bindless;
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();
    auto& limits=context.descriptorLimits;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    limits.maxPerStageResources=4096;
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;context.renderSettings=&settings;
    context.scene.resources.push_back({"surface",path});scene::SceneNodeDesc node;node.id="surface";node.resourceId="surface";context.scene.nodes.push_back(node);
    for(unsigned load=0;load<2;++load) {
        renderer.onLoad(context);
        require(hair.parameterPixels==sourcePixels,"Borrowed prepared hair parameters must retain their original allocation and bytes");
        const auto* state=renderer.sceneState();require(state&&state->asset,"Projected material source must remain queryable");
        for(const auto& output:state->asset->materials)requireReleasedPixels(output);
        for(const auto& pickable:state->pickables)for(const auto& output:pickable.asset->materials)requireReleasedPixels(output);
        std::ostringstream fields;fields<<'{';renderer.appendCaptureManifestFields(fields);fields<<"\"end\":0}";
        const auto report=nlohmann::json::parse(fields.str());
        require(report.at("sceneLoad").at("remainingMaterialPixelBytes")==0,
            "Projected parameter uploads must release every renderer source pixel allocation");
        renderer.onUnload(context);
        require(backend.allocator().statistics().liveBuffers==0&&backend.allocator().statistics().liveImages==0,
            "Projected parameters and immutable material table must release on every unload");
    }
}

int main(int argc,char** argv) {try {
    require(argc==2,"Expected compiled shader directory");
    verify(false,argv[1]);verify(true,argv[1]);verify(false,argv[1],true);verify(true,argv[1],true);
    verifyProjectedPixels(false,argv[1]);verifyProjectedPixels(true,argv[1]);
    std::cout<<"Public multi-resource fixture: fixed/indexed bindings and reload sharing passed\n";
}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
