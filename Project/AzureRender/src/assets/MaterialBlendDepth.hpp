#pragma once
#include "assets/GltfLoader.hpp"
#include <stdexcept>
namespace azurerender {
inline void validateMaterialBlendDepth(const AssetMaterial& material) {
    const auto& depth=material.blendDepth;
    if(!depth.present) {
        if(depth.write||depth.compare!=AssetMaterial::BlendDepth::Compare::Less)
            throw std::invalid_argument("blendDepth values require explicit ownership");
        return;
    }
    if(material.materialProfileVersion!=2||material.alphaMode!=AssetAlphaMode::Blend
        ||material.unlitTextureLayers.present||material.materialClass==AssetMaterialClass::Overlay
        ||(material.materialFeatures&(MaterialFeatureUnlitSurface|MaterialFeatureOverlay|MaterialFeatureMultiplyComposite|MaterialFeatureSceneTint))
        ||(depth.compare!=AssetMaterial::BlendDepth::Compare::Less&&depth.compare!=AssetMaterial::BlendDepth::Compare::LessOrEqual))
        throw std::invalid_argument("blendDepth requires a version-two lit transparent material");
}
}
