#include "support/VulkanComputeFixture.hpp"
#include "rhi/NullRhi.hpp"
#include <array>
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
bool compatible(test::VulkanComputeFixture& host,VkBuffer buffer,VkMemoryPropertyFlags wanted,VkMemoryPropertyFlags excluded=0){
    VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(host.device,buffer,&requirements);
    VkPhysicalDeviceMemoryProperties properties{};vkGetPhysicalDeviceMemoryProperties(host.physical,&properties);
    for(unsigned i=0;i<properties.memoryTypeCount;++i){const auto flags=properties.memoryTypes[i].propertyFlags;
        if((requirements.memoryTypeBits&(1U<<i))&&(flags&wanted)==wanted&&!(flags&excluded))return true;}
    return false;
}
void exercise(VkBufferUsageFlags usage){
    test::VulkanComputeFixture host;
    require(host.allocator.statistics().deviceLocalPeakBytes==0,"Fresh allocator must isolate historical device peaks");
    constexpr VkDeviceSize bytes=4096;
    auto readback=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    rhi::GpuBuffer source{};
    try{
        VkPhysicalDeviceMemoryProperties actual{};vkGetPhysicalDeviceMemoryProperties(host.physical,&actual);
        require(readback.memoryTypeIndex<actual.memoryTypeCount,"Allocated buffer must expose its actual selected memory type");
        require(readback.memoryProperties==actual.memoryTypes[readback.memoryTypeIndex].propertyFlags,"Reported properties must describe the selected type");
        require(readback.usage==VK_BUFFER_USAGE_TRANSFER_DST_BIT,"Reported buffer usage must preserve the creation request");
        constexpr VkMemoryPropertyFlags mapped=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const bool hostOnly=compatible(host,readback.buffer,mapped,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(hostOnly)require(host.allocator.statistics().deviceLocalPeakBytes==0,"Transfer-only readback must retain available host memory");
        source=host.allocator.createBuffer(bytes,usage|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
        require(source.memoryTypeIndex<actual.memoryTypeCount,"GPU access buffer must expose a real type index");
        require(source.memoryProperties==actual.memoryTypes[source.memoryTypeIndex].propertyFlags,"GPU access properties must match the actual selected type");
        VkMemoryRequirements selected{};vkGetBufferMemoryRequirements(host.device,source.buffer,&selected);
        require((selected.memoryTypeBits&(1U<<source.memoryTypeIndex))!=0,"Selected type must be permitted by this buffer");
        const auto& statistics=host.allocator.statistics();
        require(statistics.bufferMemoryPropertySamples==2,"Every real buffer creation must be observed once");
        require(statistics.liveDeviceLocalBufferBytes+statistics.liveNonDeviceLocalBufferBytes==statistics.liveBufferBytes,"Known types must account for all live allocation bytes");
        require(statistics.liveMappedDeviceBufferBytes>=bytes,"Mapped GPU usage must enter the device access subset");
        require(statistics.liveMappedNonLocalDeviceBufferBytes==((source.memoryProperties&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)?0:statistics.liveMappedDeviceBufferBytes),"Nonlocal mapped subset must follow the selected flags");
        require(source.mapped&&readback.mapped,"Persistent mapping must remain available");
        const bool local=compatible(host,source.buffer,mapped|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(local&&hostOnly)require((source.memoryProperties&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)!=0,"GPU-read buffer must select available local mapped memory");
        std::array<std::uint32_t,bytes/4> values{};for(unsigned i=0;i<values.size();++i)values[i]=(i*2654435761U)^usage;
        std::memcpy(source.mapped,values.data(),bytes);host.allocator.flush(source,0,bytes);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
            commands.bufferBarrier({source.buffer,0,bytes,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
            VkBufferCopy copy{0,0,bytes};vkCmdCopyBuffer(native,source.buffer,readback.buffer,1,&copy);
            commands.bufferBarrier({readback.buffer,0,bytes,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(readback,0,bytes);require(std::memcmp(readback.mapped,values.data(),bytes)==0,"GPU upload and readback bytes differ");
        std::cout<<host.properties.deviceName<<": usage="<<usage<<" localMapping="<<local<<" hostReadback="<<hostOnly<<" devicePeak="<<host.allocator.statistics().deviceLocalPeakBytes<<'\n';
        host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(readback);
        require(host.errors==0&&!host.allocator.statistics().liveBuffers,"Validation or buffer retirement failed");
        require(!statistics.liveDeviceLocalBufferBytes&&!statistics.liveNonDeviceLocalBufferBytes&&!statistics.liveMappedDeviceBufferBytes&&!statistics.liveMappedNonLocalDeviceBufferBytes,"Property accounting must retire with its allocations");
        require(source.memoryTypeIndex==UINT32_MAX&&source.usage==0,"Released descriptors must reset their diagnostics");
    }catch(...){host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(readback);throw;}
}
}
int main()try{
    {rhi::NullGpuAllocator backend;auto buffer=backend.createBuffer(64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
     require(buffer.usage==VK_BUFFER_USAGE_STORAGE_BUFFER_BIT&&buffer.memoryTypeIndex==UINT32_MAX&&!buffer.memoryProperties,"Host test backend must declare requested usage and unknown device properties");
     require(!backend.statistics().bufferMemoryPropertySamples,"Unknown memory types must remain outside real observations");backend.destroyBuffer(buffer);}
    for(const auto usage:{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT})exercise(usage);
    std::cout<<"PASS: seven isolated mapped usages and transfer-only readback\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
