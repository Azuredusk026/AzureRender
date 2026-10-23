#include "ResourceAccessProfile.hpp"
#include <stdexcept>
#include <limits>

namespace azurerender {
namespace {
bool fits(std::uint64_t images, std::uint64_t samplerCount, std::uint64_t buffers, std::uint64_t colors, std::uint32_t samplers,
          std::uint32_t setSamplers, std::uint32_t sampled, std::uint32_t setSampled,
          std::uint32_t resources) {
    // Subtraction avoids overflow for untrusted 64-bit requirements.
    return samplerCount <= samplers && samplerCount <= setSamplers && images <= sampled &&
        images <= setSampled && images <= resources && buffers <= resources - images
        && colors <= resources - images - buffers;
}
std::uint64_t samplerCount(std::uint64_t images,std::uint64_t separateImages,std::uint64_t separateSamplers) {
    if(separateImages>images||separateSamplers>std::numeric_limits<std::uint64_t>::max()-(images-separateImages))
        throw std::invalid_argument("Invalid separate image/sampler descriptor requirements");
    return images-separateImages+separateSamplers;
}
std::uint32_t knownOrMinimum(std::uint32_t known, std::uint32_t minimum) {
    return known ? known : minimum;
}
}
ResourceAccessProfile ResourceAccessProfile::select(
    const DeviceCapabilities& device, const ResourceAccessRequirements& request) {
    if (!request.fixedSampledImageSlots || !request.sampledImageSlots)
        throw std::invalid_argument("Resource layouts require nonzero sampled image slots");
    const auto indexedSamplers=samplerCount(request.sampledImageSlots,request.separateImageSlots,request.separateSamplerSlots);
    const auto fixedSamplers=samplerCount(request.fixedSampledImageSlots,request.fixedSeparateImageSlots,request.fixedSeparateSamplerSlots);
    const bool address = device.deviceAddressSupported && device.deviceAddressEnabled &&
        device.allocatorDeviceAddressEnabled;
    if (request.requireDeviceAddress && !address)
        return {ResourceAccessMode::Unsupported, false, "Device address requires device and allocator enablement"};
    const auto& limits = device.limits;
    const bool fixed = fits(request.fixedSampledImageSlots, fixedSamplers, request.fragmentBuffers, request.fragmentColorAttachments,
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
        if (!fits(request.sampledImageSlots,indexedSamplers,request.fragmentBuffers,request.fragmentColorAttachments,
            update.maxPerStageDescriptorUpdateAfterBindSamplers,
            update.maxDescriptorSetUpdateAfterBindSamplers,
            update.maxPerStageDescriptorUpdateAfterBindSampledImages,
            update.maxDescriptorSetUpdateAfterBindSampledImages,
            update.maxPerStageUpdateAfterBindResources) ||
            request.sampledImageSlots>update.maxUpdateAfterBindDescriptorsInAllPools ||
            request.separateSamplerSlots>update.maxUpdateAfterBindDescriptorsInAllPools-request.sampledImageSlots ||
            request.fragmentBuffers>update.maxUpdateAfterBindDescriptorsInAllPools-request.sampledImageSlots-request.separateSamplerSlots)
            return {ResourceAccessMode::Unsupported,address,"Update-after-bind layout exceeds queried capacity"};
        return {ResourceAccessMode::Indexed,address,"Indexed update-after-bind layout within queried capacity"};
    }
    if (device.descriptorIndexingSupported && device.descriptorIndexingEnabled &&
        fits(request.sampledImageSlots, indexedSamplers, request.fragmentBuffers, request.fragmentColorAttachments,
            limits.maxPerStageDescriptorSamplers,limits.maxDescriptorSetSamplers,
            limits.maxPerStageDescriptorSampledImages,limits.maxDescriptorSetSampledImages,
            limits.maxPerStageResources))
        return {ResourceAccessMode::Indexed,address,"Indexed layout enabled within queried capacity"};
    return {ResourceAccessMode::Fixed,address,"Portable fixed layout selected by feature or capacity limits"};
}
} // namespace azurerender
