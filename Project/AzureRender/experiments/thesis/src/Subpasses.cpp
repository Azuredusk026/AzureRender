#include "Benchmark.hpp"
#include <stdexcept>

namespace thesis {
void Benchmark::createSubpasses() {
    VkAttachmentDescription attachments[4]{};
    Image* images[]={&albedo,&normal,&toon,&depth};
    for(int i=0;i<4;++i) {
        auto& a=attachments[i]; a.format=images[i]->format; a.samples=VK_SAMPLE_COUNT_1_BIT;
        a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; a.storeOp=(i==0 || i==3)?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout=i==3?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    VkAttachmentReference gcolors[]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkAttachmentReference depthRef{3,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentReference inputs[]={{0,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{1,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    VkAttachmentReference target{2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpasses[2]{};
    subpasses[0].pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpasses[0].colorAttachmentCount=2;
    subpasses[0].pColorAttachments=gcolors; subpasses[0].pDepthStencilAttachment=&depthRef;
    subpasses[1].pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpasses[1].colorAttachmentCount=1;
    subpasses[1].pColorAttachments=&target; subpasses[1].inputAttachmentCount=2; subpasses[1].pInputAttachments=inputs;
    VkSubpassDependency dependencies[3]{};
    auto& first=dependencies[0]; first.srcSubpass=VK_SUBPASS_EXTERNAL; first.dstSubpass=0;
    first.srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; first.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    first.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    first.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    auto& local=dependencies[1]; local.srcSubpass=0; local.dstSubpass=1;
    // The last use of input attachments also executes their store operations.
    // Include color-output writes as well as fragment reads in the destination.
    local.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    local.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    local.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    local.dstAccessMask=VK_ACCESS_INPUT_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    local.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;
    auto& external=dependencies[2]; external.srcSubpass=1; external.dstSubpass=VK_SUBPASS_EXTERNAL;
    external.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    external.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    external.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; external.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; ci.attachmentCount=4; ci.pAttachments=attachments;
    ci.subpassCount=2; ci.pSubpasses=subpasses; ci.dependencyCount=3; ci.pDependencies=dependencies;
    if(vkCreateRenderPass(device,&ci,nullptr,&geometryPass)!=VK_SUCCESS) throw std::runtime_error("two-subpass render pass creation failed");
    geometryFramebuffer=makeFramebuffer(geometryPass,{&albedo,&normal,&toon,&depth});
    geometryPipeline=makePipeline(geometryPass,0,"geometry.frag",true,2);
    toonPipeline=makePipeline(geometryPass,1,"toon-local.frag",false,1);
}
}
