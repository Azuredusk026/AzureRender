#include "rhi/VulkanRhi.hpp"

namespace azurerender::rhi {

void VulkanCommandRecorder::copyDepthImageToBuffer(
    VkImage image, VkBuffer buffer, VkExtent2D extent) {
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(commandBuffer_, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer, 1, &copy);
}

} // namespace azurerender::rhi
