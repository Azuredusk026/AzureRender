#pragma once
#include "assets/GltfLoader.hpp"
#include <stdexcept>
namespace azurerender {
inline void validateMaterialUvPolicy(const AssetMaterial& m){
    if((m.materialFeatures&MaterialFeatureMirroredCoarseUv)==0)return;
    constexpr std::uint32_t incompatible=MaterialFeatureHairPixelIntegral|MaterialFeatureUnlitSurface|MaterialFeatureUnlitTextureLayers
        |MaterialFeatureEyeRadialSurface|MaterialFeatureSceneRefraction|MaterialFeatureSceneTint|MaterialFeatureMultiplyComposite|MaterialFeatureOverlay;
    if(static_cast<std::uint32_t>(m.materialClass)>static_cast<std::uint32_t>(AssetMaterialClass::Showcase)
        ||m.materialProfileVersion!=2||(m.alphaMode!=AssetAlphaMode::Opaque&&m.alphaMode!=AssetAlphaMode::Mask)
        ||m.unlitTextureLayers.present||m.materialClass==AssetMaterialClass::Overlay||(m.materialFeatures&incompatible))
        throw std::invalid_argument("mirrored-coarse-uv requires a version-two lit opaque or masked primary-UV material");
}
}
