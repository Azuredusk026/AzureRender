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
    // Fragment image totals include combined and image-only descriptors. Standalone
    // samplers have independent stage limits and pool allocation counts.
    std::uint64_t separateImageSlots = 0;
    std::uint64_t separateSamplerSlots = 0;
    std::uint64_t fixedSeparateImageSlots = 0;
    std::uint64_t fixedSeparateSamplerSlots = 0;
    std::uint64_t fragmentColorAttachments = 0;
    // Additional vertex-only bindings share set capacity with the fragment
    // layout, but retain their own per-stage limits.
    std::uint64_t vertexCombinedImageSamplers = 0;
    std::uint64_t fixedVertexCombinedImageSamplers = 0;
    std::uint64_t vertexBuffers = 0;
    // Distinct buffer bindings across all stages. Zero retains the fragment
    // count for callers with a single-stage descriptor contract.
    std::uint64_t descriptorSetBuffers = 0;
};
struct ResourceAccessProfile {
    ResourceAccessMode mode = ResourceAccessMode::Unsupported;
    bool deviceAddress = false;
    std::string diagnostic;
    static ResourceAccessProfile select(const DeviceCapabilities&, const ResourceAccessRequirements&);
};
} // namespace azurerender
