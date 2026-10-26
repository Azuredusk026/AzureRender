#pragma once
#include "assets/GltfLoader.hpp"
namespace azurerender {
// Plain PBR with optional emission/neutral bits shares one bounded fast path.
inline bool ordinaryOpaqueMaterial(const AssetMaterial& material) noexcept {
    return material.materialClass==AssetMaterialClass::Generic&&material.alphaMode==AssetAlphaMode::Opaque
        &&(material.materialFeatures&~std::uint32_t{MaterialFeatureNeutralFallback|MaterialFeatureEmissiveMask})==0;
}
}
