#pragma once

#include "scene/Frustum.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <stdexcept>

namespace azurerender {
inline std::uint32_t cullingDispatchBatchSize(std::uint32_t remaining) {
    return std::min(remaining, 65535U * 64U);
}

struct alignas(16) GpuCullBounds {
    std::array<float, 4> minimum{};
    std::array<float, 4> maximum{};
};

struct GpuCullParameters {
    std::array<std::array<float, 4>, 6> planes{};
    std::uint32_t drawCount = 0;
    std::uint32_t cullingEnabled = 1;
    std::uint32_t firstCommand = 0;
};

static_assert(sizeof(GpuCullBounds) == 32, "GPU bounds stride must match std430");
static_assert(sizeof(GpuCullParameters) == 108, "GPU cull push layout must match GLSL");
static_assert(sizeof(VkDrawIndexedIndirectCommand) == 20, "GPU indirect command stride");

// Reference for GPU validation. One stable slot represents one instance;
// material and transparency ordering remain unchanged by culling.
inline VkDrawIndexedIndirectCommand referenceCullCommand(
    VkDrawIndexedIndirectCommand command, const GpuCullBounds& bounds,
    const GpuCullParameters& parameters) {
    if (command.instanceCount > 1)
        throw std::invalid_argument("GPU culling slots require one instance per draw");
    const scene::FrustumPlanes frustum{parameters.planes};
    if (parameters.cullingEnabled != 0 && !scene::boundsInsideFrustum(
        frustum, {bounds.minimum[0], bounds.minimum[1], bounds.minimum[2]},
        {bounds.maximum[0], bounds.maximum[1], bounds.maximum[2]}))
        command.instanceCount = 0;
    return command;
}

}  // namespace azurerender
