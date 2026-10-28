#pragma once
#include "render/FogTransport.hpp"
#include <optional>
namespace azurerender {
struct alignas(16) PackedAerialFog {
    static constexpr unsigned layoutVersion=1;
    std::array<std::array<float,4>,6> rows{};
};
// Exponential optical density and directional Rayleigh/Mie source radiance.
// Optional minimumHeightDelta controls the dimensionless endpoint integral.
struct ExponentialAerialFog {
    float density=0,heightFalloff=0,referenceHeight=0,startDistance=0;
    std::array<float,3> extinction{},rayleighColor{},mieColor{},ambientColor{};
    std::array<float,3> sunDirection{0,1,0};
    float anisotropy=0,radianceScale=1,phaseDenominatorFloor=.001F;
    std::optional<float> minimumHeightDelta;
    void validate() const;
    PackedAerialFog pack() const;
};
FogTransport evaluateAerialFog(const ExponentialAerialFog&,std::array<float,3> start,std::array<float,3> end);
}
