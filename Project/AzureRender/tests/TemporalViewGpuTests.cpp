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
        std::array<RenderViewDescriptor,2> descriptors;
        std::array<std::vector<unsigned char>,2> firstFrame,staticFrame;
        double staticCentroid=0;
        const auto centroid=[](const std::vector<unsigned char>& pixels){double sum=0,count=0;
            for(unsigned i=0;i<96*64;++i)if(std::abs(int(pixels[i*4])-int(pixels[0]))+std::abs(int(pixels[i*4+1])-int(pixels[1]))+std::abs(int(pixels[i*4+2])-int(pixels[2]))>24){sum+=i%96;++count;}
            return count?sum/count:0;
        };
        for(unsigned i=0;i<2;++i){RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={96,64};
            descriptor.settings.antiAliasing=3;descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;
            descriptor.settings.characterPresentation.platformEnabled=false;descriptor.settings.innerOutlineEnabled=false;
            descriptor.cameraPosition={2,1,3};descriptor.cameraTarget={0,0,0};
            descriptor.settings.cameraView=CameraViewDescriptor{};descriptor.settings.cameraView->position=descriptor.cameraPosition;
            descriptor.settings.cameraView->target=descriptor.cameraTarget;
            descriptor.scene.resources.push_back({"asset",(root/(i?"assets_public/test_model.gltf":"assets_public/scene_inspector/assets/cube.gltf")).string()});
            scene::SceneNodeDesc node;node.id="object";node.resourceId="asset";descriptor.scene.nodes.push_back(node);
            descriptors[i]=descriptor;views[i]=service.create(descriptor);viewGpuCheck(service.describe(views[i]).at("temporalAvailable"),"Supported views allocate independent temporal outputs");
        }
        nlohmann::json receipt;receipt["frames"]=nlohmann::json::array();SceneFrameData frame;
        for(unsigned phase=0;phase<10;++phase){base.currentFrame=phase%2;frame.deltaSeconds=phase>=8?1.F/60:0;frame.timeSeconds=phase/60.0;
            for(auto view:views)service.request(view);
            if(phase>=8)service.setCamera(views[0],{2+.04F*float(phase-7),1,3},{.3F*float(phase-7),0,0});
            RenderGraph graph;service.schedule(graph,frame,base,phase+1);std::string error;viewGpuCheck(graph.compile(error),"Actual temporal view graph compiles");
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
            viewGpuCheck(service.submitted(phase+1),"Actual temporal views accept their rendered submission");service.complete(phase+1);
            nlohmann::json state=nlohmann::json::array();
            for(unsigned i=0;i<2;++i){auto description=service.describe(views[i]);
                viewGpuCheck(description.at("temporalHistorySubmission")==phase+1,"Each view's HDR history follows actual graphics submissions");
                viewGpuCheck(description.at("submittedRenderCount")==phase+1,"Each view advances exactly once per actual render");
                const auto pixels=service.readPixels(views[i]);viewGpuCheck(pixels.size()==96*64*4,"Temporal view readback keeps its public output extent");
                if(phase==0)firstFrame[i]=pixels;
                if(phase==7){staticFrame[i]=pixels;if(i==0)staticCentroid=centroid(pixels);}
                if(phase==9&&i==0)viewGpuCheck(std::abs(centroid(pixels)-staticCentroid)>2,"Explicit lens settings consume the view's current camera pose in actual pixels");
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
        receipt["resized"]=service.describe(views[0]);
        receipt["quality"]=nlohmann::json::array();
        frame.deltaSeconds=0;
        for(unsigned asset=0;asset<2;++asset){std::array<std::vector<unsigned char>,2> references;
            for(unsigned kind=0;kind<2;++kind){auto descriptor=descriptors[asset];descriptor.settings.antiAliasing=0;
                const unsigned scale=kind?4:1;descriptor.extent={96*scale,64*scale};descriptor.settings.outline.silhouetteWidthPixels*=float(scale);
                const auto reference=service.create(descriptor);service.request(reference);base.currentFrame=0;
                RenderGraph graph;const auto serial=13+asset*2+kind;service.schedule(graph,frame,base,serial);std::string error;
                viewGpuCheck(graph.compile(error),"Independent native and high-sample reference graphs compile");
                host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
                viewGpuCheck(service.submitted(serial),"Independent reference receives its actual submission");service.complete(serial);
                references[kind]=service.readPixels(reference);service.release(reference);service.complete(serial);
                std::ofstream image(output/(std::string(kind?"reference-high-":"reference-off-")+std::to_string(asset)+".rgba"),std::ios::binary);
                image.write(reinterpret_cast<const char*>(references[kind].data()),static_cast<std::streamsize>(references[kind].size()));
            }
            viewGpuCheck(references[0]==firstFrame[asset],"Zero-history native resolve preserves independently rendered mode-zero pixels");
            std::vector<std::array<double,3>> high(96*64);
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<96;++x)for(unsigned channel=0;channel<3;++channel)
                for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx)high[y*96+x][channel]+=references[1][((y*4+sy)*384+x*4+sx)*4+channel]/16.0;
            double offError=0,temporalError=0;unsigned edges=0;double flatMaximum=0;
            for(unsigned y=1;y<63;++y)for(unsigned x=1;x<95;++x){const auto pixel=y*96+x;double gradient=0;
                // A flat region requires every sample in the reference 3x3
                // neighbourhood to equal its centre. Symmetric edge samples
                // alone misclassify isolated subpixel detail as flat.
                for(unsigned channel=0;channel<3;++channel)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
                    gradient=std::max(gradient,std::abs(high[pixel][channel]-high[static_cast<std::size_t>(static_cast<int>(pixel)+dy*96+dx)][channel]));
                for(unsigned channel=0;channel<3;++channel){const auto off=std::abs(references[0][pixel*4+channel]-high[pixel][channel]);
                    const auto temporal=std::abs(staticFrame[asset][pixel*4+channel]-high[pixel][channel]);
                    if(gradient>4){offError+=off;temporalError+=temporal;}else if(gradient==0)flatMaximum=std::max(flatMaximum,temporal);}
                if(gradient>4)++edges;
            }
            viewGpuCheck(edges>0&&offError>0,"Independent high-resolution gradients identify nonempty aliasing regions");
            const auto ratio=temporalError/offError;receipt["quality"].push_back({{"asset",asset},{"edgePixels",edges},{"offError",offError/(edges*3)},{"temporalError",temporalError/(edges*3)},{"errorRatio",ratio},{"flatMaximumByteError",flatMaximum}});
            std::ofstream(output/"result.json")<<receipt.dump(2)<<'\n';
            viewGpuCheck(ratio<=.8,"Temporal static edge error must stay within 80 percent of independently rendered mode zero");
            viewGpuCheck(flatMaximum<=1,"Flat regions must preserve the independent reference within one output byte");
        }
        std::ofstream(output/"result.json")<<receipt.dump(2)<<'\n';
        service.clear();service.complete(16);
    }
    viewGpuCheck(!host.errors.load(),"Temporal views report no Vulkan validation errors");
    viewGpuCheck(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"Temporal views release all GPU allocations");
    std::cout<<"Temporal view GPU: two assets, independent histories, explicit camera movement, resize and independent 4x reference gates passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
