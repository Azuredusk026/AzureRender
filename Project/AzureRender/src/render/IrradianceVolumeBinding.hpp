#pragma once
#include <cmath>
#include <stdexcept>
#include <string>
#include "render/IrradianceQuery.hpp"
namespace azurerender {
struct IrradianceVolumeBinding {
    std::string asset;
    float intensity=1;
    bool enabled=false;
    IrradianceQueryParameters sampling;
    void validate() const {
        sampling.validate();
        if(!std::isfinite(intensity)||intensity<0||(enabled&&asset.empty()))
            throw std::invalid_argument("Invalid irradiance volume binding");
    }
};
}
