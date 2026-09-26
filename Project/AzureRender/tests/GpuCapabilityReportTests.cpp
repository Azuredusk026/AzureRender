#include "diagnostics/GpuCapabilityReport.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace {

nlohmann::json parseReport(const std::string& text) {
    return nlohmann::json::parse(text);
}

}  // namespace

int main() {
    using namespace azurerender;

    VkPhysicalDeviceProperties properties{};
    std::strncpy(
        properties.deviceName,
        "NVIDIA GeForce RTX 4060 Laptop GPU",
        VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
    properties.vendorID = 0x10DE;
    properties.deviceID = 0x28A0;
    properties.apiVersion = VK_API_VERSION_1_3;
    properties.driverVersion = 0x123456;

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.shaderInt64 = VK_FALSE;

    VkPhysicalDeviceVulkan12Features vulkan12Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_FALSE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_FALSE;

    std::vector<VkExtensionProperties> extensions(2);
    std::strncpy(
        extensions[0].extensionName,
        "VK_KHR_swapchain",
        VK_MAX_EXTENSION_NAME_SIZE - 1);
    std::strncpy(
        extensions[1].extensionName,
        "VK_KHR_dynamic_rendering",
        VK_MAX_EXTENSION_NAME_SIZE - 1);

    const std::string text = formatGpuCapabilityReport(
        properties, features, vulkan12Features, extensions);
    const nlohmann::json report = parseReport(text);

    // Schema contract: top-level fields exist with the expected types.
    assert(report.contains("schema_version"));
    assert(report["schema_version"] == 2);
    assert(report.contains("device_name"));
    assert(report["device_name"] == properties.deviceName);
    assert(report.contains("vendor_id"));
    assert(report["vendor_id"] == 0x10DE);
    assert(report.contains("device_id"));
    assert(report["device_id"] == 0x28A0);
    assert(report.contains("api_version"));
    assert(report["api_version"] == VK_API_VERSION_1_3);
    assert(report.contains("driver_version"));
    assert(report["driver_version"] == 0x123456);

    // Feature contract.
    assert(report.contains("features"));
    assert(report["features"]["sampler_anisotropy"] == true);
    assert(report["features"]["shader_int64"] == false);
    assert(report["compute"]["shader"] == true);
    assert(report["compute"]["storage_image_write_without_format"] == false);

    // Descriptor indexing contract.
    assert(report.contains("descriptor_indexing"));
    assert(report["descriptor_indexing"]["runtime_descriptor_array"] == true);
    assert(
        report["descriptor_indexing"]
              ["shader_sampled_image_array_non_uniform_indexing"]
        == true);
    assert(
        report["descriptor_indexing"]["descriptor_binding_partially_bound"]
        == false);
    assert(
        report["descriptor_indexing"]
              ["descriptor_binding_variable_descriptor_count"]
        == false);

    // Extension array contract.
    assert(report.contains("extensions"));
    assert(report["extensions"].is_array());
    assert(report["extensions"].size() == 2);
    assert(report["extensions"][0] == "VK_KHR_swapchain");
    assert(report["extensions"][1] == "VK_KHR_dynamic_rendering");

    // JSON safety: embedded quotes and backslashes must be escaped so the
    // document still parses and round-trips exactly.
    VkPhysicalDeviceProperties hostileProperties{};
    std::strncpy(
        hostileProperties.deviceName,
        "GPU \"quoted\" \\ backslash \n newline",
        VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
    const std::string hostileText = formatGpuCapabilityReport(
        hostileProperties,
        VkPhysicalDeviceFeatures{},
        VkPhysicalDeviceVulkan12Features{},
        {});
    const nlohmann::json hostile = parseReport(hostileText);
    assert(hostile["device_name"] == hostileProperties.deviceName);

    return 0;
}
