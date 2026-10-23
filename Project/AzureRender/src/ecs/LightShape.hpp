#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include "ecs/LightInfluenceBox.hpp"
#include "ecs/LightReceivers.hpp"

namespace azurerender {
enum class LightKind { Point = 0, Spot = 1, Directional = 2 };
enum class LightFalloff { Legacy = 0, SmoothInverseSquare = 1, RangePower = 2 };

// Cone angles are half angles in radians. Direction points away from the emitter.
// Scene documents store local direction; render snapshots store world direction.
struct LightShape {
    LightKind type = LightKind::Point;
    std::array<float, 3> direction{0, 0, -1};
    float innerCone = 0;
    float outerCone = .785398163F;
    LightFalloff falloff = LightFalloff::Legacy;
    float falloffExponent = 2;
    float specularScale = 1;
    LightInfluenceBox influenceBox;
    LightReceivers receivers;
    struct CharacterResponse {
        bool present=false;
        float diffuseFloor=0,compression=0;
        float linearLength=0;
        unsigned schemaVersion=1;
        void validate() const {
            if((schemaVersion!=1&&schemaVersion!=2)||!std::isfinite(linearLength)||linearLength<0||linearLength>65536
                ||(schemaVersion==1&&linearLength!=0))throw std::invalid_argument("Invalid character line-light version or length");
            for(float value:{diffuseFloor,compression})if(!std::isfinite(value)||value<0||value>1)
                throw std::invalid_argument("Character light response values must be in [0,1]");
        }
    } characterResponse;

    void validate() const {
        influenceBox.validate();
        characterResponse.validate();
        if(characterResponse.present&&type==LightKind::Directional)
            throw std::invalid_argument("Character local-light response requires a punctual emitter");
        if(characterResponse.linearLength>0&&(type!=LightKind::Point||falloff==LightFalloff::Legacy))
            throw std::invalid_argument("Character line emitter requires a point shape and explicit distance model");
        if (!std::isfinite(specularScale) || specularScale<0)
            throw std::invalid_argument("Light specular scale must be finite and nonnegative");
        if (type != LightKind::Point && type != LightKind::Spot && type != LightKind::Directional)
            throw std::invalid_argument("Unknown light type");
        if (falloff != LightFalloff::Legacy && falloff != LightFalloff::SmoothInverseSquare
            && falloff != LightFalloff::RangePower)
            throw std::invalid_argument("Unknown light falloff");
        if (!std::isfinite(falloffExponent) || falloffExponent<=0 || falloffExponent>64)
            throw std::invalid_argument("Light falloff exponent must be in (0,64]");
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
        if (type != LightKind::Spot) return 1;
        const float cosine = direction[0]*emitterToSurface[0]
            + direction[1]*emitterToSurface[1] + direction[2]*emitterToSurface[2];
        if(innerCone==outerCone) return cosine>=std::cos(outerCone)?1.F:0.F;
        const float t = std::clamp((cosine-std::cos(outerCone))
            / (std::cos(innerCone)-std::cos(outerCone)), 0.F, 1.F);
        return t*t;
    }
};
}
