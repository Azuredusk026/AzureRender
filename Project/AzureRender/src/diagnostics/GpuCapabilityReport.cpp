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
    const std::vector<VkExtensionProperties>& extensions) {
    nlohmann::json extensionsJson = nlohmann::json::array();
    for (const VkExtensionProperties& extension : extensions) {
        extensionsJson.push_back(extension.extensionName);
    }
    nlohmann::json report = {
        {"schema_version", 1},
        {"device_name", properties.deviceName},
        {"vendor_id", properties.vendorID},
        {"device_id", properties.deviceID},
        {"api_version", properties.apiVersion},
        {"driver_version", properties.driverVersion},
        {"features",
         {{"sampler_anisotropy", features.samplerAnisotropy == VK_TRUE},
          {"shader_int64", features.shaderInt64 == VK_TRUE}}},
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
        vkGetPhysicalDeviceFeatures(device, &features);
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
        output << formatGpuCapabilityReport(properties, features, extensions);
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
