#pragma once
#include "assets/GltfLoader.hpp"
namespace azurerender {
// Features and parameters remain dynamic, including uncommon Generic flags.
inline bool ordinaryOpaqueMaterial(const AssetMaterial& material) noexcept {
    return material.materialClass==AssetMaterialClass::Generic&&material.alphaMode==AssetAlphaMode::Opaque;
}
}
