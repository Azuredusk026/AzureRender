#include "render/RenderViewService.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <iostream>
#include <sstream>

using namespace azurerender;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void save(const std::filesystem::path& path,const std::vector<unsigned char>& bytes){
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
}
}

int main(int argc,char** argv)try{
    check(argc==3||argc==4,"Expected shader and output directories, with optional frozen reference shaders");
    const bool legacyOnly=argc==4&&std::string(argv[3])=="--legacy-only";
    const bool computedView=argc==4&&std::string(argv[3])=="--computed-viewquad";
    const std::filesystem::path root=AZURERENDER_TEST_SOURCE_DIR,output=argv[2];
    std::filesystem::create_directories(output);
    nlohmann::json model;std::ifstream(root/"assets_public/scene_inspector/assets/cube.gltf")>>model;
    model["images"]=nlohmann::json::array();model["textures"]=nlohmann::json::array();
    model["materials"]=nlohmann::json::array({{{"name","deterministic-surface"},{"doubleSided",true},
        {"pbrMetallicRoughness",{{"baseColorFactor",{.32,.14,.08,1}}}},
        {"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","generic"},{"features",nlohmann::json::array()}}}}}}});
    for(auto& mesh:model["meshes"])for(auto& primitive:mesh["primitives"])primitive["material"]=0;
    std::ofstream(output/"scene.gltf")<<model.dump();
    model["materials"][0]["extras"]["azureRenderMaterial"]["environmentLighting"]=
        {{"schemaVersion",1},{"diffuse","global-environment"},{"reflection","global-environment"},{"globalIntensity",.5}};
    std::ofstream(output/"lighting.gltf")<<model.dump();
    model["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"mirrored-coarse-uv"};
    std::ofstream(output/"uv.gltf")<<model.dump();

    std::unique_ptr<test::VulkanComputeFixture> owner;bool computedCapable=false;
    if(computedView){
        try{owner=std::make_unique<test::VulkanComputeFixture>(true,false,false,false,true,false,true,false,true);computedCapable=true;}
        catch(const test::VulkanFeatureUnavailable&){owner=std::make_unique<test::VulkanComputeFixture>(true);}
    }else owner=std::make_unique<test::VulkanComputeFixture>(true);
    auto& host=*owner;
    if(computedView&&host.properties.vendorID==0x10deU)check(computedCapable,"Qualified NVIDIA fixture must enable real quad and indexing features");
    RenderContext base;base.rhi=host.backend.get();base.allocator=&host.allocator;
    base.device=host.device;base.physicalDevice=host.physical;base.graphicsQueue=host.queue;
    base.commandPool=host.pool;base.graphicsQueueFamily=host.family;base.descriptorLimits=host.properties.limits;
    base.shadowMapSize=32;base.cullingEnabled=false;base.computeShaders=true;base.rgba16fStorageImage=true;
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.shaderDirectory=argv[1];
    base.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    base.bindlessTextures=computedView&&computedCapable;
    std::map<unsigned,std::vector<unsigned char>> references;
    std::size_t pixelsCompared=0,eligibleChecks=0,fallbackChecks=0;CharacterSceneRenderer* renderer=nullptr;
    for(unsigned repeat=0;repeat<3;++repeat){
        base.fragmentQuadControl=computedView&&computedCapable&&repeat>0;
        base.shaderDirectory=repeat==0&&argc==4&&!legacyOnly&&!computedView?argv[3]:argv[1];
        RenderViewService service(base,[&](const std::string&){auto value=std::make_unique<CharacterSceneRenderer>();renderer=value.get();return value;},
            [&]{vkDeviceWaitIdle(host.device);});
        std::uint64_t serial=0;
        for(unsigned lighting=0;lighting<(computedView?3U:2U);++lighting)for(unsigned setting=0;setting<3;++setting){
        if(legacyOnly&&(lighting!=0||setting!=1))continue;
        RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={48,48};
        descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;
        descriptor.settings.innerOutlineEnabled=descriptor.settings.silhouetteOutlineEnabled=false;
        descriptor.settings.characterPresentation={false,false};descriptor.settings.bloom.enabled=false;
        descriptor.settings.sceneLightingEnabled=setting!=1;
        descriptor.settings.screenSpaceReflection.enabled=setting!=2;
        descriptor.scene.resources.push_back({"surface",(output/(lighting==2?"uv.gltf":lighting?"lighting.gltf":"scene.gltf")).string()});
        scene::SceneNodeDesc node;node.id="surface";node.resourceId="surface";descriptor.scene.nodes.push_back(node);
        const auto view=service.create(descriptor);
        const unsigned isolations[]={0,0,0,0,1,22,0,0,0,0};
        for(unsigned phase=0;phase<10;++phase){
            if(phase==8)service.resize(view,{64,40});
            ++serial;base.currentFrame=serial%2;SceneFrameData frame;frame.timeSeconds=phase/60.0;
            frame.qaIsolationMode=isolations[phase];frame.qaEffectMode=phase==6?1:0;frame.qaEffectEnabled=phase!=6;
            service.request(view);RenderGraph graph;service.schedule(graph,frame,base,serial);
            std::string error;check(graph.compile(error),error.c_str());
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
            check(service.submitted(serial),"Actual view submission must commit");service.complete(serial);
            if(computedView){
                std::ostringstream metadata;metadata<<'{';renderer->appendCaptureManifestFields(metadata);metadata<<"\"end\":true}";
                const auto state=nlohmann::json::parse(metadata.str());
                const bool eligible=AZURE_TEST_COMPUTED_MATERIAL_VIEW&&computedCapable&&host.properties.vendorID==0x10deU&&repeat>0&&lighting!=2&&setting==0&&phase!=4&&phase!=5&&phase!=6;
                check(state.at("computedMaterialViewFrameEligible").get<bool>()==eligible,"Computed view must follow actual camera/depth/UV capability state");
                if(eligible){check(state.at("computedMaterialViewSelectionsTotal").get<std::uint64_t>()>0,"Qualified view must bind real optimized pipelines");++eligibleChecks;}
                else ++fallbackChecks;
                if(lighting==2||repeat==0||!computedCapable)check(state.at("computedMaterialViewPipelines")==0,"Reference capability/UV views must retain original pipelines");
            }
            if(phase<3)continue;
            const auto pixels=service.readPixels(view);
            check(pixels.size()==(phase>=8?64*40*4:48*48*4),"Readback must preserve the requested extent");
            const unsigned key=phase+10*(setting+3*lighting);
            if(repeat==0)references[key]=pixels;
            else{
                if(pixels!=references.at(key)){
                    std::map<unsigned,unsigned> changedRows;unsigned maximum=0;std::size_t changed=0;
                    for(std::size_t i=0;i<pixels.size();++i)if(pixels[i]!=references.at(key)[i]){
                        ++changed;++changedRows[static_cast<unsigned>(i/((phase>=8?64:48)*4))];
                        maximum=std::max(maximum,static_cast<unsigned>(std::abs(int(pixels[i])-int(references.at(key)[i]))));
                    }
                    save(output/"reference.rgba",references.at(key));save(output/"actual.rgba",pixels);
                    nlohmann::json failure={{"device",host.properties.deviceName},{"phase",phase},{"lighting",lighting},{"setting",setting},
                        {"changedBytes",changed},{"maximumByteError",maximum},{"changedRows",changedRows}};
                    std::ofstream(output/"failure.json")<<failure.dump(2)<<'\n';
                    std::cerr<<failure.dump()<<'\n';
                    throw std::runtime_error("Independent identical material views must preserve every RGBA byte");
                }
                pixelsCompared+=pixels.size()/4;
            }
        }
        service.release(view);service.complete(serial);service.clear();service.complete(serial);
        }
    }
    check(host.errors.load()==0,"Material views must have no Vulkan validation errors");
    check(!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"All view resources must retire");
    nlohmann::json result={{"device",host.properties.deviceName},{"pixelsCompared",pixelsCompared},
        {"validationErrors",host.errors.load()},{"zeroAllocator",true},{"status","passed"},
        {"computedViewMode",computedView},{"computedCapable",computedCapable},{"eligibleChecks",eligibleChecks},{"fallbackChecks",fallbackChecks}};
    std::ofstream(output/"results.json")<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
