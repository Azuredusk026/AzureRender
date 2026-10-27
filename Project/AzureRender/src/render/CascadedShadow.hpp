#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace azurerender {

struct CascadeDepthFit {
    float eyeDistance;
    float nearPlane;
    float farPlane;
};

// Receiver XY stays fixed. Casters may lie one shadow-distance upstream
// of the receiver sphere, including geometry outside the camera slice.
inline CascadeDepthFit computeCascadeDepthFit(float radius, float casterDistance) {
    if (!std::isfinite(radius) || radius <= 0 || !std::isfinite(casterDistance) || casterDistance < 0)
        throw std::invalid_argument("Invalid cascade depth coverage");
    const float upstream = std::max(radius * 2.F, radius + casterDistance);
    const CascadeDepthFit result{upstream + .1F, .1F, upstream + radius * 2.1F};
    if (!std::isfinite(result.eyeDistance) || !std::isfinite(result.farPlane) || result.farPlane <= result.nearPlane)
        throw std::invalid_argument("Overflowing cascade depth coverage");
    return result;
}

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
