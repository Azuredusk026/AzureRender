#pragma once
#include "assets/GltfLoader.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace azurerender {
inline void validateMaterialColorAdjustment(const AssetMaterial& material) {
    const auto& adjustment=material.colorAdjustment;
    if(!adjustment.present)return;
    const auto category=material.materialClass;
    const bool supportedClass=category==AssetMaterialClass::Generic||category==AssetMaterialClass::Skin
        ||category==AssetMaterialClass::Face||category==AssetMaterialClass::Fabric||category==AssetMaterialClass::Metal;
    constexpr auto incompatible=MaterialFeatureUnlitSurface|MaterialFeatureOverlay
        |MaterialFeatureMultiplyComposite|MaterialFeatureSceneRefraction;
    if(material.materialProfileVersion!=2||!supportedClass||material.unlitTextureLayers.present
        ||(material.materialFeatures&incompatible))
        throw std::invalid_argument("colorAdjustment requires a supported lit version-two material");
    const auto validGain=[](float value){return std::isfinite(value)&&value>=0.F&&value<=16.F;};
    if(!validGain(adjustment.brightness)||!validGain(adjustment.contrast)||!validGain(adjustment.saturation)
        ||!validGain(adjustment.rimIntensity)||!std::isfinite(adjustment.rimWidth)
        ||adjustment.rimWidth<0.F||adjustment.rimWidth>=1.F
        ||!std::all_of(adjustment.tint.begin(),adjustment.tint.end(),validGain)||adjustment.tint[3]>1.F
        ||!std::all_of(adjustment.rimColor.begin(),adjustment.rimColor.end(),validGain))
        throw std::invalid_argument("colorAdjustment values are outside the supported range");
}
}
