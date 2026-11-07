#include "support/VulkanComputeFixture.hpp"
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
        constexpr VkMemoryPropertyFlags mapped=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const bool hostOnly=compatible(host,readback.buffer,mapped,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(hostOnly)require(host.allocator.statistics().deviceLocalPeakBytes==0,"Transfer-only readback must retain available host memory");
        source=host.allocator.createBuffer(bytes,usage|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
        require(source.mapped&&readback.mapped,"Persistent mapping must remain available");
        const bool local=compatible(host,source.buffer,mapped|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(local&&hostOnly)require(host.allocator.statistics().deviceLocalPeakBytes>=bytes,"GPU-read buffer must use available local mapped memory");
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
    }catch(...){host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(readback);throw;}
}
}
int main()try{
    for(const auto usage:{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT})exercise(usage);
    std::cout<<"PASS: seven isolated mapped usages and transfer-only readback\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
