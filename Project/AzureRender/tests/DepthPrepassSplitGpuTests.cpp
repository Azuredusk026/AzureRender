#include "render/DepthPrepassSplit.hpp"
#include "render/DepthPyramid.hpp"
#include "rhi/RenderPassDescription.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <array>
#include <cstring>
using namespace azurerender;
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct PassState {
    test::VulkanComputeFixture& host;VkRenderPass pass{};VkDescriptorSetLayout set{};VkPipelineLayout layout{};
    VkShaderModule vertex{},fragment{};VkPipeline prefill{},color{};
    explicit PassState(test::VulkanComputeFixture& h):host(h){}
    ~PassState(){auto& r=*host.backend;for(auto p:{prefill,color})if(p)r.destroyPipeline(p);
        for(auto s:{vertex,fragment})if(s)r.destroyShaderModule(s);if(layout)r.destroyPipelineLayout(layout);if(set)r.destroyDescriptorSetLayout(set);if(pass)vkDestroyRenderPass(host.device,pass,nullptr);}
};
struct Target {
    rhi::IRhi& r;std::vector<rhi::GpuImage> images;std::vector<VkImageView> views;VkFramebuffer framebuffer{};rhi::GpuBuffer readback{};
    explicit Target(rhi::IRhi& backend):r(backend){}
    ~Target(){if(framebuffer)r.destroyFramebuffer(framebuffer);for(auto v:views)if(v)r.destroyImageView(v);for(auto& i:images)r.allocator().destroyImage(i);r.allocator().destroyBuffer(readback);}
};
void verify(test::VulkanComputeFixture& host,const std::string& shaders,VkExtent2D extent,bool colors,std::uint64_t& compared){
    auto& r=*host.backend;PassState state(host);const unsigned count=colors?4U:1U,depthIndex=colors?1U:0U;
    std::vector<VkAttachmentDescription> attachments(count);std::vector<VkAttachmentReference> references;
    for(unsigned i=0;i<count;++i){auto& a=attachments[i];const bool depth=i==depthIndex;a.format=depth?VK_FORMAT_D32_SFLOAT:VK_FORMAT_R8G8B8A8_UNORM;
        a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.finalLayout=depth?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if(!depth)references.push_back({i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});}
    const VkAttachmentReference depthReference{depthIndex,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=static_cast<unsigned>(references.size());subpass.pColorAttachments=references.data();subpass.pDepthStencilAttachment=&depthReference;
    std::array<VkSubpassDependency,2> dependencies{};dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependencies[0].dstSubpass=0;
    dependencies[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask=dependencies[0].srcStageMask;dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass=0;dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};info.attachmentCount=count;info.pAttachments=attachments.data();info.subpassCount=1;info.pSubpasses=&subpass;info.dependencyCount=2;info.pDependencies=dependencies.data();
    test::vkRequire(vkCreateRenderPass(host.device,&info,nullptr,&state.pass),"vkCreateRenderPass source");
    const auto description=rhi::describeRenderPass(info);DepthPrepassSplit split(r,description);
    state.set=r.createDescriptorSetLayout({});const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,32};state.layout=r.createPipelineLayout(state.set,&range);
    state.vertex=r.createShaderModule(readBinaryFile(shaders+"/depth_prepass_test.vert.spv"));
    if(colors)state.fragment=r.createShaderModule(readBinaryFile(shaders+"/depth_prepass_test.frag.spv"));
    rhi::GraphicsPipelineDesc pipeline;pipeline.vertexShader=state.vertex;pipeline.fragmentShader=state.fragment;pipeline.layout=state.layout;pipeline.renderPass=state.pass;pipeline.cullMode=VK_CULL_MODE_NONE;pipeline.colorAttachmentCount=static_cast<unsigned>(references.size());
    state.color=r.createGraphicsPipeline(pipeline);pipeline.fragmentShader=VK_NULL_HANDLE;pipeline.colorWriteMasks.assign(references.size(),0);state.prefill=r.createGraphicsPipeline(pipeline);
    Target baseline(r),candidate(r);
    const auto create=[&](Target& target){
        for(unsigned i=0;i<count;++i){const bool depth=i==depthIndex;const auto usage=(depth?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            target.images.push_back(host.allocator.createImage2D(extent.width,extent.height,attachments[i].format,usage));
            target.views.push_back(r.createImageView(target.images.back().image,attachments[i].format,depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,1));}
        target.framebuffer=r.createFramebuffer({state.pass,target.views,extent.width,extent.height});target.readback=host.allocator.createBuffer(VkDeviceSize(extent.width)*extent.height*4*count,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    };create(baseline);create(candidate);
    unsigned levels=1;for(auto side=std::max(extent.width,extent.height);side>1;side/=2)++levels;
    DepthPyramidGpu pyramid(r,extent,levels,1,readBinaryFile(shaders+"/depth_pyramid.comp.spv"));
    const auto render=[&](Target& target,bool divided){
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
            rhi::RenderPassBeginDesc begin;begin.renderPass=divided?split.depthPass():state.pass;begin.framebuffer=target.framebuffer;begin.extent=extent;begin.clearValues.resize(count);
            for(unsigned i=0;i<count;++i)if(i==depthIndex)begin.clearValues[i].depthStencil={1,0};else begin.clearValues[i].color.float32[3]=1;
            const auto draw=[&](VkPipeline selected,float depth,float scale,float shift,std::array<float,4> color){
                const std::array<float,8> constants{color[0],color[1],color[2],color[3],depth,shift,scale,0};
                commands.setViewport(float(extent.width),float(extent.height));commands.setScissor(extent);commands.bindPipeline(selected);
                commands.pushConstants(state.layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,constants.data(),32);commands.draw(3);
            };
            commands.beginRenderPass(begin);draw(state.prefill,.4F,1,0,{0,0,0,1});
            if(divided){commands.endRenderPass();const auto depth=target.images[depthIndex].image;
                commands.imageBarrier({depth,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
                pyramid.record(commands,0,target.views[depthIndex],DepthReduction::Maximum,{},VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
                std::vector<VkImage> colorImages;for(unsigned i=0;i<count;++i)if(i!=depthIndex)colorImages.push_back(target.images[i].image);
                split.beforeColor(commands,depth,colorImages);begin.renderPass=split.colorPass();begin.clearValues[depthIndex].depthStencil.depth=0;
                commands.beginRenderPass(begin);
            }
            draw(state.color,.7F,1,0,{1,0,0,1});draw(state.color,.3F,.45F,.2F,{0,1,0,1});commands.endRenderPass();
            const VkDeviceSize bytes=VkDeviceSize(extent.width)*extent.height*4;
            for(unsigned i=0;i<count;++i){const bool depth=i==depthIndex;const VkImageAspectFlags aspect=depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
                const VkPipelineStageFlags sourceStage=depth?(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT):VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                const VkAccessFlags sourceAccess=depth?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                commands.imageBarrier({target.images[i].image,attachments[i].finalLayout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,
                    sourceStage,VK_PIPELINE_STAGE_TRANSFER_BIT,sourceAccess,VK_ACCESS_TRANSFER_READ_BIT,aspect});
                VkBufferImageCopy copy{};copy.bufferOffset=bytes*i;copy.imageSubresource={aspect,0,0,1};copy.imageExtent={extent.width,extent.height,1};
                vkCmdCopyImageToBuffer(native,target.images[i].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target.readback.buffer,1,&copy);
            }
            commands.bufferBarrier({target.readback.buffer,0,target.readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });host.allocator.invalidate(target.readback,0,target.readback.size);
    };render(baseline,false);render(candidate,true);
    require(std::memcmp(baseline.readback.mapped,candidate.readback.mapped,static_cast<std::size_t>(baseline.readback.size))==0,"Split color/depth output differs from one-pass source");
    const auto* depth=static_cast<const float*>(candidate.readback.mapped)+std::size_t(extent.width)*extent.height*depthIndex;
    bool front=false,prepared=false;for(unsigned i=0;i<extent.width*extent.height;++i){front|=depth[i]<.35F;prepared|=depth[i]>.35F&&depth[i]<.5F;}
    require(front&&prepared,"Depth load fixture must preserve covered and newly shaded regions");compared+=baseline.readback.size;
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");test::VulkanComputeFixture host(true);std::uint64_t compared=0;
    verify(host,argv[1],{17,9},true,compared);verify(host,argv[1],{35,19},false,compared);
    require(host.errors.load()==0&&!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Depth split validation or lifetime failure");
    std::cout<<"Depth prepass split: device="<<host.properties.deviceName<<" cases=2 comparedBytes="<<compared<<" validationErrors=0 liveBuffers=0 liveImages=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
