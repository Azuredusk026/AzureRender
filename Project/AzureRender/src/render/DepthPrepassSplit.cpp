#include "DepthPrepassSplit.hpp"
#include <algorithm>
namespace azurerender {
DepthPrepassSplit::Plan DepthPrepassSplit::plan(const rhi::RenderPassDesc& source){
    rhi::validateRenderPassDesc(source);
    if(source.attachments.empty()||source.attachments.size()>9||source.depthAttachment<0||source.depthReadOnly)
        throw std::invalid_argument("Depth split requires one writable depth attachment");
    Plan result{source,source};unsigned depthCount=0;
    for(std::size_t i=0;i<source.attachments.size();++i){const auto& original=source.attachments[i];
        if(original.format==VK_FORMAT_UNDEFINED||original.samples!=VK_SAMPLE_COUNT_1_BIT||!original.clear||original.load||!original.store
            ||original.finalLayout==VK_IMAGE_LAYOUT_UNDEFINED)throw std::invalid_argument("Depth split requires stored, cleared single-sample attachments");
        auto& depth=result.depth.attachments[i];auto& color=result.color.attachments[i];
        if(original.isDepth){++depthCount;
            if(original.format!=VK_FORMAT_D16_UNORM&&original.format!=VK_FORMAT_X8_D24_UNORM_PACK32&&original.format!=VK_FORMAT_D32_SFLOAT)
                throw std::invalid_argument("Depth split requires a depth-only format");
            if(static_cast<std::int32_t>(i)!=source.depthAttachment)throw std::invalid_argument("Depth split attachment identity differs");
            depth.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;depth.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            color.clear=false;color.load=true;color.initialLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }else{
            depth.clear=false;depth.load=false;depth.store=false;depth.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            depth.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
    }
    if(depthCount!=1)throw std::invalid_argument("Depth split requires exactly one depth attachment");
    rhi::validateRenderPassDesc(result.depth);rhi::validateRenderPassDesc(result.color);return result;
}
DepthPrepassSplit::DepthPrepassSplit(rhi::IRhi& backend,const rhi::RenderPassDesc& source):rhi_(backend){
    const auto description=plan(source);colors_=source.attachments.size()-1;
    depth_=rhi_.createRenderPass(description.depth);
    try{color_=rhi_.createRenderPass(description.color);}catch(...){rhi_.destroyRenderPass(depth_);depth_={};throw;}
}
DepthPrepassSplit::~DepthPrepassSplit(){if(color_)rhi_.destroyRenderPass(color_);if(depth_)rhi_.destroyRenderPass(depth_);}
void DepthPrepassSplit::beforeColor(rhi::ICommandRecorder& commands,VkImage depth,const std::vector<VkImage>& colors)const{
    if(!depth||colors.size()!=colors_)throw std::invalid_argument("Depth split framebuffer bindings differ");
    for(std::size_t i=0;i<colors.size();++i){
        if(!colors[i]||colors[i]==depth||std::find(colors.begin(),colors.begin()+i,colors[i])!=colors.begin()+i)
            throw std::invalid_argument("Depth split framebuffer images alias or are absent");
    }
    commands.imageBarrier({depth,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,1,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    for(auto image:colors)commands.imageBarrier({image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,1,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT});
}
}
