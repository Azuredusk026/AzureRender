#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace azurerender;
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}

void verify(bool bindless,const std::filesystem::path& shaderDirectory) {
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
        require(renderer.reuseScene(context),"Shared material ownership preserves scene reuse");
        renderer.onUnload(context);
        require(rhi.allocator().statistics().liveImages==0&&rhi.allocator().statistics().liveBuffers==0,"Renderer unload must free shared materials and independent attachments exactly once");
    }
}

int main(int argc,char** argv) {try {
    require(argc==2,"Expected compiled shader directory");
    verify(false,argv[1]);verify(true,argv[1]);
    std::cout<<"Public multi-resource fixture: fixed/indexed bindings and reload sharing passed\n";
}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
