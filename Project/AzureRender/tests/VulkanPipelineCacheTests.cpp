#include "rhi/VulkanPipelineCache.hpp"
#include "resources/BinaryFile.hpp"
#include "resources/ResourceLocator.hpp"
#include "render/RenderViewService.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<unsigned char> render(test::VulkanComputeFixture& host,rhi::IRhi& backend,const std::filesystem::path& shaders){
    RenderContext base;base.rhi=&backend;base.allocator=&host.allocator;base.device=host.device;base.physicalDevice=host.physical;
    base.graphicsQueue=host.queue;base.commandPool=host.pool;base.graphicsQueueFamily=host.family;base.descriptorLimits=host.properties.limits;
    base.renderExtent=base.swapchainExtent={96,64};base.shadowMapSize=128;base.sceneColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
    base.depthFormat=base.shadowFormat=VK_FORMAT_D32_SFLOAT;base.normalFormat=VK_FORMAT_R8G8B8A8_UNORM;base.shaderDirectory=shaders.u8string();base.cullingEnabled=false;
    ResourceLocator locator;base.rampAtlasPath=locator.rampAtlas().u8string();
    RenderViewService service(base,[](const std::string&){return std::make_unique<CharacterSceneRenderer>();},[&]{vkDeviceWaitIdle(host.device);});
    RenderViewDescriptor descriptor;descriptor.rendererId="character";descriptor.extent={96,64};descriptor.settings.antiAliasing=0;
    descriptor.settings.faceSdfFeature=descriptor.settings.showcasePlatformFeature=false;descriptor.settings.characterPresentation.platformEnabled=false;descriptor.settings.innerOutlineEnabled=false;
    descriptor.cameraPosition={2,1,3};descriptor.cameraTarget={0,0,0};descriptor.settings.cameraView=CameraViewDescriptor{};
    descriptor.settings.cameraView->position=descriptor.cameraPosition;descriptor.settings.cameraView->target=descriptor.cameraTarget;
    descriptor.scene.resources.push_back({"asset",locator.publicAsset("scene_inspector/assets/cube.gltf").u8string()});scene::SceneNodeDesc node;node.id="object";node.resourceId="asset";descriptor.scene.nodes.push_back(node);
    const auto handle=service.create(descriptor);service.request(handle);SceneFrameData frame;RenderGraph graph;service.schedule(graph,frame,base,1);
    std::string error;require(graph.compile(error),error.c_str());host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
    require(service.submitted(1),"Cached graphics view must receive a submission");service.complete(1);auto pixels=service.readPixels(handle);
    service.clear();service.complete(1);require(pixels.size()==96*64*4,"Cached graphics readback must be complete");return pixels;
}
rhi::VulkanRhi::CacheUsage create(test::VulkanComputeFixture& host,rhi::VulkanPipelineCache& cache,const std::vector<char>& code){
    rhi::VulkanRhi backend(host.device,host.physical,host.queue,host.pool,host.allocator,false,nullptr,false,false,cache.handle());
    const auto set=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    const auto layout=backend.createPipelineLayout(set,nullptr);const auto shader=backend.createShaderModule(code);
    const auto pipeline=backend.createComputePipeline({shader,layout});
    backend.destroyPipeline(pipeline);backend.destroyShaderModule(shader);backend.destroyPipelineLayout(layout);backend.destroyDescriptorSetLayout(set);
    return backend.pipelineCacheUsage();
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Expected shader and dedicated cache directory");
    ResourceLocator locations;
    if(locations.installationRoot().empty())
        require(locations.generatedCacheDirectory("one/project.azureproject")==locations.generatedCacheDirectory("two/project.azureproject"),"Development projects must share the bounded cache pool");
    test::VulkanComputeFixture host(true);const auto code=readBinaryFile(argv[1]);const std::filesystem::path root=argv[2];
    std::filesystem::path cacheFile;std::uint64_t saved=0,loaded=0,hits=0;
    {
        rhi::VulkanPipelineCache cache(host.device,host.physical,root);require(cache.handle()!=VK_NULL_HANDLE,"Device cache must initialize");
        require(create(host,cache,code).requests==1,"RHI must record a cached creation request");
        require(cache.flush(),"Device cache must persist");saved=cache.statistics().writtenBytes;cacheFile=cache.file();
    }
    {
        rhi::VulkanPipelineCache cache(host.device,host.physical,root);loaded=cache.statistics().loadedBytes;
        require(loaded==saved&&loaded>=32,"A new cache owner must load the persisted driver payload");
        const auto usage=create(host,cache,code);require(usage.requests==1,"Reloaded cache must reach pipeline creation");hits=usage.hits;
        const auto control=render(host,*host.backend,std::filesystem::path(argv[1]).parent_path());
        rhi::VulkanRhi cached(host.device,host.physical,host.queue,host.pool,host.allocator,false,nullptr,false,false,cache.handle());
        require(render(host,cached,std::filesystem::path(argv[1]).parent_path())==control,"Cached graphics must preserve independently rendered pixels");
        require(cached.pipelineCacheUsage().requests>0,"Graphics requests must use the cache-enabled RHI");
        require(cache.flush(),"Reloaded cache must remain writable");
    }
    {
        std::fstream file(cacheFile,std::ios::binary|std::ios::in|std::ios::out);file.seekg(-1,std::ios::end);char byte=0;file.read(&byte,1);byte^=1;file.seekp(-1,std::ios::end);file.write(&byte,1);require(bool(file),"Cache corruption control failed");
    }
    {
        rhi::VulkanPipelineCache cache(host.device,host.physical,root);
        require(cache.statistics().loadedBytes==0&&cache.handle()!=VK_NULL_HANDLE,"Damaged disk data must fall back to an empty device cache");
        create(host,cache,code);require(cache.flush(),"Fallback cache must recover persistent state");
    }
    require(!host.errors.load(),"Cached pipeline creation must keep Vulkan validation clean");
    require(!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Cache checks must release all allocations");
    std::cout<<"Device="<<host.properties.deviceName<<" saved="<<saved<<" loaded="<<loaded<<" applicationHits="<<hits<<" graphicsPixelsEqual=true\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
