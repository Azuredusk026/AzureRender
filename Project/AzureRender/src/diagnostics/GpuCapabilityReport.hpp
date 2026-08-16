#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <string>
#include <vector>

namespace azurerender {

// Formats a JSON document from GPU capability data. Uses nlohmann::json for
// serialization so device and extension names are always validly escaped.
// The returned document matches schemas/gpu_capability_report.schema.json.
[[nodiscard]] std::string formatGpuCapabilityReport(
    const VkPhysicalDeviceProperties& properties,
    const VkPhysicalDeviceFeatures& features,
    const std::vector<VkExtensionProperties>& extensions);

bool writeGpuCapabilityReport(
    VkPhysicalDevice device,
    const std::filesystem::path& path) noexcept;

}  // namespace azurerender
