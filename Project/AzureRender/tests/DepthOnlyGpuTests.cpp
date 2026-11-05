#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
using namespace azurerender;
void verify(int argc,char** argv,VkFormat format){
    if(argc!=3)
        throw std::invalid_argument("Expected depth-only vertex/fragment shaders");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
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
    image=host.allocator.createImage2D(32,32,format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    view=backend.createImageView(image.image,format,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    readback=host.allocator.createBuffer(32*32*(format==VK_FORMAT_D16_UNORM?2:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    rhi::RenderPassDesc desc;desc.depthAttachment=0;
    desc.attachments={{format,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true}};
    pass=backend.createRenderPass(desc);framebuffer=backend.createFramebuffer({pass,{view},32,32});
    set=backend.createDescriptorSetLayout({});
    const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT,sizeof(float)};
    layout=backend.createPipelineLayout(set,&push);vertex=backend.createShaderModule(readBinaryFile(argv[1]));
    fragment=backend.createShaderModule(readBinaryFile(argv[2]));
    std::vector<unsigned char> reference(static_cast<std::size_t>(readback.size));
    for(float bias:{0.F,1.25F,-2.F}){
      for(bool fragmentEnabled:{true,false}){
        rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;
        graphics.fragmentShader=fragmentEnabled?fragment:VK_NULL_HANDLE;
        graphics.cullMode=VK_CULL_MODE_NONE;graphics.renderPass=pass;graphics.layout=layout;
        graphics.depthCompare=VK_COMPARE_OP_LESS;graphics.depthBias=true;
        graphics.depthBiasConstant=bias;graphics.depthBiasSlope=1.75F;
        pipeline=backend.createGraphicsPipeline(graphics);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
          rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent=extent;
          begin.clearValues.resize(1);begin.clearValues[0].depthStencil={1,0};
          commands.beginRenderPass(begin);commands.setViewport(32,32);commands.setScissor({24,24},{4,4});
          commands.bindPipeline(pipeline);
          for(float depth:{.6F,.2F,.8F}){
            commands.pushConstants(layout,VK_SHADER_STAGE_VERTEX_BIT,0,&depth,4);commands.draw(3);
          }
          commands.endRenderPass();
          rhi::ImageBarrierDesc barrier;barrier.image=image.image;barrier.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
          barrier.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          barrier.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
          barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
          barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(barrier);
          VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={32,32,1};
          vkCmdCopyImageToBuffer(raw,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
          rhi::BufferBarrierDesc hostRead;hostRead.buffer=readback.buffer;hostRead.size=readback.size;
          hostRead.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;hostRead.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
          hostRead.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;hostRead.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(hostRead);
        });
        host.allocator.invalidate(readback,0,readback.size);
        const auto* values=static_cast<const unsigned char*>(readback.mapped);
        if(fragmentEnabled)std::copy(values,values+reference.size(),reference.begin());
        else if(std::memcmp(reference.data(),values,reference.size()))throw std::runtime_error("Vertex-only depth differs from fragment depth");
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){
          const auto i=y*32+x;
          const float value=format==VK_FORMAT_D16_UNORM
            ?reinterpret_cast<const std::uint16_t*>(values)[i]/65535.F:reinterpret_cast<const float*>(values)[i];
          const bool covered=x>=4&&x<28&&y>=4&&y<28;
          if(covered?(value<.19F||value>.21F):value!=1.F)throw std::runtime_error("Depth overlap/scissor/clear contract failed: format="+std::to_string(format)+" pixel="+std::to_string(i)+" depth="+std::to_string(value)+" bias="+std::to_string(bias)+" fragment="+std::to_string(fragmentEnabled));
        }
        backend.destroyPipeline(pipeline);pipeline={};
      }
    }
    cleanup.reset();
    if(host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)
      throw std::runtime_error("Depth-only test leaked resources");
    if(host.errors.load())throw std::runtime_error("Depth-only Vulkan validation errors");
    std::cout<<"Depth format "<<format<<": vertex-only and fragment depth byte exact, three biases, overlap/scissor/clear, release passed\n";
}
int main(int argc,char** argv)try{
    verify(argc,argv,VK_FORMAT_D16_UNORM);verify(argc,argv,VK_FORMAT_D32_SFLOAT);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
