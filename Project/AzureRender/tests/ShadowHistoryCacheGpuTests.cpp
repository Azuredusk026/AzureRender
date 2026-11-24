#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ShadowHistoryCache.hpp"
#include <cstring>
using namespace azurerender;
namespace {
void copyHistory(const rhi::GpuBuffer& source,const rhi::GpuBuffer& readback,rhi::ICommandRecorder& commands,VkCommandBuffer raw){
    commands.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
    commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT});
    VkBufferCopy copy{0,0,source.size};vkCmdCopyBuffer(raw,source.buffer,readback.buffer,1,&copy);
    commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    const std::string directory=argv[1];test::VulkanComputeFixture host(true,false,false,false,false,true);auto& backend=*host.backend;
    const auto code=readBinaryFile(directory+"/shadow_history_clear.comp.spv");
    rhi::GpuImage image{};rhi::GpuBuffer readback{};
    VkImageView view{};VkRenderPass pass{};VkFramebuffer framebuffer{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule vertex{},fragment{};VkPipeline pipeline{};
    std::unique_ptr<ShadowHistoryCache> cache;
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(vertex)backend.destroyShaderModule(vertex);if(fragment)backend.destroyShaderModule(fragment);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        if(framebuffer)backend.destroyFramebuffer(framebuffer);if(pass)backend.destroyRenderPass(pass);if(view)backend.destroyImageView(view);
        host.allocator.destroyImage(image);host.allocator.destroyBuffer(readback);cache.reset();
    });
    readback=host.allocator.createBuffer(32*32*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    {
        ShadowHistoryCache tail(backend,{17,9},2,2,code);
        for(unsigned slot=0;slot<2;++slot)for(unsigned workflow=0;workflow<2;++workflow){
            const auto ticket=tail.prepareClear(slot,workflow);const auto& buffer=tail.buffer(slot,workflow);
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
                commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT});
                vkCmdFillBuffer(raw,buffer.buffer,0,buffer.size,0x12345678U);tail.recordClear(commands,ticket);copyHistory(buffer,readback,commands,raw);
            });
            host.allocator.invalidate(readback,0,buffer.size);const auto* words=static_cast<const std::uint32_t*>(readback.mapped);
            for(unsigned word=0;word<buffer.size/4;++word)if(words[word]!=UINT32_MAX)throw std::runtime_error("History clear missed a tail word");
        }
    }
    image=host.allocator.createImage2D(32,32,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    view=backend.createImageView(image.image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);
    rhi::RenderPassDesc render;render.attachments={{VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,true,true,false}};
    pass=backend.createRenderPass(render);framebuffer=backend.createFramebuffer({pass,{view},32,32});
    cache=std::make_unique<ShadowHistoryCache>(backend,VkExtent2D{32,32},2,2,code);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
    rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=4;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4}};pool=backend.createDescriptorPool(poolDesc);
    const auto sets=backend.allocateDescriptorSets(pool,setLayout,4);
    for(unsigned workflow=0;workflow<2;++workflow)for(unsigned slot=0;slot<2;++slot){const auto& buffer=cache->buffer(slot,workflow);
        backend.writeDescriptorBuffer({sets[workflow*2+slot],0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffer.buffer,buffer.size});}
    const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_VERTEX_BIT,4};layout=backend.createPipelineLayout(setLayout,&push);
    vertex=backend.createShaderModule(readBinaryFile(directory+"/shadow_history_ownership_test.vert.spv"));
    fragment=backend.createShaderModule(readBinaryFile(directory+"/shadow_history_ownership_test.frag.spv"));
    rhi::GraphicsPipelineDesc graphics;graphics.vertexShader=vertex;graphics.fragmentShader=fragment;graphics.layout=layout;graphics.renderPass=pass;
    graphics.colorAttachmentCount=1;graphics.colorWriteMasks={0};graphics.depthTest=false;graphics.depthWrite=false;graphics.cullMode=VK_CULL_MODE_NONE;
    pipeline=backend.createGraphicsPipeline(graphics);unsigned checked=0;
    for(unsigned layers:{1U,32U,128U})for(unsigned iteration=0;iteration<8;++iteration)for(unsigned workflow=0;workflow<2;++workflow){
        const unsigned slot=iteration%2,seed=iteration*13+workflow*19;
        const auto ticket=cache->prepareClear(slot,workflow);const auto& buffer=cache->buffer(slot,workflow);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            cache->recordClear(commands,ticket);
            commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_IMAGE_ASPECT_COLOR_BIT});
            rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent={32,32};begin.clearValues.resize(1);
            commands.beginRenderPass(begin);commands.setViewport(32,32);commands.setScissor({32,32});commands.bindPipeline(pipeline);commands.bindDescriptorSet(layout,sets[workflow*2+slot]);
            commands.pushConstants(layout,VK_SHADER_STAGE_VERTEX_BIT,0,&seed,4);commands.draw(layers*3);commands.endRenderPass();copyHistory(buffer,readback,commands,raw);
        });
        host.allocator.invalidate(readback,0,buffer.size);const auto* values=static_cast<const std::uint32_t*>(readback.mapped);
        for(unsigned pixel=0;pixel<1024;++pixel){const auto key=values[pixel*2],value=values[pixel*2+1];bool admitted=false;
            for(unsigned layer=0;layer<layers;++layer)admitted|=key==(layer+seed)%128;
            if(!admitted||value!=(key^0xa5a5a5a5U))throw std::runtime_error("History first-writer record is torn or uncleared");++checked;
        }
    }
    cleanup.reset();const auto stats=host.allocator.statistics();
    if(host.errors.load()||stats.liveBuffers||stats.liveImages)throw std::runtime_error("Shadow history validation or lifetime failure");
    std::cout<<"Shadow history GPU: 4 tail clears, "<<checked<<" ownership records, two slots/workflows, zero validation errors and live allocations\n";
    return 0;
}catch(const test::VulkanFeatureUnavailable& error){std::cerr<<error.what()<<'\n';return 77;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
