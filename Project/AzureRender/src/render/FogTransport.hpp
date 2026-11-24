#pragma once
#include <array>

namespace azurerender {
// Linear scattered radiance and per-channel optical transmittance.
struct FogTransport {
    std::array<float,3> scattering{};
    std::array<float,3> transmittance{1,1,1};
};
struct ExponentialHeightFog {
    float density=0;
    float heightFalloff=0;
    float referenceHeight=0;
    std::array<float,3> scatteringColor{};
};
void validateFogTransport(const FogTransport&);
// Front lies between the camera and the independently integrated back segment.
FogTransport composeFogTransport(const FogTransport& front,const FogTransport& back);
// Additive emitters use zero scattering weight; alpha surfaces use one.
std::array<float,3> applyFogTransport(const FogTransport&,std::array<float,3> linearColor,float scatteringWeight=1);
FogTransport evaluateHeightFog(const ExponentialHeightFog&,std::array<float,3> start,std::array<float,3> end);
}
