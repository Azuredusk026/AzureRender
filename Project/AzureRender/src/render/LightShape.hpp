#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace azurerender {
enum class LightKind { Point = 0, Spot = 1 };

// Cone angles are half angles in radians. Direction points away from the emitter.
// Scene documents store local direction; render snapshots store world direction.
struct LightShape {
    LightKind type = LightKind::Point;
    std::array<float, 3> direction{0, 0, -1};
    float innerCone = 0;
    float outerCone = .785398163F;

    void validate() const {
        if (type != LightKind::Point && type != LightKind::Spot)
            throw std::invalid_argument("Unknown light type");
        float length = 0;
        for (const float value : direction) {
            if (!std::isfinite(value)) throw std::invalid_argument("Non-finite light direction");
            length += value * value;
        }
        if (std::abs(length - 1.F) > 1e-4F)
            throw std::invalid_argument("Light direction must be normalized");
        if (!std::isfinite(innerCone) || !std::isfinite(outerCone)
            || innerCone < 0 || outerCone <= 0 || outerCone < innerCone || outerCone > 1.570796327F)
            throw std::invalid_argument("Invalid light cone half angles");
    }
    float angularWeight(const std::array<float, 3>& emitterToSurface) const {
        if (type == LightKind::Point) return 1;
        const float cosine = direction[0]*emitterToSurface[0]
            + direction[1]*emitterToSurface[1] + direction[2]*emitterToSurface[2];
        if(innerCone==outerCone) return cosine>=std::cos(outerCone)?1.F:0.F;
        const float t = std::clamp((cosine-std::cos(outerCone))
            / (std::cos(innerCone)-std::cos(outerCone)), 0.F, 1.F);
        return t*t;
    }
};
}
