#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <string>
#include <vector>

namespace azurerender {
struct GpuEnabledCapabilities {
    bool descriptorIndexing = false;
    bool deviceAddress = false;
    bool allocatorDeviceAddress = false;
    bool updateAfterBind = false;
};

// Formats a JSON document from GPU capability data. Uses nlohmann::json for
// serialization so device and extension names are always validly escaped.
// The returned document matches schemas/gpu_capability_report.schema.json.
[[nodiscard]] std::string formatGpuCapabilityReport(
    const VkPhysicalDeviceProperties& properties,
    const VkPhysicalDeviceFeatures& features,
    const VkPhysicalDeviceVulkan12Features& vulkan12Features,
    const std::vector<VkExtensionProperties>& extensions,
    GpuEnabledCapabilities enabled = {},
    const VkPhysicalDeviceDescriptorIndexingProperties& indexing = {});

bool writeGpuCapabilityReport(
    VkPhysicalDevice device,
    const std::filesystem::path& path,
    GpuEnabledCapabilities enabled = {}) noexcept;

}  // namespace azurerender
