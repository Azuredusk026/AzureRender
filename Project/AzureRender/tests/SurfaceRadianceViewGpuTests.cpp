#include "render/RenderViewService.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <map>
#include <cstring>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    require(argc==3,"Expected shaders and output directory");
    std::unique_ptr<test::VulkanComputeFixture> owner;bool capable=true;
    try{owner=std::make_unique<test::VulkanComputeFixture>(true,false,false,true,true,(AZURE_TEST_SURFACE_RADIANCE_DIAGNOSTICS||AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION)!=0);}
    catch(const test::VulkanFeatureUnavailable&){capable=false;owner=std::make_unique<test::VulkanComputeFixture>(true);}
    auto& host=*owner;
    const std::filesystem::path root=AZURERENDER_TEST_SOURCE_DIR,output=argv[2];std::filesystem::create_directories(output);
    nlohmann::json model;std::ifstream(root/"assets_public/scene_inspector/assets/cube.gltf")>>model;
    model["images"]=nlohmann::json::array();model["textures"]=nlohmann::json::array();
    model["materials"]=nlohmann::json::array({{{"name","ordinary-view-surface"},{"doubleSided",true},
        {"pbrMetallicRoughness",{{"baseColorFactor",{.32,.14,.08,1}}}},
        {"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","generic"},{"features",nlohmann::json::array()}}}}}}});
    for(auto& mesh:model["meshes"])for(auto& primitive:mesh["primitives"])primitive["material"]=0;
    std::ofstream(output/"scene.gltf")<<model.dump();
#if AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION
    nlohmann::json grid={{"asset",{{"version","2.0"}}},{"scene",0},{"scenes",nlohmann::json::array({{{"nodes",{0}}}})},
        {"nodes",nlohmann::json::array({{{"mesh",0}}})},{"materials",model["materials"]}};
    std::vector<float> positions;nlohmann::json accessors=nlohmann::json::array(),primitives=nlohmann::json::array();
    for(unsigned i=0;i<256;++i){const float x=(float(i%16)-7.5F)*.1F,y=(float(i/16)-7.5F)*.1F;
        for(auto v:std::array<std::array<float,3>,3>{{{x-.04F,y-.04F,0},{x+.04F,y-.04F,0},{x,y+.04F,0}}})positions.insert(positions.end(),v.begin(),v.end());
        accessors.push_back({{"bufferView",0},{"byteOffset",i*36},{"componentType",5126},{"count",3},{"type","VEC3"},
            {"min",{x-.04F,y-.04F,0.F}},{"max",{x+.04F,y+.04F,0.F}}});
        primitives.push_back({{"attributes",{{"POSITION",i}}},{"material",0}});
    }
    const auto bytes=positions.size()*4;std::ofstream binary(output/"grid.bin",std::ios::binary);binary.write(reinterpret_cast<const char*>(positions.data()),bytes);binary.close();
    grid["buffers"]=nlohmann::json::array({{{"uri","grid.bin"},{"byteLength",bytes}}});
    grid["bufferViews"]=nlohmann::json::array({{{"buffer",0},{"byteLength",bytes},{"target",34962}}});
    grid["accessors"]=accessors;grid["meshes"]=nlohmann::json::array({{{"primitives",primitives}}});std::ofstream(output/"grid.gltf")<<grid.dump();
#endif
    model["materials"][0]["extras"]["azureRenderMaterial"]["environmentLighting"]={{"schemaVersion",1},{"diffuse","global-environment"},{"reflection","global-environment"},{"globalIntensity",.5}};
    std::ofstream(output/"lighting.gltf")<<model.dump();
    RenderContext base;base.rhi=host.backend.get();base.allocator=&host.allocator;base.device=host.device;
    base.physicalDevice=host.physical;base.graphicsQueue=host.queue;base.commandPool=host.pool;base.graphicsQueueFamily=host.family;
    base.descriptorLimits=host.properties.limits;base.shadowMapSize=32;base.shaderDirectory=argv[1];base.cullingEnabled=false;
#if AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION
    base.computeShaders=base.gpuCulling=true;
#endif
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    nlohmann::json receipt;receipt["checks"]=nlohmann::json::array();receipt["device"]=host.properties.deviceName;receipt["quadCapable"]=capable;
    std::map<unsigned,std::vector<std::uint8_t>> reference;
    std::uint64_t serial=0;CharacterSceneRenderer* renderer=nullptr;
    for(bool requested:{false,true}){
        base.fragmentPrimitiveIdentity=base.fragmentQuadControl=capable&&requested;
        RenderViewService service(base,[&](const std::string&){auto value=std::make_unique<CharacterSceneRenderer>();renderer=value.get();return value;},[&]{vkDeviceWaitIdle(host.device);});
        for(unsigned lighting=0;lighting<(AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION?3U:2U);++lighting){
        RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={48,48};
        descriptor.settings.sceneLightingEnabled=true;
        descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;
        descriptor.settings.innerOutlineEnabled=descriptor.settings.silhouetteOutlineEnabled=false;
        descriptor.settings.characterPresentation={false,false};descriptor.settings.bloom.enabled=false;
        descriptor.scene.resources.push_back({"surface",(output/(lighting==2?"grid.gltf":lighting==1?"lighting.gltf":"scene.gltf")).string()});
        if(lighting==2)descriptor.settings.screenSpaceReflection.enabled=true;
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
            const unsigned key=lighting*8+phase;
            if(!requested)reference[key]=pixels;
            else require(reference.at(key)==pixels,"Cache and complete-shading RGB bytes must match");
            std::ostringstream metadata;metadata<<'{';renderer->appendCaptureManifestFields(metadata);metadata<<"\"end\":true}";
            auto state=nlohmann::json::parse(metadata.str());
            const bool expectedCache=capable&&requested&&lighting!=1;
            require(state["surfaceRadiancePrepared"].get<bool>()==expectedCache,"View must select capability and material fallback");
            require(state["surfaceRadianceBytes"].get<std::uint64_t>()==(expectedCache?(phase==7?64U*40U:48U*48U)*64U*2U:0U),"View cache accounting must follow resize");
#if AZURE_TEST_SURFACE_RADIANCE_FRAME_SELECTION
            if(lighting==2&&phase==3&&capable&&requested){
                require(state["surfaceRadianceSelection"]["prepared"].get<bool>(),"Grid view must prepare real frame selection");
                require(state["surfaceRadianceSelection"]["fastFrames"].get<std::uint64_t>()>0,"Grid view must execute the lightweight consumer");
            }
#endif
            receipt["checks"].push_back({{"requested",requested},{"phase",phase},{"lighting",lighting},{"isolation",isolations[phase]},{"bytes",pixels.size()},{"metadata",state}});
        }
        service.release(view);service.complete(serial);service.clear();service.complete(serial);
        }
    }
    require(host.errors.load()==0,"View probe must have zero Vulkan synchronization and validation errors");
    require(!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"View probe must retire all allocations");
    receipt["validationErrors"]=host.errors.load();receipt["zeroAllocator"]=true;receipt["status"]="passed";
    std::ofstream(output/(std::string(host.properties.deviceName).find("Intel")!=std::string::npos?"intel-summary.json":"nvidia-summary.json"))<<receipt.dump(2);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
