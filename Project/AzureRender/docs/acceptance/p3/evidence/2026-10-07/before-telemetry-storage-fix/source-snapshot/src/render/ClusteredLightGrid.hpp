#pragma once

#include "render/LightBuffer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace azurerender {

struct ClusterGridDesc {
    std::uint32_t x = 1;
    std::uint32_t y = 1;
    std::uint32_t z = 1;
    float nearPlane = 0.1F;
    float farPlane = 100.0F;
    float projectionScaleX = 1.0F;
    float projectionScaleY = 1.0F;
};

class ClusteredLightGrid final {
public:
    explicit ClusteredLightGrid(ClusterGridDesc desc) : desc_(desc) {
        if (desc_.x == 0 || desc_.y == 0 || desc_.z == 0
            || !(desc_.nearPlane > 0.0F) || !(desc_.farPlane > desc_.nearPlane)) {
            throw std::invalid_argument("Invalid clustered light grid");
        }
        clusters_.resize(clusterCount());
    }

    void assign(const std::vector<RenderLight>& lights) {
        for (auto& cluster : clusters_) cluster.clear();
        for (std::size_t index = 0; index < lights.size(); ++index) {
            const RenderLight& light = lights[index];
            const float radius = std::max(light.radius, 0.0F);
            const float centerDepth = light.clusterPosition[2];
            const float minimumDepth = std::max(
                desc_.nearPlane, centerDepth - radius);
            const float maximumDepth = std::min(
                desc_.farPlane, centerDepth + radius);
            if (!(radius > 0.0F) || maximumDepth < minimumDepth
                || centerDepth + radius < desc_.nearPlane
                || centerDepth - radius > desc_.farPlane) {
                continue;
            }
            const auto axis = [](const float value, const std::uint32_t count) {
                return std::min<std::uint32_t>(
                    count - 1,
                    static_cast<std::uint32_t>(std::clamp((value + 1.0F) * 0.5F
                        * static_cast<float>(count), 0.0F,
                        static_cast<float>(count - 1))));
            };
            const float minimumX = light.clusterPosition[0]
                - desc_.projectionScaleX * radius / minimumDepth;
            const float maximumX = light.clusterPosition[0]
                + desc_.projectionScaleX * radius / minimumDepth;
            const float minimumY = light.clusterPosition[1]
                - desc_.projectionScaleY * radius / minimumDepth;
            const float maximumY = light.clusterPosition[1]
                + desc_.projectionScaleY * radius / minimumDepth;
            if (maximumX < -1.0F || minimumX > 1.0F
                || maximumY < -1.0F || minimumY > 1.0F) {
                continue;
            }
            const std::uint32_t firstX = axis(minimumX, desc_.x);
            const std::uint32_t lastX = axis(maximumX, desc_.x);
            const std::uint32_t firstY = axis(minimumY, desc_.y);
            const std::uint32_t lastY = axis(maximumY, desc_.y);
            const auto depthSlice = [this](const float depth) {
                const float logarithmic = std::log(
                    std::max(depth, desc_.nearPlane) / desc_.nearPlane)
                    / std::log(desc_.farPlane / desc_.nearPlane);
                return std::min<std::uint32_t>(
                    desc_.z - 1,
                    static_cast<std::uint32_t>(std::clamp(
                        logarithmic, 0.0F, 0.999999F)
                        * static_cast<float>(desc_.z)));
            };
            const std::uint32_t firstZ = depthSlice(minimumDepth);
            const std::uint32_t lastZ = depthSlice(maximumDepth);
            for (std::uint32_t z = firstZ; z <= lastZ; ++z) {
                for (std::uint32_t y = firstY; y <= lastY; ++y) {
                    for (std::uint32_t x = firstX; x <= lastX; ++x) {
                        clusters_[linearIndex(x, y, z)].push_back(
                            static_cast<std::uint32_t>(index));
                    }
                }
            }
        }
    }

    [[nodiscard]] std::size_t clusterCount() const noexcept {
        return static_cast<std::size_t>(desc_.x) * desc_.y * desc_.z;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& indicesFor(
        std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
        if (x >= desc_.x || y >= desc_.y || z >= desc_.z) throw std::out_of_range("Cluster index");
        return clusters_[linearIndex(x, y, z)];
    }
    [[nodiscard]] std::vector<std::uint32_t> gpuIndexData() const {
        std::vector<std::uint32_t> result;
        for (const auto& cluster : clusters_) result.insert(result.end(), cluster.begin(), cluster.end());
        return result;
    }
    [[nodiscard]] std::vector<std::array<std::uint32_t, 2>> gpuHeaderData() const {
        std::vector<std::array<std::uint32_t, 2>> result;
        result.reserve(clusters_.size());
        std::uint32_t offset = 0;
        for (const auto& cluster : clusters_) {
            result.push_back({offset, static_cast<std::uint32_t>(cluster.size())});
            offset += static_cast<std::uint32_t>(cluster.size());
        }
        return result;
    }
    [[nodiscard]] const ClusterGridDesc& desc() const noexcept { return desc_; }

private:
    [[nodiscard]] std::size_t linearIndex(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
        return (static_cast<std::size_t>(z) * desc_.y + y) * desc_.x + x;
    }
    ClusterGridDesc desc_;
    std::vector<std::vector<std::uint32_t>> clusters_;
};

}  // namespace azurerender
