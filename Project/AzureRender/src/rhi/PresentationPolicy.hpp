#pragma once

#include <vulkan/vulkan.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace azurerender::rhi {
inline VkPresentModeKHR selectPresentMode(const std::vector<VkPresentModeKHR>& modes,
                                         bool synchronized) {
    const auto supports = [&](VkPresentModeKHR mode) {
        return std::find(modes.begin(), modes.end(), mode) != modes.end();
    };
    if (!supports(VK_PRESENT_MODE_FIFO_KHR))
        throw std::invalid_argument("Surface must advertise FIFO presentation");
    return !synchronized && supports(VK_PRESENT_MODE_MAILBOX_KHR)
        ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
}
}
