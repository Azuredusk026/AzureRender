#include "rhi/VulkanRhi.hpp"
#include <string>

namespace azurerender::rhi {
VkImageView VulkanRhi::createMappedSampledImageView(VkImage image,VkFormat format,
        std::uint32_t levels,const VkComponentMapping& mapping) {
    VkImageViewCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    create.image=image;create.viewType=VK_IMAGE_VIEW_TYPE_2D;create.format=format;
    create.components=mapping;
    create.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,1};
    VkImageView view=VK_NULL_HANDLE;
    const auto result=vkCreateImageView(device_,&create,nullptr,&view);
    if(result!=VK_SUCCESS)throw std::runtime_error("vkCreateImageView component mapping: "+std::to_string(result));
    return view;
}
}
