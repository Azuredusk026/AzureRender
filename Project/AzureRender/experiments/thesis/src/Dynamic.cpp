#include "Benchmark.hpp"

namespace thesis {
void Benchmark::createDynamic() {
    geometryPipeline=makePipeline(VK_NULL_HANDLE,0,"geometry.frag",true,3,true);
    toonPipeline=makePipeline(VK_NULL_HANDLE,0,"toon-dynamic.frag",false,3,true);
}

void Benchmark::recordDynamic() {
    Image* images[]={&albedo,&normal,&toon,&depth};
    VkImageMemoryBarrier transitions[4]{};
    for(int i=0;i<4;++i) {
        auto& b=transitions[i]; b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        b.dstAccessMask=i==3?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout=i==3?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            (i==2?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR);
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=images[i]->image;
        b.subresourceRange={VkImageAspectFlags(i==3?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT),0,1,0,1};
    }
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0,0,nullptr,0,nullptr,4,transitions);
    VkRenderingAttachmentInfo colors[3]{};
    for(int i=0;i<3;++i) {
        auto& a=colors[i]; a.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        a.imageView=images[i]->view;
        a.imageLayout=i==2?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
        a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; a.storeOp=i==0?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;
        if(i==1) a.clearValue.color.float32[3]=1;
    }
    VkRenderingAttachmentInfo d{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; d.imageView=depth.view;
    d.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; d.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    d.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE; d.clearValue.depthStencil.depth=1;
    VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO}; info.renderArea.extent={options.width,options.height};
    info.layerCount=1; info.colorAttachmentCount=3; info.pColorAttachments=colors; info.pDepthAttachment=&d;
    beginRendering(command,&info); ++beginRenderingCalls;
    uint32_t locations[]={0,1,2}, indices[]={0,1,VK_ATTACHMENT_UNUSED}, unused=VK_ATTACHMENT_UNUSED;
    VkRenderingAttachmentLocationInfoKHR locationInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR};
    locationInfo.colorAttachmentCount=3; locationInfo.pColorAttachmentLocations=locations;
    setLocations(command,&locationInfo);
    VkRenderingInputAttachmentIndexInfoKHR inputInfo{VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR};
    inputInfo.colorAttachmentCount=3; inputInfo.pColorAttachmentInputIndices=indices;
    inputInfo.pDepthInputAttachmentIndex=&unused; inputInfo.pStencilInputAttachmentIndex=&unused;
    setInputIndices(command,&inputInfo);
    geometryDraw();
    VkMemoryBarrier local{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    local.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    local.dstAccessMask=VK_ACCESS_INPUT_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_DEPENDENCY_BY_REGION_BIT,1,&local,0,nullptr,0,nullptr);
    fullscreenDraw(toonPipeline,inputPipelineLayout,inputSet); ++toonDraws;
    endRendering(command);
    VkImageMemoryBarrier sampled[2]{};
    Image* readable[]={&normal,&toon};
    for(int i=0;i<2;++i) {
        auto& b=sampled[i]; b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        b.oldLayout=i==0?VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=readable[i]->image; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    }
    // A full-image dependency is needed before the contour's neighboring reads.
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,2,sampled);
}
}
