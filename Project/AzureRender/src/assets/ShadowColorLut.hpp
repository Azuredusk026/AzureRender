#pragma once
#include <cstdint>
#include <stdexcept>
namespace azurerender {
inline void validateShadowColorLutDimensions(std::uint32_t width,std::uint32_t height) {
    if(height<2||height>64||width!=height*height)
        throw std::invalid_argument("shadowColorLut requires a horizontal cube strip with height 2..64 and width height squared");
}
}
