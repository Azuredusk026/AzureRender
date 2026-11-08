#include "render/DepthPrepassSplit.hpp"
#include "rhi/NullRhi.hpp"
#include "rhi/RenderPassDescription.hpp"
#include <iostream>
#include <algorithm>
#include <array>
#include <cstring>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"Unsupported depth split accepted");}
rhi::RenderPassDesc description(){
    rhi::RenderPassDesc result;result.depthAttachment=1;
    result.attachments={{VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
        {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true},
        {VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false}};
    VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
    dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dependency.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;result.dependencies.push_back(dependency);return result;
}
}
int main()try{
    auto source=description();const auto plan=DepthPrepassSplit::plan(source);
    require(plan.depth.attachments.size()==3&&plan.color.attachments.size()==3,"Attachment layout changed");
    for(unsigned i=0;i<3;++i){require(plan.depth.attachments[i].format==source.attachments[i].format&&plan.color.attachments[i].samples==source.attachments[i].samples,"Compatibility format or sample count changed");
        if(i!=1)require(!plan.depth.attachments[i].clear&&!plan.depth.attachments[i].store&&plan.color.attachments[i].clear,"Color preparation must discard and color phase must clear");}
    require(plan.depth.attachments[1].clear&&plan.depth.attachments[1].store,"Prepared depth must be stored");
    require(!plan.color.attachments[1].clear&&plan.color.attachments[1].load&&plan.color.attachments[1].initialLayout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,"Color phase must load explicitly transitioned depth");
    require(plan.depth.dependencies.size()==1&&plan.color.dependencies.size()==1&&std::memcmp(plan.depth.dependencies.data(),source.dependencies.data(),sizeof(VkSubpassDependency))==0,"Source dependencies must remain compatible");
    require(source.attachments[0].clear&&source.attachments[1].clear&&!source.attachments[1].load,"Source description was mutated");
    std::array<VkAttachmentDescription,3> native{};
    for(unsigned i=0;i<3;++i){native[i].format=source.attachments[i].format;native[i].samples=VK_SAMPLE_COUNT_1_BIT;native[i].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;native[i].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        native[i].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;native[i].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;native[i].finalLayout=source.attachments[i].finalLayout;}
    const std::array<VkAttachmentReference,2> colors{{{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}};
    const VkAttachmentReference depthReference{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=2;subpass.pColorAttachments=colors.data();subpass.pDepthStencilAttachment=&depthReference;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};info.attachmentCount=3;info.pAttachments=native.data();info.subpassCount=1;info.pSubpasses=&subpass;info.dependencyCount=1;info.pDependencies=source.dependencies.data();
    const auto captured=rhi::describeRenderPass(info);require(captured.depthAttachment==1&&captured.attachments[1].isDepth&&captured.dependencies.size()==1,"Native pass metadata did not preserve actual references");
    auto badInfo=info;badInfo.subpassCount=2;rejects([&]{rhi::describeRenderPass(badInfo);});
    rhi::NullRhi backend;const auto original=backend.createRenderPass(source);
    {DepthPrepassSplit split(backend,source);require(split.depthPass()!=original&&split.colorPass()!=original&&split.depthPass()!=split.colorPass(),"Split owns two distinct compatible passes");
     rhi::NullCommandRecorder commands;split.beforeColor(commands,reinterpret_cast<VkImage>(std::uintptr_t(1)),{reinterpret_cast<VkImage>(std::uintptr_t(2)),reinterpret_cast<VkImage>(std::uintptr_t(3))});
     require(commands.calls.size()==3,"Depth load and both color writes require publication");
     rejects([&]{split.beforeColor(commands,VK_NULL_HANDLE,{});});}
    require(std::count_if(backend.calls.begin(),backend.calls.end(),[](const auto& call){return call.name=="destroyRenderPass";})==2,"Split must only destroy its own render passes");
    backend.destroyRenderPass(original);
    auto bad=source;bad.depthAttachment=-1;rejects([&]{DepthPrepassSplit::plan(bad);});
    bad=source;bad.attachments[0].clear=false;bad.attachments[0].load=true;bad.attachments[0].initialLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;rejects([&]{DepthPrepassSplit::plan(bad);});
    bad=source;for(auto& a:bad.attachments)a.samples=VK_SAMPLE_COUNT_4_BIT;rejects([&]{DepthPrepassSplit::plan(bad);});
    bad=source;bad.depthReadOnly=true;rejects([&]{DepthPrepassSplit::plan(bad);});
    bad=source;bad.attachments[1].format=VK_FORMAT_D24_UNORM_S8_UINT;rejects([&]{DepthPrepassSplit::plan(bad);});
    std::cout<<"Depth split: source compatibility, preserved depth, color publication, ownership and fallback passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
