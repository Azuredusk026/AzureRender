#pragma once
#include "assets/GltfLoader.hpp"
#include "scene/SceneDescription.hpp"
#include <algorithm>
#include <cmath>

namespace azurerender {
inline scene::AxisAlignedBounds expandBounds(scene::AxisAlignedBounds bounds, float margin) {
    for (std::size_t i = 0; i < 3; ++i) {
        bounds.minimum[i] -= margin;
        bounds.maximum[i] += margin;
    }
    return bounds;
}
inline float materialDisplacementMargin(const LoadedAsset& asset) {
    float margin = 0.0F;
    for (const auto& material : asset.materials) {
        if ((material.materialFeatures & MaterialFeatureBrowOverlay) != 0)
            margin = std::max(margin, std::abs(material.featureParameters[0])
                + std::abs(std::min(material.styleParameters[0], 0.0012F)));
    }
    // The vertex fallback applies brow thickness before skinning. A row-sum
    // bound covers arbitrary affine joint scale/shear; retain post-skin size.
    float amplification = 1.0F;
    for (const auto& matrix : asset.jointMatrices)
        for (std::size_t row = 0; row < 3; ++row)
            amplification = std::max(amplification,
                std::abs(matrix[row]) + std::abs(matrix[4 + row]) + std::abs(matrix[8 + row]));
    return margin * amplification;
}
inline scene::AxisAlignedBounds includeTransformedBounds(
    scene::AxisAlignedBounds bounds, const internal::Matrix4& transform) {
    const auto transformed = scene::transformBounds(bounds, transform);
    for (std::size_t i = 0; i < 3; ++i) {
        bounds.minimum[i] = std::min(bounds.minimum[i], transformed.minimum[i]);
        bounds.maximum[i] = std::max(bounds.maximum[i], transformed.maximum[i]);
    }
    return bounds;
}
inline scene::AxisAlignedBounds morphBounds(const LoadedAsset& asset,
                                            const std::array<float, 2>& weights) {
    scene::AxisAlignedBounds bounds{asset.boundsMin, asset.boundsMax};
    for (const auto& vertex : asset.vertices) {
        std::array<float, 3> position{};
        for (std::size_t i = 0; i < 3; ++i)
            position[i] = vertex.position[i] + vertex.morph0[i] * weights[0]
                + vertex.morph1[i] * weights[1];
        if (asset.hasSkin && !asset.jointMatrices.empty()) {
            const auto morphed = position;
            position = {};
            for (std::size_t joint = 0; joint < 4; ++joint) {
                const auto& matrix = asset.jointMatrices.at(vertex.joints[joint]);
                for (std::size_t i = 0; i < 3; ++i)
                    position[i] += vertex.weights[joint] * (matrix[i] * morphed[0]
                        + matrix[4 + i] * morphed[1] + matrix[8 + i] * morphed[2] + matrix[12 + i]);
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            bounds.minimum[i] = std::min(bounds.minimum[i], position[i]);
            bounds.maximum[i] = std::max(bounds.maximum[i], position[i]);
        }
    }
    return bounds;
}
}  // namespace azurerender
