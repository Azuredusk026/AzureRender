#pragma once
#include "render/ReflectionProbeField.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace azurerender {
struct ReflectionLodMapping {
    enum class Kind { Linear,Logarithmic };
    Kind kind=Kind::Linear;
    float scale=1,bias=0,minimumRoughness=.001F;
    void validate() const {
        if((kind!=Kind::Linear&&kind!=Kind::Logarithmic)||!std::isfinite(scale)||scale<0||scale>32
            ||!std::isfinite(bias)||std::abs(bias)>32||!std::isfinite(minimumRoughness)
            ||minimumRoughness<=0||minimumRoughness>1)
            throw std::invalid_argument("Invalid reflection LOD mapping");
    }
    [[nodiscard]] std::array<float,3> shaderParameters() const {
        validate();return kind==Kind::Logarithmic?std::array<float,3>{scale,bias,minimumRoughness}:std::array<float,3>{};
    }
};
// One world-axis-aligned reflection volume. Positions use scene metres.
// The renderer owns its texture; an empty path disables this contribution.
struct ReflectionProbe {
    static constexpr int kSchemaVersion=4;
    std::string texture;
    std::array<float,3> center{0,0,0};
    std::array<float,3> halfExtents{1,1,1};
    std::array<float,3> capturePosition{0,0,0};
    float intensity=1;
    float blendDistance=0;
    bool boxProjection=true;
    // An omitted origin follows capturePosition when that position changes.
    std::optional<std::array<float,3>> projectionOrigin;
    std::optional<ReflectionLodMapping> lodMapping;
    std::optional<ReflectionProbeField> field;
    void validate() const {
        if(field)field->validate(256);
        if(lodMapping)lodMapping->validate();
        for(unsigned axis=0;axis<3;++axis)
            if(!std::isfinite(center[axis])||!std::isfinite(capturePosition[axis])
                ||!std::isfinite(halfExtents[axis])||halfExtents[axis]<=0
                ||(projectionOrigin&&!std::isfinite((*projectionOrigin)[axis])))
                throw std::invalid_argument("Invalid reflection probe volume");
        if(!std::isfinite(intensity)||intensity<0||!std::isfinite(blendDistance)||blendDistance<0)
            throw std::invalid_argument("Invalid reflection probe intensity or blend distance");
    }
    [[nodiscard]] float lod(float roughness,unsigned mipCount) const {
        if(!std::isfinite(roughness)||mipCount==0||mipCount>32)
            throw std::invalid_argument("Reflection LOD requires finite roughness and 1..32 levels");
        const auto r=std::clamp(roughness,0.F,1.F);const auto last=static_cast<float>(mipCount-1);
        if(!lodMapping)return r*last;
        lodMapping->validate();
        return lodMapping->kind==ReflectionLodMapping::Kind::Linear?r*last:
            std::clamp(lodMapping->scale*std::log2(std::max(r,lodMapping->minimumRoughness))+lodMapping->bias,0.F,last);
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
