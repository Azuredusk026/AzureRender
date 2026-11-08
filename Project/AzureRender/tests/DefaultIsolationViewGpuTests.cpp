#include "render/RenderViewService.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    require(argc==3,"Expected shaders and output directory");
    test::VulkanComputeFixture host(true);
    const std::filesystem::path root=AZURERENDER_TEST_SOURCE_DIR,output=argv[2];std::filesystem::create_directories(output);
    nlohmann::json model;std::ifstream(root/"assets_public/scene_inspector/assets/cube.gltf")>>model;
    model["images"]=nlohmann::json::array();model["textures"]=nlohmann::json::array();
    model["materials"]=nlohmann::json::array({{{"name","ordinary-view-surface"},{"doubleSided",true},
        {"pbrMetallicRoughness",{{"baseColorFactor",{.32,.14,.08,1}}}},
        {"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","generic"},{"features",nlohmann::json::array()}}}}}}});
    for(auto& mesh:model["meshes"])for(auto& primitive:mesh["primitives"])primitive["material"]=0;
    std::ofstream(output/"scene.gltf")<<model.dump();
    model["materials"][0]["extras"]["azureRenderMaterial"]["environmentLighting"]={{"schemaVersion",1},{"diffuse","global-environment"},{"reflection","global-environment"},{"globalIntensity",.5}};
    std::ofstream(output/"lighting.gltf")<<model.dump();
    RenderContext base;base.rhi=host.backend.get();base.allocator=&host.allocator;base.device=host.device;
    base.physicalDevice=host.physical;base.graphicsQueue=host.queue;base.commandPool=host.pool;base.graphicsQueueFamily=host.family;
    base.descriptorLimits=host.properties.limits;base.shadowMapSize=32;base.shaderDirectory=argv[1];base.cullingEnabled=false;
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    nlohmann::json receipt;receipt["checks"]=nlohmann::json::array();receipt["device"]=host.properties.deviceName;
    std::uint64_t serial=0;CharacterSceneRenderer* renderer=nullptr;
    {
        RenderViewService service(base,[&](const std::string&){auto value=std::make_unique<CharacterSceneRenderer>();renderer=value.get();return value;},[&]{vkDeviceWaitIdle(host.device);});
        for(unsigned lighting=0;lighting<2;++lighting){
        RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={48,48};
        descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;
        descriptor.settings.innerOutlineEnabled=descriptor.settings.silhouetteOutlineEnabled=false;
        descriptor.settings.characterPresentation={false,false};descriptor.settings.bloom.enabled=false;
        descriptor.scene.resources.push_back({"surface",(output/(lighting?"lighting.gltf":"scene.gltf")).string()});
        scene::SceneNodeDesc node;node.id="surface";node.resourceId="surface";descriptor.scene.nodes.push_back(node);
        const auto view=service.create(descriptor);
        const unsigned isolations[]={0,0,0,0,1,22,0,0};
        for(unsigned phase=0;phase<8;++phase){
            if(phase==7)service.resize(view,{64,40});
            ++serial;base.currentFrame=serial%2;SceneFrameData frame;frame.timeSeconds=phase/60.0;
            frame.qaIsolationMode=isolations[phase];frame.qaEffectMode=phase==6?1:0;frame.qaEffectEnabled=phase!=6;
            service.request(view);RenderGraph graph;service.schedule(graph,frame,base,serial);std::string error;require(graph.compile(error),error.c_str());
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
            require(service.submitted(serial),"View submission must commit");service.complete(serial);
            if(phase<3)continue;
            const auto pixels=service.readPixels(view);const std::size_t expected=phase==7?64*40*4:48*48*4;
            require(pixels.size()==expected,"View readback must follow resize");
            const auto file=output/("phase-"+std::to_string(phase)+"-lighting-"+std::to_string(lighting)+".rgba");std::ofstream bytes(file,std::ios::binary);bytes.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());bytes.close();
            std::ostringstream metadata;metadata<<'{';renderer->appendCaptureManifestFields(metadata);metadata<<"\"end\":true}";
            auto state=nlohmann::json::parse(metadata.str());
            const bool candidate=true;
            require(state["defaultIsolationPipelines"].get<unsigned>()==(candidate?4U*(lighting+1U):0U),"View must prepare actual main-only default-isolation pipelines");
            require(state["defaultIsolationSelected"].get<bool>()==(candidate&&isolations[phase]==0),"View must preserve dynamic diagnostics");
            require(state["defaultIsolationFallback"]=="","View preparation must succeed");
            receipt["checks"].push_back({{"phase",phase},{"lighting",lighting},{"isolation",isolations[phase]},{"bytes",pixels.size()},{"metadata",state}});
        }
        service.release(view);service.complete(serial);service.clear();service.complete(serial);
        }
    }
    require(host.errors.load()==0,"View probe must have zero Vulkan synchronization and validation errors");
    require(!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"View probe must retire all allocations");
    receipt["validationErrors"]=host.errors.load();receipt["zeroAllocator"]=true;receipt["status"]="passed";
    std::ofstream(output/"summary.json")<<receipt.dump(2);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
