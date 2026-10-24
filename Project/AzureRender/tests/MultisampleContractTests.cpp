#include "rhi/NullRhi.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender::rhi;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejectAtomically(NullRhi& rhi,F operation){
    const auto before=rhi.calls.size();bool rejected=false;
    try{operation();}catch(const std::exception&){rejected=true;}
    require(rejected&&rhi.calls.size()==before,"Invalid multisampling must fail before resource creation");
}
}
int main()try{
    NullRhi rhi;GraphicsPipelineDesc pipeline;RenderPassDesc pass;
    pass.attachments={{VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    require(pipeline.rasterizationSamples==VK_SAMPLE_COUNT_1_BIT&&pipeline.minSampleShading==0
        &&pass.attachments[0].samples==VK_SAMPLE_COUNT_1_BIT,"Existing descriptors default to single-sample shading");
    require(rhi.imageSampleCounts(VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)==VK_SAMPLE_COUNT_1_BIT
        &&!rhi.supportsSampleShading(),"The recording backend declares conservative default capabilities");
    for(auto invalid:{0U,3U,128U}){
        pipeline.rasterizationSamples=static_cast<VkSampleCountFlagBits>(invalid);
        rejectAtomically(rhi,[&]{rhi.createGraphicsPipeline(pipeline);});
        pass.attachments[0].samples=static_cast<VkSampleCountFlagBits>(invalid);
        rejectAtomically(rhi,[&]{rhi.createRenderPass(pass);});
    }
    pipeline.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;pass.attachments[0].samples=VK_SAMPLE_COUNT_4_BIT;
    rejectAtomically(rhi,[&]{rhi.createRenderPass(pass);});
    rhi.supportedSamples=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;
    pipeline.renderPass=rhi.createRenderPass(pass);pipeline.colorAttachmentCount=1;
    for(float rate:{-.1F,1.1F,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        pipeline.minSampleShading=rate;rejectAtomically(rhi,[&]{rhi.createGraphicsPipeline(pipeline);});
    }
    pipeline.minSampleShading=1;rejectAtomically(rhi,[&]{rhi.createGraphicsPipeline(pipeline);});
    rhi.sampleShadingSupported=true;
    const auto handle=rhi.createGraphicsPipeline(pipeline);
    require(rhi.calls.back().detail.find("samples=4")!=std::string::npos
        &&rhi.calls.back().detail.find("sampleShading=1.000000")!=std::string::npos,"Recording preserves actual raster and shading policy");
    pipeline.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    rejectAtomically(rhi,[&]{rhi.createGraphicsPipeline(pipeline);});
    pass.depthAttachment=1;pass.attachments.push_back({VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,true,true,true});
    rejectAtomically(rhi,[&]{rhi.createRenderPass(pass);});
    rhi.destroyPipeline(handle);rhi.destroyRenderPass(pipeline.renderPass);
    const auto empty=rhi.createRenderPass({});
    GraphicsPipelineDesc attachmentless;attachmentless.renderPass=empty;attachmentless.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;
    const auto emptyPipeline=rhi.createGraphicsPipeline(attachmentless);
    rhi.destroyPipeline(emptyPipeline);rhi.destroyRenderPass(empty);
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;
    info.extent={8,8,1};info.arrayLayers=info.mipLevels=1;info.samples=VK_SAMPLE_COUNT_4_BIT;info.format=VK_FORMAT_R8G8B8A8_UNORM;
    auto image=rhi.allocator().createImage(info,false);
    require(rhi.allocator().statistics().liveImageBytes==8*8*4*4,"The recording allocator counts each sample");
    rhi.allocator().destroyImage(image);
    require(rhi.allocator().statistics().liveImages==0&&rhi.allocator().statistics().liveImageBytes==0,"Multisample allocations retire completely");
    std::cout<<"Multisample defaults, capabilities, atomic validation, attachment matching and allocation accounting passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
