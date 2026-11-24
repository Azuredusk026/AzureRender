#include "rhi/VulkanRhi.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender::rhi;
namespace { unsigned expectedStages=0,calls=0;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice,VkPipelineCache,
    std::uint32_t count,const VkGraphicsPipelineCreateInfo* input,const VkAllocationCallbacks*,VkPipeline* output){
    require(count==1,"Expected one pipeline");const auto& p=input[0];
    require(p.stageCount==expectedStages,"Null fragment module must omit the fragment stage");
    require(p.pStages[0].stage==VK_SHADER_STAGE_VERTEX_BIT&&p.pStages[0].module!=VK_NULL_HANDLE,"Vertex stage retained");
    if(expectedStages==2)require(p.pStages[1].stage==VK_SHADER_STAGE_FRAGMENT_BIT&&p.pStages[1].module!=VK_NULL_HANDLE,"Fragment stage retained");
    require(p.pColorBlendState->attachmentCount==0&&p.pDepthStencilState->depthWriteEnable,"Depth-only attachments retained");
    output[0]=reinterpret_cast<VkPipeline>(std::uintptr_t(++calls));return VK_SUCCESS;
}
int main()try{
    GpuAllocator allocator;VulkanRhi backend({}, {}, {}, {},allocator);
    GraphicsPipelineDesc desc;desc.vertexShader=reinterpret_cast<VkShaderModule>(std::uintptr_t(1));
    desc.colorAttachmentCount=0;desc.depthWrite=true;
    expectedStages=1;require(backend.createGraphicsPipeline(desc)!=VK_NULL_HANDLE,"Depth-only creation");
    desc.fragmentShader=reinterpret_cast<VkShaderModule>(std::uintptr_t(2));expectedStages=2;
    require(backend.createGraphicsPipeline(desc)!=VK_NULL_HANDLE,"Conventional creation");
    require(calls==2,"Both paths must reach actual Vulkan conversion");
    std::cout<<"Actual Vulkan pipeline descriptions: vertex-only and vertex/fragment passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
