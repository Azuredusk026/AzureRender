#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>

namespace azurerender {

struct RenderLight {
    std::uint64_t stableId = 0;
    float position[3]{};
    float clusterPosition[3]{};
    float color[3]{1.0F, 1.0F, 1.0F};
    float intensity = 1.0F;
    float radius = 1.0F;
};

struct RenderLightGpu {
    float positionRadius[4]{};
    float colorIntensity[4]{};
};

class LightBuffer final {
public:
    void setLights(std::vector<RenderLight> lights, std::size_t maxLights = 1024) {
        std::sort(lights.begin(), lights.end(), [](const RenderLight& a, const RenderLight& b) {
            return a.stableId < b.stableId;
        });
        if (lights.size() > maxLights) lights.resize(maxLights);
        lights_ = std::move(lights);
    }
    [[nodiscard]] const std::vector<RenderLight>& lights() const noexcept { return lights_; }
    [[nodiscard]] std::vector<RenderLightGpu> gpuData() const {
        std::vector<RenderLightGpu> packed;
        packed.reserve(lights_.size());
        for (const RenderLight& light : lights_) {
            RenderLightGpu value{};
            value.positionRadius[0] = light.position[0];
            value.positionRadius[1] = light.position[1];
            value.positionRadius[2] = light.position[2];
            value.positionRadius[3] = light.radius;
            value.colorIntensity[0] = light.color[0];
            value.colorIntensity[1] = light.color[1];
            value.colorIntensity[2] = light.color[2];
            value.colorIntensity[3] = light.intensity;
            packed.push_back(value);
        }
        return packed;
    }
private:
    std::vector<RenderLight> lights_;
};

}  // namespace azurerender
