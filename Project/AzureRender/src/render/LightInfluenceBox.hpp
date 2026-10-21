#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace azurerender {
// Explicit world-space lighting volume, independently anchored from its emitter.
// Rows map world positions into a unit box with boundaries at -1 and +1.
struct LightInfluenceBox {
    bool enabled = false;
    std::array<std::array<float,4>,3> worldToBox{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    float falloffThreshold = .8F;

    void validate() const {
        for(const auto& row:worldToBox)for(float value:row)
            if(!std::isfinite(value))throw std::invalid_argument("Non-finite light influence transform");
        if(!std::isfinite(falloffThreshold)||falloffThreshold<0||falloffThreshold>1)
            throw std::invalid_argument("Light influence threshold must be in [0,1]");
        const auto& m=worldToBox;
        const float determinant=m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])
            -m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])
            +m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
        if(enabled&&(!std::isfinite(determinant)||determinant==0))
            throw std::invalid_argument("Singular light influence transform");
    }
    [[nodiscard]] float weight(const std::array<float,3>& position) const {
        if(!enabled)return 1;
        float edge=0;
        for(const auto& row:worldToBox)
            edge=std::max(edge,std::abs(row[0]*position[0]+row[1]*position[1]+row[2]*position[2]+row[3]));
        if(falloffThreshold==1)return edge<1?1.F:0.F;
        const float inner=(1+falloffThreshold)*.5F;
        const float value=1-std::clamp((edge-inner)/(1-inner),0.F,1.F);
        return value*value;
    }
};
}
