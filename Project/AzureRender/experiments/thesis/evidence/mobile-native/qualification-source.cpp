#include <vulkan/vulkan.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

static void check(VkResult r, const char* operation) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": " + std::to_string(r));
}

int main(int argc, char** argv) {
    VkInstance instance = VK_NULL_HANDLE;
    try {
        if (argc != 2 && argc != 3) throw std::runtime_error("Usage: AzureThesisQualification output.json [--attempt-local-read]");
        const bool attempt = argc == 3 && std::string(argv[2]) == "--attempt-local-read";
        if (argc == 3 && !attempt) throw std::runtime_error("unknown qualification option");
        std::ofstream out(argv[1]);
        if (!out) throw std::runtime_error("Cannot open output");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "AzureThesisQualification";
        app.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &app;
        check(vkCreateInstance(&ci, nullptr, &instance), "vkCreateInstance");
        uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "device count");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "devices");
        out << "{\"schemaVersion\":1,\"devices\":[";
        bool firstDevice = true;
        for (auto device : devices) {
            if (!firstDevice) out << ',';
            firstDevice = false;
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(device, &p);
            uint32_t n = 0;
            check(vkEnumerateDeviceExtensionProperties(device, nullptr, &n, nullptr), "extension count");
            std::vector<VkExtensionProperties> extensions(n);
            check(vkEnumerateDeviceExtensionProperties(device, nullptr, &n, extensions.data()), "extensions");
            bool localExtension = false;
            bool dynamicExtension = false;
            out << "{\"name\":" << std::quoted(p.deviceName)
                << ",\"apiVersion\":" << p.apiVersion << ",\"driverVersion\":" << p.driverVersion
                << ",\"vendorID\":" << p.vendorID << ",\"deviceID\":" << p.deviceID
                << ",\"timestampPeriodNs\":" << p.limits.timestampPeriod
                << ",\"maxColorAttachments\":" << p.limits.maxColorAttachments
                << ",\"maxPerStageDescriptorInputAttachments\":" << p.limits.maxPerStageDescriptorInputAttachments
                << ",\"extensions\":[";
            for (size_t i = 0; i < extensions.size(); ++i) {
                if (i) out << ',';
                out << std::quoted(extensions[i].extensionName);
                localExtension |= std::strcmp(extensions[i].extensionName, VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME) == 0;
                dynamicExtension |= std::strcmp(extensions[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0;
            }
            VkPhysicalDeviceDynamicRenderingFeatures dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
            VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR local{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR};
            VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            if (p.apiVersion >= VK_API_VERSION_1_3 || dynamicExtension) {
                features.pNext = &dynamic;
                dynamic.pNext = &local;
            }
            vkGetPhysicalDeviceFeatures2(device, &features);
            out << "],\"dynamicRendering\":" << (dynamic.dynamicRendering ? "true" : "false")
                << ",\"dynamicRenderingLocalRead\":" << (local.dynamicRenderingLocalRead ? "true" : "false");
            VkPhysicalDeviceVulkan14Properties properties14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES};
            if (p.apiVersion >= VK_API_VERSION_1_4) {
                VkPhysicalDeviceProperties2 pp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
                pp.pNext = &properties14;
                vkGetPhysicalDeviceProperties2(device, &pp);
                out << ",\"depthStencilLocalRead\":" << (properties14.dynamicRenderingLocalReadDepthStencilAttachments ? "true" : "false")
                    << ",\"multisampledLocalRead\":" << (properties14.dynamicRenderingLocalReadMultisampledAttachments ? "true" : "false");
            } else {
                out << ",\"depthStencilLocalRead\":null,\"multisampledLocalRead\":null";
            }
            vkGetPhysicalDeviceQueueFamilyProperties(device, &n, nullptr);
            std::vector<VkQueueFamilyProperties> queues(n);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &n, queues.data());
            out << ",\"queues\":[";
            for (size_t i = 0; i < queues.size(); ++i) {
                if (i) out << ',';
                out << "{\"index\":" << i << ",\"graphics\":" << ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? "true" : "false")
                    << ",\"timestampValidBits\":" << queues[i].timestampValidBits << '}';
            }
            struct Format { const char* name; VkFormat format; VkImageUsageFlags usage; };
            const VkImageUsageFlags color = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            const VkImageUsageFlags input = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            const VkImageUsageFlags sample = VK_IMAGE_USAGE_SAMPLED_BIT;
            const Format formats[] = {
                {"albedo", VK_FORMAT_R8G8B8A8_UNORM, color | input | sample},
                {"normalDepth", VK_FORMAT_R16G16B16A16_SFLOAT, color | input | sample},
                {"depth", VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
                {"toon", VK_FORMAT_R8G8B8A8_UNORM, color | sample},
                {"output", VK_FORMAT_R8G8B8A8_UNORM, color | VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
            };
            out << "],\"formats\":[";
            for (size_t i = 0; i < 5; ++i) {
                if (i) out << ',';
                VkImageFormatProperties f{};
                auto r = vkGetPhysicalDeviceImageFormatProperties(device, formats[i].format, VK_IMAGE_TYPE_2D,
                    VK_IMAGE_TILING_OPTIMAL, formats[i].usage, 0, &f);
                if (r != VK_SUCCESS && r != VK_ERROR_FORMAT_NOT_SUPPORTED) check(r, "image format query");
                out << "{\"name\":" << std::quoted(formats[i].name)
                    << ",\"format\":" << formats[i].format << ",\"usage\":" << formats[i].usage
                    << ",\"supported\":" << (r == VK_SUCCESS ? "true" : "false")
                    << ",\"maxWidth\":" << f.maxExtent.width << ",\"maxHeight\":" << f.maxExtent.height
                    << ",\"sampleCounts\":" << f.sampleCounts << '}';
            }
            out << ']';
            if (attempt) {
                uint32_t graphics = uint32_t(queues.size());
                for (uint32_t i = 0; i < queues.size(); ++i) {
                    if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphics = i; break; }
                }
                if (graphics == queues.size()) throw std::runtime_error("no graphics queue for local-read device attempt");
                float priority = 1.0f;
                VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                queueInfo.queueFamilyIndex = graphics; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
                VkPhysicalDeviceDynamicRenderingFeatures enabledDynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
                VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR enabledLocal{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR};
                enabledDynamic.dynamicRendering = VK_TRUE; enabledDynamic.pNext = &enabledLocal;
                enabledLocal.dynamicRenderingLocalRead = VK_TRUE;
                const char* requestedExtension = VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME;
                VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
                deviceInfo.pNext = &enabledDynamic; deviceInfo.queueCreateInfoCount = 1; deviceInfo.pQueueCreateInfos = &queueInfo;
                if (p.apiVersion < VK_API_VERSION_1_4) {
                    deviceInfo.enabledExtensionCount = 1; deviceInfo.ppEnabledExtensionNames = &requestedExtension;
                }
                VkDevice candidate{};
                auto result = vkCreateDevice(device, &deviceInfo, nullptr, &candidate);
                out << ",\"localReadDeviceCreationResult\":" << result;
                if (candidate) vkDestroyDevice(candidate, nullptr);
            }
            out << '}';
        }
        out << "]}\n";
        if (!out) throw std::runtime_error("Output write failed");
        vkDestroyInstance(instance, nullptr);
        return 0;
    } catch (const std::exception& e) {
        if (instance) vkDestroyInstance(instance, nullptr);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
