#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/MaterialUvGradients.hpp"
#include <array>
#include <cstring>
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Expected UV gradient vertex and fragment shaders");
    validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::Device,{});
    const MaterialUvGradientSupport supported{VK_SHADER_STAGE_FRAGMENT_BIT,VK_SUBGROUP_FEATURE_QUAD_BIT};
    validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,supported);
    unsigned rejections=0;
    for(const auto support:{MaterialUvGradientSupport{},MaterialUvGradientSupport{VK_SHADER_STAGE_COMPUTE_BIT,VK_SUBGROUP_FEATURE_QUAD_BIT},MaterialUvGradientSupport{VK_SHADER_STAGE_FRAGMENT_BIT,VK_SUBGROUP_FEATURE_BASIC_BIT}}){
        try{validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,support);}catch(const std::invalid_argument&){++rejections;}
    }
    try{validateMaterialUvGradientPolicy(static_cast<MaterialUvGradientPolicy>(99),supported);}catch(const std::invalid_argument&){++rejections;}
    try{(void)queryMaterialUvGradientSupport(VK_NULL_HANDLE);}catch(const std::invalid_argument&){++rejections;}
    if(rejections!=5)throw std::runtime_error("UV gradient capability and enum rejection contract failed");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,queryMaterialUvGradientSupport(host.physical));
    rhi::GpuImage image{};rhi::GpuBuffer readback{};VkImageView view{};VkRenderPass pass{};VkFramebuffer framebuffer{};
    VkDescriptorSetLayout set{};VkPipelineLayout layout{};VkShaderModule vertex{},fragment{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(vertex)backend.destroyShaderModule(vertex);
        if(fragment)backend.destroyShaderModule(fragment);if(layout)backend.destroyPipelineLayout(layout);
        if(set)backend.destroyDescriptorSetLayout(set);if(framebuffer)backend.destroyFramebuffer(framebuffer);
        if(pass)backend.destroyRenderPass(pass);if(view)backend.destroyImageView(view);
        host.allocator.destroyImage(image);host.allocator.destroyBuffer(readback);
    });
    constexpr auto format=VK_FORMAT_R32G32B32A32_SFLOAT;
    image=host.allocator.createImage2D(33,35,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    view=backend.createImageView(image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1);
    readback=host.allocator.createBuffer(33*35*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    rhi::RenderPassDesc description;description.attachments={{format,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,true,true,false}};
    pass=backend.createRenderPass(description);framebuffer=backend.createFramebuffer({pass,{view},33,35});
    set=backend.createDescriptorSetLayout({});const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_FRAGMENT_BIT,4};
    layout=backend.createPipelineLayout(set,&push);vertex=backend.createShaderModule(readBinaryFile(argv[1]));fragment=backend.createShaderModule(readBinaryFile(argv[2]));
    rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;graphics.cullMode=VK_CULL_MODE_NONE;
    graphics.renderPass=pass;graphics.layout=layout;graphics.depthTest=false;graphics.depthWrite=false;graphics.colorAttachmentCount=1;
    pipeline=backend.createGraphicsPipeline(graphics);unsigned pixels=0;
    for(const auto extent:{VkExtent2D{32,32},VkExtent2D{33,35}})for(unsigned discard=0;discard<2;++discard){
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent=extent;begin.clearValues.resize(1);
            for(auto& channel:begin.clearValues[0].color.float32)channel=-1;
            commands.beginRenderPass(begin);commands.setViewport(static_cast<float>(extent.width),static_cast<float>(extent.height));commands.setScissor(extent);commands.bindPipeline(pipeline);
            commands.pushConstants(layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,&discard,4);commands.draw(3);commands.endRenderPass();
            rhi::ImageBarrierDesc barrier;barrier.image=image.image;barrier.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(barrier);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={extent.width,extent.height,1};
            vkCmdCopyImageToBuffer(raw,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
            rhi::BufferBarrierDesc readable;readable.buffer=readback.buffer;readable.size=readback.size;readable.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
            readable.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;readable.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;readable.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            commands.bufferBarrier(readable);
        });
        host.allocator.invalidate(readback,0,readback.size);const auto values=static_cast<const std::array<float,4>*>(readback.mapped);
        for(unsigned y=0;y<extent.height;++y)for(unsigned x=0;x<extent.width;++x){
            const float dx=(float((y/2)*2)+1.5F)/1024.F,dy=(float((x/2)*2)+.5F)/1024.F;
            std::array<float,4> expected{dx,-2*dx,dy,-2*dy};if(discard&&((x+y)&3U)==0)expected.fill(-1);
            const auto& actual=values[y*extent.width+x];if(std::memcmp(expected.data(),actual.data(),16)!=0)
                throw std::runtime_error("UV gradient raster mismatch at "+std::to_string(x)+","+std::to_string(y)+" discard="+std::to_string(discard));
            ++pixels;
        }
    }
    cleanup.reset();if(host.errors.load())throw std::runtime_error("UV gradient Vulkan validation errors");
    std::cout<<host.properties.deviceName<<": "<<pixels<<" pixels / "<<pixels*4<<" exact components; even/odd extents, post-gradient discard, capability rejects passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
