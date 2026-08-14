#include "GpuCapabilityReport.hpp"

#include "RuntimeDiagnostics.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace azurerender {

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
        output << "{\n"
               << "  \"schema_version\": 1,\n"
               << "  \"device_name\": \"" << properties.deviceName << "\",\n"
               << "  \"vendor_id\": " << properties.vendorID << ",\n"
               << "  \"device_id\": " << properties.deviceID << ",\n"
               << "  \"api_version\": " << properties.apiVersion << ",\n"
               << "  \"driver_version\": " << properties.driverVersion << ",\n"
               << "  \"features\": {\n"
               << "    \"sampler_anisotropy\": "
               << (features.samplerAnisotropy ? "true" : "false") << ",\n"
               << "    \"shader_int64\": "
               << (features.shaderInt64 ? "true" : "false") << "\n"
               << "  },\n"
               << "  \"extensions\": [\n";
        for (std::size_t index = 0; index < extensions.size(); ++index) {
            output << "    \"" << extensions[index].extensionName << "\""
                   << (index + 1 < extensions.size() ? "," : "") << '\n';
        }
        output << "  ]\n}\n";
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
