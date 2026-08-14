#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>

namespace azurerender {

bool writeGpuCapabilityReport(
    VkPhysicalDevice device,
    const std::filesystem::path& path) noexcept;

}  // namespace azurerender
