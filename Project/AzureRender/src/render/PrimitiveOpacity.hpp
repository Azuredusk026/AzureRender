#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace azurerender {
struct PrimitiveOpacityRun {
    std::uint32_t firstInstance;
    std::uint32_t instanceCount;
    float opacity;
};

// Preserve source instance indices so direct and indirect draws share the same
// immutable pose mapping. Assets without a source node retain full coverage.
template<class Emit>
void forEachPrimitiveOpacityRun(const std::vector<std::vector<float>>& values,
    std::uint32_t nodeIndex, std::uint32_t firstInstance,
    std::uint32_t instanceCount, Emit&& emit) {
    if (instanceCount == 0) return;
    if (values.empty() || nodeIndex == std::numeric_limits<std::uint32_t>::max()) {
        emit(PrimitiveOpacityRun{firstInstance, instanceCount, 1.0F});
        return;
    }
    if (firstInstance > values.size() || instanceCount > values.size() - firstInstance)
        throw std::out_of_range("Primitive opacity instance range");
    const auto opacityAt = [&](std::uint32_t instance) {
        if (nodeIndex >= values[instance].size())
            throw std::out_of_range("Primitive opacity source node");
        const float value = values[instance][nodeIndex];
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
            throw std::invalid_argument("Primitive opacity outside [0,1]");
        return value;
    };
    PrimitiveOpacityRun run{firstInstance, 1, opacityAt(firstInstance)};
    for (std::uint32_t offset = 1; offset < instanceCount; ++offset) {
        const auto instance = firstInstance + offset;
        const auto value = opacityAt(instance);
        if (value == run.opacity) ++run.instanceCount;
        else {
            emit(run);
            run = {instance, 1, value};
        }
    }
    emit(run);
}
} // namespace azurerender
