#include "ResourceAccessProfile.hpp"
#include <stdexcept>

namespace azurerender {
namespace {
bool fits(std::uint64_t images, std::uint64_t buffers, std::uint32_t samplers,
          std::uint32_t setSamplers, std::uint32_t sampled, std::uint32_t setSampled,
          std::uint32_t resources) {
    // Subtraction avoids overflow for untrusted 64-bit requirements.
    return images <= samplers && images <= setSamplers && images <= sampled &&
        images <= setSampled && images <= resources && buffers <= resources - images;
}
std::uint32_t knownOrMinimum(std::uint32_t known, std::uint32_t minimum) {
    return known ? known : minimum;
}
}
ResourceAccessProfile ResourceAccessProfile::select(
    const DeviceCapabilities& device, const ResourceAccessRequirements& request) {
    if (!request.fixedSampledImageSlots || !request.sampledImageSlots)
        throw std::invalid_argument("Resource layouts require nonzero sampled image slots");
    const bool address = device.deviceAddressSupported && device.deviceAddressEnabled &&
        device.allocatorDeviceAddressEnabled;
    if (request.requireDeviceAddress && !address)
        return {ResourceAccessMode::Unsupported, false, "Device address requires device and allocator enablement"};
    const auto& limits = device.limits;
    const bool fixed = fits(request.fixedSampledImageSlots, request.fragmentBuffers,
        knownOrMinimum(limits.maxPerStageDescriptorSamplers,16),
        knownOrMinimum(limits.maxDescriptorSetSamplers,96),
        knownOrMinimum(limits.maxPerStageDescriptorSampledImages,16),
        knownOrMinimum(limits.maxDescriptorSetSampledImages,96),
        knownOrMinimum(limits.maxPerStageResources,128));
    if (!fixed)
        return {ResourceAccessMode::Unsupported, address, "Fixed layout exceeds device capacity"};
    if (request.requireUpdateAfterBind) {
        if (!device.updateAfterBindSupported || !device.updateAfterBindEnabled ||
            !device.descriptorIndexingSupported || !device.descriptorIndexingEnabled)
            return {ResourceAccessMode::Unsupported,address,"Update-after-bind requires explicit enablement"};
        const auto& update = device.indexingLimits;
        if (!fits(request.sampledImageSlots,request.fragmentBuffers,
            update.maxPerStageDescriptorUpdateAfterBindSamplers,
            update.maxDescriptorSetUpdateAfterBindSamplers,
            update.maxPerStageDescriptorUpdateAfterBindSampledImages,
            update.maxDescriptorSetUpdateAfterBindSampledImages,
            update.maxPerStageUpdateAfterBindResources) ||
            request.sampledImageSlots>update.maxUpdateAfterBindDescriptorsInAllPools ||
            request.fragmentBuffers>update.maxUpdateAfterBindDescriptorsInAllPools-request.sampledImageSlots)
            return {ResourceAccessMode::Unsupported,address,"Update-after-bind layout exceeds queried capacity"};
        return {ResourceAccessMode::Indexed,address,"Indexed update-after-bind layout within queried capacity"};
    }
    if (device.descriptorIndexingSupported && device.descriptorIndexingEnabled &&
        fits(request.sampledImageSlots, request.fragmentBuffers,
            limits.maxPerStageDescriptorSamplers,limits.maxDescriptorSetSamplers,
            limits.maxPerStageDescriptorSampledImages,limits.maxDescriptorSetSampledImages,
            limits.maxPerStageResources))
        return {ResourceAccessMode::Indexed,address,"Indexed layout enabled within queried capacity"};
    return {ResourceAccessMode::Fixed,address,"Portable fixed layout selected by feature or capacity limits"};
}
} // namespace azurerender
