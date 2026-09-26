#include "GpuCapabilityReport.hpp"

#include "RuntimeDiagnostics.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <vector>

namespace azurerender {

std::string formatGpuCapabilityReport(
    const VkPhysicalDeviceProperties& properties,
    const VkPhysicalDeviceFeatures& features,
    const VkPhysicalDeviceVulkan12Features& vulkan12Features,
    const std::vector<VkExtensionProperties>& extensions) {
    nlohmann::json extensionsJson = nlohmann::json::array();
    for (const VkExtensionProperties& extension : extensions) {
        extensionsJson.push_back(extension.extensionName);
    }
    nlohmann::json report = {
        {"schema_version", 2},
        {"device_name", properties.deviceName},
        {"vendor_id", properties.vendorID},
        {"device_id", properties.deviceID},
        {"api_version", properties.apiVersion},
        {"driver_version", properties.driverVersion},
        {"features",
         {{"sampler_anisotropy", features.samplerAnisotropy == VK_TRUE},
          {"shader_int64", features.shaderInt64 == VK_TRUE}}},
        {"compute",
         {{"shader", true},
          {"storage_image_write_without_format",
           features.shaderStorageImageWriteWithoutFormat == VK_TRUE}}},
        {"descriptor_indexing",
         {{"runtime_descriptor_array",
           vulkan12Features.runtimeDescriptorArray == VK_TRUE},
          {"shader_sampled_image_array_non_uniform_indexing",
           vulkan12Features.shaderSampledImageArrayNonUniformIndexing
               == VK_TRUE},
          {"descriptor_binding_partially_bound",
           vulkan12Features.descriptorBindingPartiallyBound == VK_TRUE},
          {"descriptor_binding_variable_descriptor_count",
           vulkan12Features.descriptorBindingVariableDescriptorCount
               == VK_TRUE}}},
        {"extensions", extensionsJson},
    };
    return report.dump(2) + "\n";
}

bool writeGpuCapabilityReport(
    const VkPhysicalDevice device,
    const std::filesystem::path& path) noexcept {
    try {
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceProperties(device, &properties);
        VkPhysicalDeviceVulkan12Features vulkan12Features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 features2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features2.pNext = &vulkan12Features;
        vkGetPhysicalDeviceFeatures2(device, &features2);
        features = features2.features;
        std::uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(
            device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(
            device, nullptr, &extensionCount, extensions.data());
        std::error_code error;
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path(), error);
        }
        std::ofstream output(path);
        if (!output) {
            RuntimeDiagnostics::instance().error(
                "gpu", DiagnosticCode::Runtime,
                "Unable to write GPU capability report: " + path.string());
            return false;
        }
        output << formatGpuCapabilityReport(
            properties, features, vulkan12Features, extensions);
        if (!output) {
            RuntimeDiagnostics::instance().error(
                "gpu", DiagnosticCode::Runtime,
                "Failed to flush GPU capability report: " + path.string());
            return false;
        }
        RuntimeDiagnostics::instance().info(
            "gpu", "GPU capability report: " + path.string());
        return true;
    } catch (...) {
        RuntimeDiagnostics::instance().error(
            "gpu", DiagnosticCode::Runtime,
            "Unexpected GPU capability report failure");
        return false;
    }
}

}  // namespace azurerender
