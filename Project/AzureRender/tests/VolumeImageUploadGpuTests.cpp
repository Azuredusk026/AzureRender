#include "support/VulkanComputeFixture.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>

using namespace azurerender;
namespace {
void verify(test::VulkanComputeFixture& host,bool volume){
    auto& backend=*host.backend;
    rhi::GpuImage image{};rhi::GpuBuffer source{},result{};VkImageView view{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(view)backend.destroyImageView(view);
        host.allocator.destroyImage(image);host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(result);
    });
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType=volume?VK_IMAGE_TYPE_3D:VK_IMAGE_TYPE_2D;
    info.extent=volume?VkExtent3D{5,3,4}:VkExtent3D{4,4,1};
    info.mipLevels=3;info.arrayLayers=volume?1:6;info.format=VK_FORMAT_R32_UINT;
    info.flags=volume?0:VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;
    info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    image=host.allocator.createImage(info,false);
    if(image.depth!=info.extent.depth)throw std::runtime_error("Allocation must retain image depth");
    view=backend.createImageView(image.image,info.format,VK_IMAGE_ASPECT_COLOR_BIT,info.mipLevels,0,
        volume?VK_IMAGE_VIEW_TYPE_3D:VK_IMAGE_VIEW_TYPE_CUBE,info.arrayLayers);
    std::vector<VkBufferImageCopy> regions;VkDeviceSize bytes=0;
    for(unsigned level=0;level<info.mipLevels;++level){
        VkBufferImageCopy region{};region.bufferOffset=bytes;
        region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,info.arrayLayers};
        region.imageExtent={std::max(1U,info.extent.width>>level),std::max(1U,info.extent.height>>level),std::max(1U,info.extent.depth>>level)};
        regions.push_back(region);
        bytes+=VkDeviceSize(region.imageExtent.width)*region.imageExtent.height*region.imageExtent.depth*info.arrayLayers*4;
    }
    std::vector<std::uint32_t> expected(static_cast<std::size_t>(bytes/4));
    std::iota(expected.begin(),expected.end(),0x13450000U);
    source=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    result=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    std::memcpy(source.mapped,expected.data(),bytes);host.allocator.flush(source,0,bytes);
    backend.transitionImageLayout(image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,info.mipLevels);
    backend.copyBufferToImage(source,image,info.extent.width,info.extent.height,info.mipLevels);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer command){
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=image.image;
        barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,info.mipLevels,0,info.arrayLayers};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        vkCmdCopyImageToBuffer(command,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,result.buffer,
            static_cast<unsigned>(regions.size()),regions.data());
        commands.bufferBarrier({result.buffer,0,bytes,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(result,0,bytes);
    if(std::memcmp(result.mapped,expected.data(),bytes)!=0)
        throw std::runtime_error(volume?"Three-dimensional mip/slice upload mismatch":"Cube-array mip upload mismatch");
}
}
int main()try{
    test::VulkanComputeFixture host;
    verify(host,true);verify(host,false);
    if(host.errors.load())throw std::runtime_error("Volume upload validation errors");
    if(host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)
        throw std::runtime_error("Volume upload resource leak");
    std::cout<<"PASS: three-dimensional slices/mips and two-dimensional cube arrays\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
