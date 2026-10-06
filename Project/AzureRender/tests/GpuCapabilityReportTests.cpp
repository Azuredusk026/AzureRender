#include "diagnostics/GpuCapabilityReport.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

namespace {

nlohmann::json parseReport(const std::string& text) {
    return nlohmann::json::parse(text);
}
void require(bool value) { if (!value) throw std::runtime_error("GPU capability report contract failed"); }

}  // namespace

int main() try {
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
    require(report.contains("schema_version"));
    require(report["schema_version"] == 3);
    require(report.contains("resource_access"));
    require(report["resource_access"]["enabled"]["descriptor_indexing"] == false);
    require(report["resource_access"]["enabled"]["device_address"] == false);
    require(report["resource_access"]["enabled"]["allocator_device_address"] == false);
    require(report["resource_access"]["limits"]["max_per_stage_descriptor_samplers"] == 0);
    require(report.contains("device_name"));
    require(report["device_name"] == properties.deviceName);
    require(report.contains("vendor_id"));
    require(report["vendor_id"] == 0x10DE);
    require(report.contains("device_id"));
    require(report["device_id"] == 0x28A0);
    require(report.contains("api_version"));
    require(report["api_version"] == VK_API_VERSION_1_3);
    require(report.contains("driver_version"));
    require(report["driver_version"] == 0x123456);

    // Feature contract.
    require(report.contains("features"));
    require(report["features"]["sampler_anisotropy"] == true);
    require(report["features"]["shader_int64"] == false);
    require(report["compute"]["shader"] == true);
    require(report["compute"]["storage_image_write_without_format"] == false);

    // Descriptor indexing contract.
    require(report.contains("descriptor_indexing"));
    require(report["descriptor_indexing"]["runtime_descriptor_array"] == true);
    require(
        report["descriptor_indexing"]
              ["shader_sampled_image_array_non_uniform_indexing"]
        == true);
    require(
        report["descriptor_indexing"]["descriptor_binding_partially_bound"]
        == false);
    require(
        report["descriptor_indexing"]
              ["descriptor_binding_variable_descriptor_count"]
        == false);

    // Extension array contract.
    require(report.contains("extensions"));
    require(report["extensions"].is_array());
    require(report["extensions"].size() == 2);
    require(report["extensions"][0] == "VK_KHR_swapchain");
    require(report["extensions"][1] == "VK_KHR_dynamic_rendering");

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
    require(hostile["device_name"] == hostileProperties.deviceName);

    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
