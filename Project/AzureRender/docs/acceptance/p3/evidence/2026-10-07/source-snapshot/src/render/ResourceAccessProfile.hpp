#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>

namespace azurerender {
enum class ResourceAccessMode { Fixed, Indexed, Unsupported };
struct DeviceCapabilities {
    bool descriptorIndexingSupported = false;
    bool descriptorIndexingEnabled = false;
    VkPhysicalDeviceLimits limits{};
    bool deviceAddressSupported = false;
    bool deviceAddressEnabled = false;
    bool allocatorDeviceAddressEnabled = false;
    bool updateAfterBindSupported = false;
    bool updateAfterBindEnabled = false;
    VkPhysicalDeviceDescriptorIndexingProperties indexingLimits{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES};
};
struct ResourceAccessRequirements {
    std::uint64_t sampledImageSlots = 0;
    std::uint64_t fixedSampledImageSlots = 0;
    std::uint64_t fragmentBuffers = 0;
    bool requireDeviceAddress = false;
    bool requireUpdateAfterBind = false;
};
struct ResourceAccessProfile {
    ResourceAccessMode mode = ResourceAccessMode::Unsupported;
    bool deviceAddress = false;
    std::string diagnostic;
    static ResourceAccessProfile select(const DeviceCapabilities&, const ResourceAccessRequirements&);
};
} // namespace azurerender
