#pragma once
#include "render/AerialFog.hpp"
#include <vector>
namespace azurerender {
struct LayeredHeightFog {
    std::vector<ExponentialHeightFog> layers;
    float startViewDepth=0,minimumTransmittance=0;
    std::array<float,4> transmittanceBias{};
    void validate() const;
};
struct AnalyticFogSettings {
    std::optional<ExponentialAerialFog> aerial;
    std::optional<LayeredHeightFog> height;
    void validate() const;
};
FogTransport evaluateAnalyticFog(const AnalyticFogSettings&,std::array<float,3> rayStart,
    std::array<float,3> end,std::array<float,3> viewForward);
}
