#include "support/VulkanComputeFixture.hpp"
#include "render/SampledTextureCache.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>

namespace {
PFN_vkCmdPipelineBarrier driverBarrier=nullptr;
bool observe=false;
std::vector<VkPipelineStageFlags> sampledDestinations;
}

// Observe the actual production RHI barriers, then forward them unchanged to
// the driver. This checks the stage contract as well as the sampled pixels.
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer command,
    VkPipelineStageFlags source,VkPipelineStageFlags destination,VkDependencyFlags flags,
    std::uint32_t memoryCount,const VkMemoryBarrier* memory,
    std::uint32_t bufferCount,const VkBufferMemoryBarrier* buffers,
    std::uint32_t imageCount,const VkImageMemoryBarrier* images) {
    if(observe)for(std::uint32_t i=0;i<imageCount;++i)
        if(images[i].newLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            &&(images[i].dstAccessMask&VK_ACCESS_SHADER_READ_BIT))sampledDestinations.push_back(destination);
    driverBarrier(command,source,destination,flags,memoryCount,memory,bufferCount,buffers,imageCount,images);
}

int main(int argc,char** argv)try {
    using namespace azurerender;
    if(argc!=2)throw std::invalid_argument("Expected sampled upload compute shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    driverBarrier=reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetDeviceProcAddr(host.device,"vkCmdPipelineBarrier"));
    if(!driverBarrier)throw std::runtime_error("Driver pipeline barrier is unavailable");
    for(unsigned policy:{1U,0U,3U}) {
        SampledTextureCache cache(host.allocator,backend);
        SampledTextureUpload upload;upload.width=upload.height=4;upload.mipLevels=policy;
        upload.sampler.maxLod=2;upload.sampler.filter=VK_FILTER_LINEAR;
        std::vector<std::uint8_t> pixels;
        for(unsigned i=0;i<16;++i)pixels.insert(pixels.end(),{128,64,32,255});
        sampledDestinations.clear();observe=true;
        const auto image=cache.acquire(pixels,upload);observe=false;
        if(sampledDestinations.empty())throw std::runtime_error("Sampled uploads must record a shader-read transition");
        for(auto stages:sampledDestinations) {
            const bool all=(stages&VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)!=0;
            const bool vertex=all||(stages&VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT)||(stages&VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
            const bool compute=all||(stages&VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            if(!vertex||!compute)throw std::runtime_error("Sampled upload visibility excludes vertex or compute consumers");
        }
        auto output=host.allocator.createBuffer(48,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
        auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*) {
            if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
            if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
            if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);
        });
        std::memset(output.mapped,0xcd,48);host.allocator.flush(output,0,48);
        setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc descriptors;descriptors.maxSets=1;
        descriptors.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
        pool=backend.createDescriptorPool(descriptors);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
        backend.writeDescriptorImage({set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,image.view,image.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
        layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
        pipeline=backend.createComputePipeline({shader,layout});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer command) {
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(1,1,1);
            VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};memory.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;memory.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&memory,0,nullptr,0,nullptr);
        });
        host.allocator.invalidate(output,0,48);const auto* actual=static_cast<const float*>(output.mapped);
        const std::array<float,4> expected{128/255.F,64/255.F,32/255.F,1};
        for(unsigned i=0;i<12;++i)if(!std::isfinite(actual[i])||std::abs(actual[i]-expected[i%4])>1e-6F)
            throw std::runtime_error("Uploaded base and mip samples must preserve their independent RGBA oracle");
        std::cout<<"Sampled upload policy "<<policy<<": "<<sampledDestinations.size()<<" transitions, three valid mip samples\n";
    }
    if(host.errors.load())throw std::runtime_error("Vulkan synchronization validation errors");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
