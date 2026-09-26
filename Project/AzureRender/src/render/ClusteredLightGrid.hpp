#pragma once

#include "render/LightBuffer.hpp"

#include <algorithm>
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
            const auto axis = [](float value, std::uint32_t count) {
                return std::min<std::uint32_t>(
                    count - 1,
                    static_cast<std::uint32_t>(std::clamp((value + 1.0F) * 0.5F
                        * static_cast<float>(count), 0.0F,
                        static_cast<float>(count - 1))));
            };
            const std::uint32_t x = axis(light.position[0], desc_.x);
            const std::uint32_t y = axis(light.position[1], desc_.y);
            const float logarithmic = std::log(std::max(light.position[2], desc_.nearPlane)
                / desc_.nearPlane) / std::log(desc_.farPlane / desc_.nearPlane);
            const std::uint32_t z = std::min<std::uint32_t>(
                desc_.z - 1,
                static_cast<std::uint32_t>(std::clamp(logarithmic, 0.0F, 0.999999F)
                    * static_cast<float>(desc_.z)));
            clusters_[linearIndex(x, y, z)].push_back(static_cast<std::uint32_t>(index));
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

private:
    [[nodiscard]] std::size_t linearIndex(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
        return (static_cast<std::size_t>(z) * desc_.y + y) * desc_.x + x;
    }
    ClusterGridDesc desc_;
    std::vector<std::vector<std::uint32_t>> clusters_;
};

}  // namespace azurerender
