#pragma once
#include "assets/GltfLoader.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace azurerender {
inline void validateMaterialOutputPrecision(const AssetMaterial& material) {
    if((material.materialFeatures&MaterialFeatureR11G11B10Output)==0)return;
    if(material.materialProfileVersion!=2
        ||(material.alphaMode!=AssetAlphaMode::Opaque&&material.alphaMode!=AssetAlphaMode::Mask)
        ||material.unlitTextureLayers.present||material.materialClass==AssetMaterialClass::Overlay
        ||(material.materialFeatures&(MaterialFeatureUnlitSurface|MaterialFeatureOverlay|MaterialFeatureMultiplyComposite|MaterialFeatureSceneTint|MaterialFeatureUnlitTextureLayers)))
        throw std::invalid_argument("r11g11b10-output requires a version-two lit opaque or masked material");
}
inline float quantizeUnsignedMaterialFloat(float value,unsigned mantissa) {
    if(mantissa!=5&&mantissa!=6)throw std::invalid_argument("Unsigned output precision requires five or six mantissa bits");
    std::uint32_t word;std::memcpy(&word,&value,4);const auto magnitude=word&0x7fffffffU;
    const auto number=[](std::uint32_t bits){float result;std::memcpy(&result,&bits,4);return result;};
    if(magnitude>0x7f800000U)return number(0x7fc00000U);
    if(word&0x80000000U)return 0;
    if(magnitude==0x7f800000U)return value;
    const auto maximum=mantissa==6?0x477e0000U:0x477c0000U;
    if(magnitude>maximum)return number(maximum);
    const auto exponent=magnitude>>23;
    if(exponent<113U){
        if(exponent<113U-mantissa)return 0;
        const auto significand=(magnitude&0x7fffffU)|0x800000U;
        const auto count=significand>>(23U+113U-exponent-mantissa);
        return std::ldexp(float(count),-14-int(mantissa));
    }
    return number(magnitude&~((1U<<(23U-mantissa))-1U));
}
inline std::array<float,4> quantizeMaterialOutputColor(std::array<float,4> color) {
    color[0]=quantizeUnsignedMaterialFloat(color[0],6);
    color[1]=quantizeUnsignedMaterialFloat(color[1],6);
    color[2]=quantizeUnsignedMaterialFloat(color[2],5);
    return color;
}
}
