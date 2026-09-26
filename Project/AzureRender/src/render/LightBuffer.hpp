#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <utility>

namespace azurerender {

struct RenderLight {
    std::uint64_t stableId = 0;
    float position[3]{};
    float color[3]{1.0F, 1.0F, 1.0F};
    float intensity = 1.0F;
    float radius = 1.0F;
};

class LightBuffer final {
public:
    void setLights(std::vector<RenderLight> lights) {
        std::sort(lights.begin(), lights.end(), [](const RenderLight& a, const RenderLight& b) {
            return a.stableId < b.stableId;
        });
        lights_ = std::move(lights);
    }
    [[nodiscard]] const std::vector<RenderLight>& lights() const noexcept { return lights_; }
private:
    std::vector<RenderLight> lights_;
};

}  // namespace azurerender
