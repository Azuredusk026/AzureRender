#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cstring>
using namespace azurerender;
namespace {PFN_vkCreateRenderPass driverCreate=nullptr;bool observe=false,sawReadOnly=false;unsigned observationErrors=0;}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device,const VkRenderPassCreateInfo* info,
    const VkAllocationCallbacks* allocator,VkRenderPass* pass){
    if(observe){
        if(info->subpassCount!=1||!info->pSubpasses[0].pDepthStencilAttachment)observationErrors|=1;
        else {
            const auto& depth=*info->pSubpasses[0].pDepthStencilAttachment;
            if(depth.layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL||info->pAttachments[depth.attachment].loadOp!=VK_ATTACHMENT_LOAD_OP_LOAD)
                observationErrors|=2;
        }
        bool storeBefore=false,storeAfter=false;
        for(unsigned i=0;i<info->dependencyCount;++i){const auto& dependency=info->pDependencies[i];
            if(dependency.dstSubpass==0&&(dependency.dstAccessMask&VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))storeBefore=true;
            if(dependency.srcSubpass==0&&(dependency.srcAccessMask&VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))storeAfter=true;
        }
        if(!storeBefore||!storeAfter)observationErrors|=4;
        sawReadOnly=true;
    }
    if(!driverCreate)return VK_ERROR_INITIALIZATION_FAILED;
    return driverCreate(device,info,allocator,pass);
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Expected read-only depth vertex and fragment shaders");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    driverCreate=reinterpret_cast<PFN_vkCreateRenderPass>(vkGetDeviceProcAddr(host.device,"vkCreateRenderPass"));
    constexpr VkExtent2D extent{32,32};std::array<rhi::GpuImage,2> images{};std::array<VkImageView,2> views{};
    std::array<rhi::GpuBuffer,2> readback{};VkRenderPass pass{};VkFramebuffer framebuffer{};VkDescriptorSetLayout set{};
    VkPipelineLayout layout{};VkPipeline pipeline{};VkShaderModule vertex{},fragment{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(layout)backend.destroyPipelineLayout(layout);if(set)backend.destroyDescriptorSetLayout(set);
        if(vertex)backend.destroyShaderModule(vertex);if(fragment)backend.destroyShaderModule(fragment);
        if(framebuffer)backend.destroyFramebuffer(framebuffer);if(pass)backend.destroyRenderPass(pass);
        for(auto view:views)if(view)backend.destroyImageView(view);for(auto& image:images)host.allocator.destroyImage(image);
        for(auto& buffer:readback)host.allocator.destroyBuffer(buffer);
    });
    images[0]=host.allocator.createImage2D(32,32,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    images[1]=host.allocator.createImage2D(32,32,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    views[0]=backend.createImageView(images[0].image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
    views[1]=backend.createImageView(images[1].image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    readback[0]=host.allocator.createBuffer(32*32*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    readback[1]=host.allocator.createBuffer(32*32*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    rhi::RenderPassDesc desc;desc.depthAttachment=1;desc.depthReadOnly=true;desc.externalReadDependency=true;
    desc.attachments={{VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
        {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,false,true,true}};
    desc.attachments[1].load=true;observe=true;pass=backend.createRenderPass(desc);observe=false;
    if(!sawReadOnly||observationErrors)throw std::runtime_error("Actual Vulkan depth descriptor must load, use a read-only subpass and synchronize preserving STORE writes");
    framebuffer=backend.createFramebuffer({pass,{views[0],views[1]},32,32});set=backend.createDescriptorSetLayout({});
    const rhi::PushConstantRangeDesc pushRange{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(float)};
    layout=backend.createPipelineLayout(set,&pushRange);
    vertex=backend.createShaderModule(readBinaryFile(argv[1]));fragment=backend.createShaderModule(readBinaryFile(argv[2]));
    rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;
    graphics.cullMode=VK_CULL_MODE_NONE;graphics.depthTest=true;graphics.depthWrite=false;graphics.depthCompare=VK_COMPARE_OP_LESS_OR_EQUAL;
    graphics.colorAttachmentCount=1;graphics.renderPass=pass;graphics.layout=layout;pipeline=backend.createGraphicsPipeline(graphics);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
        rhi::ImageBarrierDesc barrier;barrier.image=images[1].image;barrier.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(barrier);
        const VkClearDepthStencilValue clear{.5F,0};VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        vkCmdClearDepthStencilImage(raw,images[1].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&range);
        barrier.oldLayout=barrier.newLayout;barrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;commands.imageBarrier(barrier);
        rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent=extent;begin.clearValues.resize(2);
        commands.beginRenderPass(begin);commands.setViewport(32,32);commands.setScissor(extent);commands.bindPipeline(pipeline);
        for(const float depth:{.25F,.75F}){commands.pushConstants(layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&depth,4);commands.draw(3);}
        commands.endRenderPass();
        barrier.image=images[0].image;barrier.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        commands.imageBarrier(barrier);commands.copyImageToBuffer(images[0].image,readback[0].buffer,extent);
        barrier.image=images[1].image;barrier.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;commands.imageBarrier(barrier);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={32,32,1};
        vkCmdCopyImageToBuffer(raw,images[1].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback[1].buffer,1,&copy);
        for(const auto& buffer:readback){rhi::BufferBarrierDesc hostRead;hostRead.buffer=buffer.buffer;hostRead.size=buffer.size;
            hostRead.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;hostRead.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
            hostRead.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;hostRead.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(hostRead);}
    });
    for(const auto& buffer:readback)host.allocator.invalidate(buffer,0,buffer.size);
    const auto* rgba=static_cast<const std::uint16_t*>(readback[0].mapped);const auto* depth=static_cast<const float*>(readback[1].mapped);
    for(unsigned i=0;i<1024;++i)if(rgba[i*4]!=0x3c00||rgba[i*4+1]||rgba[i*4+2]||rgba[i*4+3]!=0x3c00||depth[i]!=.5F)
        throw std::runtime_error("Read-only depth must reject farther blue geometry and preserve every depth pixel");
    if(host.errors.load())throw std::runtime_error("Read-only depth Vulkan validation errors");
    std::cout<<"Read-only depth: actual LOAD/layout/dependencies, 1024 preserved depth pixels and red-only coverage passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
