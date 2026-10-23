#include "render/RenderViewService.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace azurerender;
void viewGpuCheck(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Expected shaders and output directory");
    test::VulkanComputeFixture host(true);RenderContext base;base.rhi=host.backend.get();base.allocator=&host.allocator;
    base.device=host.device;base.physicalDevice=host.physical;base.graphicsQueue=host.queue;base.commandPool=host.pool;base.graphicsQueueFamily=host.family;
    base.descriptorLimits=host.properties.limits;base.renderExtent=base.swapchainExtent={96,64};base.shadowMapSize=128;
    base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;
    base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.shaderDirectory=argv[1];base.cullingEnabled=false;
    const auto root=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);base.rampAtlasPath=(root/"assets_public/toon_ramp_atlas.ppm").string();
    const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
    {
        RenderViewService service(base,[](const std::string&){return std::make_unique<CharacterSceneRenderer>();},[&]{vkDeviceWaitIdle(host.device);});
        std::array<RenderViewHandle,2> views{};
        for(unsigned i=0;i<2;++i){RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={96,64};
            descriptor.settings.antiAliasing=3;descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;
            descriptor.settings.characterPresentation.platformEnabled=false;descriptor.settings.innerOutlineEnabled=false;
            descriptor.cameraPosition={2,1,3};descriptor.cameraTarget={0,0,0};
            descriptor.scene.resources.push_back({"asset",(root/(i?"assets_public/test_model.gltf":"assets_public/scene_inspector/assets/cube.gltf")).string()});
            scene::SceneNodeDesc node;node.id="object";node.resourceId="asset";descriptor.scene.nodes.push_back(node);
            views[i]=service.create(descriptor);viewGpuCheck(service.describe(views[i]).at("temporalAvailable"),"Supported views allocate independent temporal outputs");
        }
        nlohmann::json receipt;receipt["frames"]=nlohmann::json::array();SceneFrameData frame;
        for(unsigned phase=0;phase<10;++phase){base.currentFrame=phase%2;frame.deltaSeconds=1.F/60;frame.timeSeconds=phase/60.0;
            for(auto view:views)service.request(view);
            if(phase>=8)service.setCamera(views[0],{2+.04F*float(phase-7),1,3},{0,0,0});
            RenderGraph graph;service.schedule(graph,frame,base,phase+1);std::string error;viewGpuCheck(graph.compile(error),"Actual temporal view graph compiles");
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
            viewGpuCheck(service.submitted(phase+1),"Actual temporal views accept their rendered submission");service.complete(phase+1);
            nlohmann::json state=nlohmann::json::array();
            for(unsigned i=0;i<2;++i){auto description=service.describe(views[i]);
                viewGpuCheck(description.at("temporalHistorySubmission")==phase+1,"Each view's HDR history follows actual graphics submissions");
                viewGpuCheck(description.at("submittedRenderCount")==phase+1,"Each view advances exactly once per actual render");
                const auto pixels=service.readPixels(views[i]);viewGpuCheck(pixels.size()==96*64*4,"Temporal view readback keeps its public output extent");
                unsigned minimum=255,maximum=0;for(std::size_t p=0;p<pixels.size();p+=4){minimum=std::min(minimum,unsigned(pixels[p]));maximum=std::max(maximum,unsigned(pixels[p]));}
                viewGpuCheck(maximum>minimum,"Temporal output retains nonuniform scene detail");
                std::ofstream image(output/("view-"+std::to_string(i)+"-"+std::to_string(phase)+".rgba"),std::ios::binary);image.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
                state.push_back(std::move(description));
            }
            receipt["frames"].push_back(std::move(state));
        }
        service.request(views[0]);base.currentFrame=0;RenderGraph abandoned;service.schedule(abandoned,frame,base,11);viewGpuCheck(service.cancel(11),"Unsubmitted view work cancels");
        viewGpuCheck(service.describe(views[0]).at("temporalHistorySubmission")==10,"Abandoned recording preserves submitted HDR history");
        abandoned=RenderGraph{};
        service.resize(views[0],{64,64});RenderGraph resized;service.schedule(resized,frame,base,12);std::string error;viewGpuCheck(resized.compile(error),"Resized temporal graph compiles");
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){resized.execute(&commands);});
        viewGpuCheck(service.submitted(12),"Resized temporal view publishes its first history");service.complete(12);
        viewGpuCheck(service.describe(views[0]).at("temporalHistorySubmission")==12,"Recreated view history uses the new submission");
        receipt["resized"]=service.describe(views[0]);std::ofstream(output/"result.json")<<receipt.dump(2)<<'\n';
        service.clear();service.complete(12);
    }
    viewGpuCheck(!host.errors.load(),"Temporal views report no Vulkan validation errors");
    viewGpuCheck(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"Temporal views release all GPU allocations");
    std::cout<<"Temporal view GPU: two assets, independent eight-phase histories, camera movement, cancellation and resize passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
