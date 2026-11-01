#include "render/RenderViewService.hpp"
#include "render/ColorGradingLut.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Expected shaders and output directory");
    test::VulkanComputeFixture host(true,true);
    const auto root=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR),output=std::filesystem::path(argv[2]);
    std::filesystem::create_directories(output);
    nlohmann::json model;std::ifstream(root/"assets_public/scene_inspector/assets/cube.gltf")>>model;
    model["images"]=nlohmann::json::array();model["textures"]=nlohmann::json::array();
    model["materials"]=nlohmann::json::array({{{"name","display-view-surface"},{"doubleSided",true},
        {"pbrMetallicRoughness",{{"baseColorFactor",{.32,.14,.08,1}}}},
        {"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","generic"},{"features",{"unlit-surface"}},
            {"radianceScale",.25},{"surface",{{"schemaVersion",1},{"lighting","unlit"}}}}}}}}});
    for(auto& mesh:model["meshes"])for(auto& primitive:mesh["primitives"])primitive["material"]=0;
    std::ofstream(output/"scene.gltf")<<model.dump();
    model["materials"][0]["extras"]["azureRenderMaterial"]["displayTransform"]="linear";
    std::ofstream(output/"linear.gltf")<<model.dump();
    auto lut=makeIdentityColorGradingLut();lut.source={"azure.display-view-test",1,"constant-linear-rgb",ColorGradingEvidence::Authored};
    const std::array<float,3> constant{.03F,.05F,.08F};
    for(std::size_t i=0;i<lut.image.rgba32.size();i+=4)for(unsigned c=0;c<3;++c)lut.image.rgba32[i+c]=constant[c];
    saveColorGradingLut(output/"constant.azlut",lut);
    RenderContext base;base.rhi=host.backend.get();base.allocator=&host.allocator;base.device=host.device;
    base.physicalDevice=host.physical;base.graphicsQueue=host.queue;base.commandPool=host.pool;base.graphicsQueueFamily=host.family;
    base.descriptorLimits=host.properties.limits;base.shadowMapSize=32;base.shaderDirectory=argv[1];base.cullingEnabled=false;
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    nlohmann::json receipt;receipt["checks"]=nlohmann::json::array();std::uint64_t serial=0;
    {
        RenderViewService service(base,[](const std::string&){return std::make_unique<CharacterSceneRenderer>();},[&]{vkDeviceWaitIdle(host.device);});
        for(unsigned quality:{0U,3U,4U}){
            std::array<RenderViewHandle,3> views{};VkExtent2D extent{48,48};
            for(unsigned kind=0;kind<3;++kind){
                RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent=extent;
                descriptor.settings.antiAliasing=quality;descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;
                descriptor.settings.stylizedLightingEnabled=false;descriptor.settings.innerOutlineEnabled=descriptor.settings.silhouetteOutlineEnabled=false;
                descriptor.settings.characterPresentation={false,false};descriptor.settings.grade.toneMappingEnabled=false;
                descriptor.settings.bloom.enabled=false;
                descriptor.scene.resources.push_back({"surface",(output/(kind==1?"linear.gltf":"scene.gltf")).string()});
                scene::SceneNodeDesc node;node.id="surface";node.resourceId="surface";descriptor.scene.nodes.push_back(node);
                if(kind){descriptor.settings.grade.lut.asset="constant.azlut";descriptor.settings.grade.lut.resolvedAsset=output/"constant.azlut";}
                views[kind]=service.create(descriptor);
            }
            for(unsigned phase=0;phase<10;++phase){
                if(phase==8){extent={64,40};for(auto view:views)service.resize(view,extent);}
                ++serial;base.currentFrame=serial%2;SceneFrameData frame;frame.timeSeconds=phase/60.0;
                for(auto view:views)service.request(view);
                RenderGraph graph;service.schedule(graph,frame,base,serial);std::string error;require(graph.compile(error),error.c_str());
                host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
                require(service.submitted(serial),"Display views commit actual graphics submissions");service.complete(serial);
                if(phase!=7&&phase!=9)continue;
                std::array<std::vector<unsigned char>,3> pixels;
                for(unsigned kind=0;kind<3;++kind){pixels[kind]=service.readPixels(views[kind]);
                    require(pixels[kind].size()==extent.width*extent.height*4,"Display readback follows resize");
                    const auto description=service.describe(views[kind]);
                    require(description.at("displayMaskFormat")==VK_FORMAT_R8_UNORM&&description.at("displayMaskBytes")==extent.width*extent.height,
                        "Editor views expose their owned R8 display attachment");
                    if(quality==4)require(description.at("rasterizationSamples")==4,"Display coverage uses actual four-sample shading");
                }
                unsigned interiors=0,background=0,maximum=0;
                const auto foreground=[&](unsigned x,unsigned y){const auto p=(y*extent.width+x)*4;
                    unsigned delta=0;for(unsigned c=0;c<3;++c)delta+=std::abs(int(pixels[0][p+c])-int(pixels[0][c]));return delta>4;};
                for(unsigned y=1;y+1<extent.height;++y)for(unsigned x=1;x+1<extent.width;++x){
                    const auto p=(y*extent.width+x)*4;bool inside=true;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)inside&=foreground(x+dx,y+dy);
                    for(unsigned c=0;c<3;++c){
                        require(std::abs(int(pixels[2][p+c])-int(std::lround(constant[c]*255)))<=1,"Scene display consumes the authored LUT");
                        if(inside)maximum=std::max(maximum,unsigned(std::abs(int(pixels[0][p+c])-int(pixels[1][p+c]))));
                        if(!foreground(x,y))require(pixels[1][p+c]==pixels[2][p+c],"Linear material retains scene display on clear background");
                    }
                    interiors+=inside;background+=!foreground(x,y);
                }
                require(interiors>16&&background>16,"View comparison has independent interior and background controls");
                require(maximum<=1,"Linear view interiors retain the independent ungraded reference within one byte");
                receipt["checks"].push_back({{"quality",quality},{"phase",phase},{"interiorPixels",interiors},{"backgroundPixels",background},{"maximumByteError",maximum}});
            }
            for(auto view:views)service.release(view);service.complete(serial);
        }
        service.clear();service.complete(serial);
    }
    require(host.errors.load()==0,"Display views report no Vulkan validation errors");
    require(!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"Display views retire all GPU allocations");
    receipt["status"]="passed";std::ofstream(output/"summary.json")<<receipt.dump(2)<<'\n';
    std::cout<<"Material display views: real character renderer, LUT controls, temporal, multisampling, resize, recreation and retirement passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
