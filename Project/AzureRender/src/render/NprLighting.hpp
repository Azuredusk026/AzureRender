#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace azurerender {
// Scene-owned illumination for materials declaring the NPR diffuse response.
// The selected scene sun supplies main radiance; local lights remain separate.
struct NprLightingSettings {
    static constexpr unsigned kSchemaVersion=1;
    bool enabled=false,useSceneIrradiance=false;
    std::array<float,3> ambientColor{1,1,1},ambientDirection{0,1,0};
    float directionBias=0,directionGain=0,directionFloor=1;
    float outdoorGain=1,shadowAmbientGain=1,ambientWeight=1;
    float indoorMix=0,litAmbientMix=0,mainColorMix=0,shadowOverride=0;
};
inline void validateNprLighting(const NprLightingSettings& s){
    const auto range=[](float value,float minimum,float maximum){
        if(!std::isfinite(value)||value<minimum||value>maximum)
            throw std::invalid_argument("NPR lighting parameter outside legal domain");
    };
    float length2=0;
    for(float value:s.ambientColor)range(value,0,64);
    for(float value:s.ambientDirection){range(value,-1,1);length2+=value*value;}
    if(std::abs(length2-1)>1e-4F)throw std::invalid_argument("NPR ambient direction must be normalized");
    range(s.directionBias,-1,1);range(s.directionGain,0,16);range(s.directionFloor,0,16);
    range(s.outdoorGain,0,16);range(s.shadowAmbientGain,0,16);range(s.ambientWeight,0,16);
    for(float value:{s.indoorMix,s.litAmbientMix,s.mainColorMix,s.shadowOverride})range(value,0,1);
}
}
