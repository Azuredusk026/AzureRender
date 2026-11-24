#pragma once
#include <cmath>
#include <stdexcept>

namespace azurerender {
inline void validateMaterialRadianceScale(float value,bool multiplicative=false) {
    if(!std::isfinite(value)||value<0||value>16)
        throw std::invalid_argument("radianceScale requires a finite value within zero and sixteen");
    if(multiplicative&&value!=1)
        throw std::invalid_argument("radianceScale must be one for a multiplicative surface");
}
}
