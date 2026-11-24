#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
using namespace azurerender;
void verify(int argc,char** argv,VkFormat format,unsigned colorCount){
    if(argc!=3)
        throw std::invalid_argument("Expected depth-only vertex/fragment shaders");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    constexpr VkExtent2D extent{32,32};rhi::GpuImage image{};rhi::GpuBuffer readback{};
    VkImageView view{};VkRenderPass pass{};VkFramebuffer framebuffer{};VkDescriptorSetLayout set{};
    VkPipelineLayout layout{};VkShaderModule vertex{},fragment{};VkPipeline pipeline{};
    std::array<rhi::GpuImage,3> colors{};std::array<rhi::GpuBuffer,3> colorReadbacks{};
    std::array<VkImageView,3> colorViews{};
    constexpr std::array<VkFormat,3> colorFormats{VK_FORMAT_R16G16B16A16_SFLOAT,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8_UNORM};
    constexpr std::array<unsigned,3> pixelBytes{8,4,1};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(vertex)backend.destroyShaderModule(vertex);
        if(fragment)backend.destroyShaderModule(fragment);if(layout)backend.destroyPipelineLayout(layout);
        if(set)backend.destroyDescriptorSetLayout(set);if(framebuffer)backend.destroyFramebuffer(framebuffer);
        if(pass)backend.destroyRenderPass(pass);if(view)backend.destroyImageView(view);
        host.allocator.destroyImage(image);host.allocator.destroyBuffer(readback);
        for(unsigned i=0;i<colorCount;++i){if(colorViews[i])backend.destroyImageView(colorViews[i]);
          host.allocator.destroyImage(colors[i]);host.allocator.destroyBuffer(colorReadbacks[i]);}
    });
    image=host.allocator.createImage2D(32,32,format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    view=backend.createImageView(image.image,format,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    readback=host.allocator.createBuffer(32*32*(format==VK_FORMAT_D16_UNORM?2:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    rhi::RenderPassDesc desc;desc.depthAttachment=0;
    desc.attachments={{format,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true}};
    std::vector<VkImageView> views{view};
    for(unsigned i=0;i<colorCount;++i){
      colors[i]=host.allocator.createImage2D(32,32,colorFormats[i],VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
      colorViews[i]=backend.createImageView(colors[i].image,colorFormats[i],VK_IMAGE_ASPECT_COLOR_BIT,1);views.push_back(colorViews[i]);
      colorReadbacks[i]=host.allocator.createBuffer(32*32*pixelBytes[i],VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
      desc.attachments.push_back({colorFormats[i],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,true,true,false});
    }
    pass=backend.createRenderPass(desc);framebuffer=backend.createFramebuffer({pass,views,32,32});
    set=backend.createDescriptorSetLayout({});
    const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT,sizeof(float)};
    layout=backend.createPipelineLayout(set,&push);vertex=backend.createShaderModule(readBinaryFile(argv[1]));
    fragment=backend.createShaderModule(readBinaryFile(argv[2]));
    std::vector<unsigned char> reference(static_cast<std::size_t>(readback.size));
    for(float bias:{0.F,1.25F,-2.F}){
      for(bool fragmentEnabled:{true,false}){
        rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;
        graphics.fragmentShader=fragmentEnabled?fragment:VK_NULL_HANDLE;
        graphics.colorAttachmentCount=colorCount;graphics.colorWriteMasks.assign(colorCount,0);
        graphics.cullMode=VK_CULL_MODE_NONE;graphics.renderPass=pass;graphics.layout=layout;
        graphics.depthCompare=VK_COMPARE_OP_LESS;graphics.depthBias=true;
        graphics.depthBiasConstant=bias;graphics.depthBiasSlope=1.75F;
        pipeline=backend.createGraphicsPipeline(graphics);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
          rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent=extent;
          begin.clearValues.resize(1+colorCount);begin.clearValues[0].depthStencil={1,0};
          for(unsigned i=0;i<colorCount;++i)begin.clearValues[i+1].color={{i%2?0.F:1.F,i%2?1.F:0.F,1.F,1.F}};
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
          for(unsigned i=0;i<colorCount;++i){
            rhi::ImageBarrierDesc colorBarrier; colorBarrier.image=colors[i].image;colorBarrier.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            colorBarrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;colorBarrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            colorBarrier.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;colorBarrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
            colorBarrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;colorBarrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(colorBarrier);
            VkBufferImageCopy colorCopy{};colorCopy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};colorCopy.imageExtent={32,32,1};
            vkCmdCopyImageToBuffer(raw,colors[i].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,colorReadbacks[i].buffer,1,&colorCopy);
            hostRead.buffer=colorReadbacks[i].buffer;hostRead.size=colorReadbacks[i].size;commands.bufferBarrier(hostRead);
          }
        });
        host.allocator.invalidate(readback,0,readback.size);
        const auto* values=static_cast<const unsigned char*>(readback.mapped);
        if(fragmentEnabled)std::copy(values,values+reference.size(),reference.begin());
        else if(std::memcmp(reference.data(),values,reference.size()))throw std::runtime_error("Vertex-only depth differs from fragment depth");
        for(unsigned i=0;i<colorCount;++i){
          host.allocator.invalidate(colorReadbacks[i],0,colorReadbacks[i].size);
          const auto* bytes=static_cast<const unsigned char*>(colorReadbacks[i].mapped);
          for(unsigned pixel=0;pixel<32*32;++pixel){
            if(i==0){const std::array<std::uint16_t,4> expected{0x3c00,0,0x3c00,0x3c00};
              if(std::memcmp(bytes+pixel*8,expected.data(),8))throw std::runtime_error("Masked FP16 color changed");}
            else if(i==1){const std::array<unsigned char,4> expected{0,255,255,255};
              if(std::memcmp(bytes+pixel*4,expected.data(),4))throw std::runtime_error("Masked UNORM color changed");}
            else if(bytes[pixel]!=255)throw std::runtime_error("Masked display attachment changed");
          }
        }
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
    std::cout<<"Depth format "<<format<<", color attachments "<<colorCount<<": depth byte exact, masked colors, three biases, overlap/scissor/clear, release passed\n";
}
int main(int argc,char** argv)try{
    for(unsigned colors:{0U,2U,3U}){verify(argc,argv,VK_FORMAT_D16_UNORM,colors);verify(argc,argv,VK_FORMAT_D32_SFLOAT,colors);}return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
