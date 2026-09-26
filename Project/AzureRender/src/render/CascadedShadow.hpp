#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace azurerender {

inline std::vector<float> computeCascadeSplits(float nearPlane,
                                               float farPlane,
                                               std::size_t cascadeCount,
                                               float logarithmicWeight) {
    if (!(nearPlane > 0.0F) || !(farPlane > nearPlane) || cascadeCount == 0
        || logarithmicWeight < 0.0F || logarithmicWeight > 1.0F) {
        throw std::invalid_argument("Invalid cascade split parameters");
    }
    std::vector<float> splits;
    splits.reserve(cascadeCount);
    for (std::size_t i = 1; i <= cascadeCount; ++i) {
        const float fraction = static_cast<float>(i) / static_cast<float>(cascadeCount);
        const float logarithmic = nearPlane * std::pow(farPlane / nearPlane, fraction);
        const float uniform = nearPlane + (farPlane - nearPlane) * fraction;
        splits.push_back(std::max(logarithmicWeight * logarithmic
                                      + (1.0F - logarithmicWeight) * uniform,
                                  nearPlane));
    }
    splits.back() = farPlane;
    return splits;
}

}  // namespace azurerender
