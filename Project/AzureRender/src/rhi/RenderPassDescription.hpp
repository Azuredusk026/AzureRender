#pragma once
#include "rhi/Rhi.hpp"
namespace azurerender::rhi {
// Captures the single-subpass layout represented by IRhi. Unsupported Vulkan
// layouts are rejected so callers can retain their original pass unchanged.
inline RenderPassDesc describeRenderPass(const VkRenderPassCreateInfo& source){
    if(source.sType!=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO||source.pNext||source.flags||source.subpassCount!=1||!source.pSubpasses||!source.attachmentCount
        ||source.attachmentCount>9||!source.pAttachments||source.dependencyCount>64
        ||(source.dependencyCount&&!source.pDependencies))throw std::invalid_argument("Unsupported native render pass description");
    const auto& subpass=source.pSubpasses[0];
    if(subpass.flags||subpass.pipelineBindPoint!=VK_PIPELINE_BIND_POINT_GRAPHICS||subpass.inputAttachmentCount
        ||subpass.pResolveAttachments||subpass.preserveAttachmentCount||(subpass.colorAttachmentCount&&!subpass.pColorAttachments))
        throw std::invalid_argument("Native render pass requires unsupported subpass features");
    RenderPassDesc result;
    if(subpass.pDepthStencilAttachment&&subpass.pDepthStencilAttachment->attachment!=VK_ATTACHMENT_UNUSED){
        const auto& depth=*subpass.pDepthStencilAttachment;
        if(depth.attachment>=source.attachmentCount||(depth.layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            &&depth.layout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL))throw std::invalid_argument("Unsupported native depth reference");
        result.depthAttachment=static_cast<std::int32_t>(depth.attachment);result.depthReadOnly=depth.layout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    unsigned color=0;
    for(unsigned i=0;i<source.attachmentCount;++i){const auto& item=source.pAttachments[i];
        if(item.flags||item.stencilLoadOp!=VK_ATTACHMENT_LOAD_OP_DONT_CARE||item.stencilStoreOp!=VK_ATTACHMENT_STORE_OP_DONT_CARE
            ||(item.loadOp!=VK_ATTACHMENT_LOAD_OP_CLEAR&&item.loadOp!=VK_ATTACHMENT_LOAD_OP_LOAD&&item.loadOp!=VK_ATTACHMENT_LOAD_OP_DONT_CARE)
            ||(item.storeOp!=VK_ATTACHMENT_STORE_OP_STORE&&item.storeOp!=VK_ATTACHMENT_STORE_OP_DONT_CARE))
            throw std::invalid_argument("Native attachment operations are not representable");
        const bool depth=static_cast<std::int32_t>(i)==result.depthAttachment;
        if(!depth){
            if(color>=subpass.colorAttachmentCount||subpass.pColorAttachments[color].attachment!=i
                ||subpass.pColorAttachments[color].layout!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                throw std::invalid_argument("Native color references must follow attachment order");
            ++color;
        }
        RenderPassAttachmentDesc a;a.format=item.format;a.samples=item.samples;a.initialLayout=item.initialLayout;a.finalLayout=item.finalLayout;
        a.clear=item.loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR;a.load=item.loadOp==VK_ATTACHMENT_LOAD_OP_LOAD;a.store=item.storeOp==VK_ATTACHMENT_STORE_OP_STORE;a.isDepth=depth;
        result.attachments.push_back(a);
    }
    if(color!=subpass.colorAttachmentCount)throw std::invalid_argument("Native color reference count differs");
    if(source.dependencyCount)result.dependencies.assign(source.pDependencies,source.pDependencies+source.dependencyCount);
    validateRenderPassDesc(result);return result;
}
}
