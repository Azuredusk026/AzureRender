#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cstdint>
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=3&&(argc!=4||std::strcmp(argv[3],"--legacy-probe")!=0))
        throw std::invalid_argument("Expected depth-bias shaders and optional --legacy-probe");
    test::VulkanComputeFixture host(true,false,true);auto& backend=*host.backend;
    constexpr VkExtent2D extent{32,32};rhi::GpuImage image{};rhi::GpuBuffer readback{};
    VkImageView view{};VkRenderPass pass{};VkFramebuffer framebuffer{};VkDescriptorSetLayout set{};
    VkPipelineLayout layout{};VkShaderModule vertex{},fragment{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(vertex)backend.destroyShaderModule(vertex);
        if(fragment)backend.destroyShaderModule(fragment);if(layout)backend.destroyPipelineLayout(layout);
        if(set)backend.destroyDescriptorSetLayout(set);if(framebuffer)backend.destroyFramebuffer(framebuffer);
        if(pass)backend.destroyRenderPass(pass);if(view)backend.destroyImageView(view);
        host.allocator.destroyImage(image);host.allocator.destroyBuffer(readback);
    });
    image=host.allocator.createImage2D(32,32,VK_FORMAT_D16_UNORM,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    view=backend.createImageView(image.image,VK_FORMAT_D16_UNORM,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    readback=host.allocator.createBuffer(32*32*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    rhi::RenderPassDesc desc;desc.depthAttachment=0;
    desc.attachments={{VK_FORMAT_D16_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,false,true,true}};
    pass=backend.createRenderPass(desc);framebuffer=backend.createFramebuffer({pass,{view},32,32});
    set=backend.createDescriptorSetLayout({});
    const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT,sizeof(float)};
    layout=backend.createPipelineLayout(set,&push);vertex=backend.createShaderModule(readBinaryFile(argv[1]));
    fragment=backend.createShaderModule(readBinaryFile(argv[2]));
    std::array<int,3> baseline{};
    for(float bias:{0.F,1.F,-1.F,8.F,-8.F}){
        rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;
        graphics.cullMode=VK_CULL_MODE_NONE;graphics.renderPass=pass;graphics.layout=layout;
        graphics.depthBiasExact=argc==3;graphics.depthCompare=VK_COMPARE_OP_ALWAYS;graphics.depthBias=true;graphics.depthBiasConstant=bias;
        pipeline=backend.createGraphicsPipeline(graphics);
        unsigned depthIndex=0;
        // Integral attachment depths keep rounding ties separate from the
        // signed bias-unit contract being measured here.
        for(float depth:{12345.F/65535.F,32768.F/65535.F,54321.F/65535.F}){
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
                rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent=extent;
                begin.clearValues.resize(1);begin.clearValues[0].depthStencil={1,0};
                commands.beginRenderPass(begin);commands.setViewport(32,32);commands.setScissor(extent);
                commands.bindPipeline(pipeline);commands.pushConstants(layout,VK_SHADER_STAGE_VERTEX_BIT,0,&depth,4);
                commands.draw(3);commands.endRenderPass();
                rhi::ImageBarrierDesc barrier;barrier.image=image.image;barrier.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
                barrier.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                commands.imageBarrier(barrier);VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={32,32,1};
                vkCmdCopyImageToBuffer(raw,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                rhi::BufferBarrierDesc hostRead;hostRead.buffer=readback.buffer;hostRead.size=readback.size;
                hostRead.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;hostRead.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
                hostRead.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;hostRead.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(hostRead);
            });
            host.allocator.invalidate(readback,0,readback.size);
            const auto* values=static_cast<const std::uint16_t*>(readback.mapped);
            if(bias==0)baseline[depthIndex]=values[0];
            const int expected=baseline[depthIndex++]+static_cast<int>(bias);
            for(unsigned i=0;i<1024;++i)if(values[i]!=expected)
                throw std::runtime_error("Exact D16 bias must use one attachment unit: bias="+std::to_string(bias)+" expected="+std::to_string(expected)+" actual="+std::to_string(values[i]));
        }
        backend.destroyPipeline(pipeline);pipeline={};
    }
    if(host.errors.load())throw std::runtime_error("Depth-bias Vulkan validation errors");
    std::cout<<"Exact D16 depth bias: signed 1/8 units, three depths and 15360 pixels passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
