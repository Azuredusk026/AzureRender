#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cstring>
using namespace azurerender;
namespace {
PFN_vkCreateRenderPass driverPass=nullptr;
PFN_vkCreateGraphicsPipelines driverPipelines=nullptr;
unsigned passCalls=0,pipelineCalls=0,observationErrors=0;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device,const VkRenderPassCreateInfo* info,
    const VkAllocationCallbacks* allocator,VkRenderPass* pass){
    ++passCalls;
    if(info->attachmentCount!=1||info->pAttachments[0].samples!=VK_SAMPLE_COUNT_4_BIT){
        ++observationErrors;std::cerr<<"Attachment sample count was not forwarded to Vulkan\n";return VK_ERROR_INITIALIZATION_FAILED;}
    return driverPass?driverPass(device,info,allocator,pass):VK_ERROR_INITIALIZATION_FAILED;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device,VkPipelineCache cache,
    std::uint32_t count,const VkGraphicsPipelineCreateInfo* infos,const VkAllocationCallbacks* allocator,VkPipeline* pipelines){
    ++pipelineCalls;
    for(unsigned i=0;i<count;++i){const auto* state=infos[i].pMultisampleState;
        if(!state||state->rasterizationSamples!=VK_SAMPLE_COUNT_4_BIT||!state->sampleShadingEnable||state->minSampleShading!=1){
            ++observationErrors;std::cerr<<"Raster sample count or shading fraction was not forwarded to Vulkan\n";return VK_ERROR_INITIALIZATION_FAILED;}}
    return driverPipelines?driverPipelines(device,cache,count,infos,allocator,pipelines):VK_ERROR_INITIALIZATION_FAILED;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    test::VulkanComputeFixture host(true,true);auto& backend=*host.backend;
    const auto usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(!backend.supportsSampleShading()||!host.properties.limits.standardSampleLocations
        ||!(backend.imageSampleCounts(VK_FORMAT_R16G16B16A16_SFLOAT,usage)&VK_SAMPLE_COUNT_4_BIT)){
        std::cout<<"Standard four-sample shading unavailable; GPU quality checks were not executed\n";return 77;}
    std::cout<<"Multisample device: "<<host.properties.deviceName<<", standardLocations=1, sampleShadingEnabled=1, HDRsampleMask="
        <<backend.imageSampleCounts(VK_FORMAT_R16G16B16A16_SFLOAT,usage)<<'\n';
    driverPass=reinterpret_cast<PFN_vkCreateRenderPass>(vkGetDeviceProcAddr(host.device,"vkCreateRenderPass"));
    driverPipelines=reinterpret_cast<PFN_vkCreateGraphicsPipelines>(vkGetDeviceProcAddr(host.device,"vkCreateGraphicsPipelines"));
    std::array<rhi::GpuImage,3> images{};std::array<VkImageView,2> views{};
    std::array<rhi::GpuBuffer,2> buffers{};VkRenderPass pass{};VkFramebuffer framebuffer{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkPipeline pipeline{};
    VkSampler sampler{};VkShaderModule vertex{},fragment{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(layout)backend.destroyPipelineLayout(layout);
        if(vertex)backend.destroyShaderModule(vertex);if(fragment)backend.destroyShaderModule(fragment);
        if(framebuffer)backend.destroyFramebuffer(framebuffer);if(pass)backend.destroyRenderPass(pass);
        if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        if(sampler)backend.destroySampler(sampler);for(auto view:views)if(view)backend.destroyImageView(view);
        for(auto& image:images)if(image.image)host.allocator.destroyImage(image);
        for(auto& buffer:buffers)if(buffer.buffer)host.allocator.destroyBuffer(buffer);
    });
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;
    image.extent={32,32,1};image.format=VK_FORMAT_R16G16B16A16_SFLOAT;image.mipLevels=image.arrayLayers=1;
    image.samples=VK_SAMPLE_COUNT_4_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=usage;
    images[0]=host.allocator.createImage(image,false);
    images[1]=host.allocator.createImage2D(32,32,image.format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    images[2]=host.allocator.createImage2D(4,1,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    views[0]=backend.createImageView(images[0].image,image.format,VK_IMAGE_ASPECT_COLOR_BIT,1);
    views[1]=backend.createImageView(images[2].image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);
    buffers[0]=host.allocator.createBuffer(16,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    buffers[1]=host.allocator.createBuffer(32*32*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    const std::array<std::uint8_t,16> alpha{255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,0};
    std::memcpy(buffers[0].mapped,alpha.data(),alpha.size());host.allocator.flush(buffers[0],0,16);
    backend.transitionImageLayout(images[2],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
    backend.copyBufferToImage(buffers[0],images[2],4,1);
    backend.transitionImageLayout(images[2],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
    rhi::SamplerDesc sampling;sampling.filter=VK_FILTER_NEAREST;sampler=backend.createSampler(sampling);
    rhi::RenderPassDesc renderPass;renderPass.attachments={{image.format,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    renderPass.attachments[0].samples=VK_SAMPLE_COUNT_4_BIT;pass=backend.createRenderPass(renderPass);
    framebuffer=backend.createFramebuffer({pass,{views[0]},32,32});
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
    pool=backend.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}},1});
    const auto set=backend.allocateDescriptorSets(pool,setLayout,1).at(0);
    backend.writeDescriptorImage({set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,views[1],sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_FRAGMENT_BIT,4};layout=backend.createPipelineLayout(setLayout,&push);
    vertex=backend.createShaderModule(readBinaryFile(std::string(argv[1])+"/multisample_probe.vert.spv"));
    fragment=backend.createShaderModule(readBinaryFile(std::string(argv[1])+"/multisample_probe.frag.spv"));
    rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;graphics.layout=layout;
    graphics.renderPass=pass;graphics.depthTest=graphics.depthWrite=false;graphics.cullMode=VK_CULL_MODE_NONE;
    graphics.colorAttachmentCount=1;graphics.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;graphics.minSampleShading=1;
    rhi::VulkanRhi disabled(host.device,host.physical,host.queue,host.pool,host.allocator);
    const auto before=pipelineCalls;bool rejected=false;VkPipeline unwanted{};
    try{unwanted=disabled.createGraphicsPipeline(graphics);}catch(const std::runtime_error&){rejected=true;}
    if(unwanted)backend.destroyPipeline(unwanted);
    if(!rejected||pipelineCalls!=before)throw std::runtime_error("Disabled sample shading must fail before driver creation");
    pipeline=backend.createGraphicsPipeline(graphics);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        rhi::ImageBarrierDesc barrier;barrier.image=images[0].image;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;commands.imageBarrier(barrier);
        barrier.image=images[1].image;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
        barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(barrier);
    });
    for(std::uint32_t masked=0;masked<2;++masked){
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent={32,32};begin.clearValues.resize(1);
            commands.beginRenderPass(begin);commands.setViewport(32,32);commands.setScissor({32,32});commands.bindPipeline(pipeline);
            commands.bindDescriptorSet(layout,set);commands.pushConstants(layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,&masked,4);commands.draw(3);commands.endRenderPass();
            rhi::ImageBarrierDesc barrier;barrier.image=images[0].image;barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(barrier);
            VkImageResolve resolve{};resolve.srcSubresource=resolve.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};resolve.extent={32,32,1};
            vkCmdResolveImage(raw,images[0].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[1].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&resolve);
            barrier.image=images[1].image;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(barrier);
            commands.copyImageToBuffer(images[1].image,buffers[1].buffer,{32,32});
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(barrier);
            barrier.image=images[0].image;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;commands.imageBarrier(barrier);
            rhi::BufferBarrierDesc read;read.buffer=buffers[1].buffer;read.size=buffers[1].size;
            read.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;read.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
            read.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;read.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(read);
        });
        host.allocator.invalidate(buffers[1],0,buffers[1].size);const auto* actual=static_cast<const std::uint16_t*>(buffers[1].mapped);
        const std::array<std::uint16_t,4> expected=masked?std::array<std::uint16_t,4>{0x2e00,0x2800,0,0x3400}:std::array<std::uint16_t,4>{0x3800,0x3800,0x3600,0x3c00};
        for(unsigned i=0;i<4096;++i)if(std::abs(int(actual[i])-int(expected[i%4]))>1)
            throw std::runtime_error("Actual four-sample interpolation or alpha coverage differs from the standard sample reference");
    }
    cleanup.reset();
    if(host.errors.load()||observationErrors||passCalls!=1||pipelineCalls!=1
        ||host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)
        throw std::runtime_error("Multisample driver state, validation or resource retirement failed");
    std::cout<<"Multisample GPU: 8192 HDR values, standard four-sample interpolation, alpha coverage, driver state and resource retirement passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
