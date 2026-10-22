#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace azurerender {
// One world-axis-aligned reflection volume. Positions use scene metres.
// The renderer owns its texture; an empty path disables this contribution.
struct ReflectionProbe {
    static constexpr int kSchemaVersion=2;
    std::string texture;
    std::array<float,3> center{0,0,0};
    std::array<float,3> halfExtents{1,1,1};
    std::array<float,3> capturePosition{0,0,0};
    float intensity=1;
    float blendDistance=0;
    bool boxProjection=true;
    // An omitted origin follows capturePosition when that position changes.
    std::optional<std::array<float,3>> projectionOrigin;
    void validate() const {
        for(unsigned axis=0;axis<3;++axis)
            if(!std::isfinite(center[axis])||!std::isfinite(capturePosition[axis])
                ||!std::isfinite(halfExtents[axis])||halfExtents[axis]<=0
                ||(projectionOrigin&&!std::isfinite((*projectionOrigin)[axis])))
                throw std::invalid_argument("Invalid reflection probe volume");
        if(!std::isfinite(intensity)||intensity<0||!std::isfinite(blendDistance)||blendDistance<0)
            throw std::invalid_argument("Invalid reflection probe intensity or blend distance");
    }
    [[nodiscard]] float weight(const std::array<float,3>& position) const {
        if(texture.empty())return 0;
        float distance=std::numeric_limits<float>::max();
        for(unsigned axis=0;axis<3;++axis)
            distance=std::min(distance,halfExtents[axis]-std::abs(position[axis]-center[axis]));
        if(distance<=0)return 0;
        return blendDistance>0?std::clamp(distance/blendDistance,0.F,1.F):1.F;
    }
    [[nodiscard]] std::array<float,3> project(const std::array<float,3>& position,
        const std::array<float,3>& direction) const {
        if(!boxProjection)return direction;
        float exit=std::numeric_limits<float>::max();
        for(unsigned axis=0;axis<3;++axis)if(std::abs(direction[axis])>1e-8F){
            const auto face=center[axis]+(direction[axis]>0?halfExtents[axis]:-halfExtents[axis]);
            exit=std::min(exit,(face-position[axis])/direction[axis]);
        }
        if(exit<0||exit==std::numeric_limits<float>::max())return direction;
        const auto& origin=projectionOrigin?*projectionOrigin:capturePosition;
        std::array<float,3> result{};
        for(unsigned axis=0;axis<3;++axis)result[axis]=position[axis]+direction[axis]*exit-origin[axis];
        return result;
    }
};
}
