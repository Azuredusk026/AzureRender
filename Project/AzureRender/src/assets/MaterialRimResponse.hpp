#pragma once
#include "assets/GltfLoader.hpp"
#include <stdexcept>
namespace azurerender {
inline void validateMaterialRimResponse(const AssetMaterial& material) {
    const auto mode=material.nprDiffuse.rimResponse;
    if(mode!=AssetMaterial::NprDiffuse::RimResponse::ScreenDepth
        &&mode!=AssetMaterial::NprDiffuse::RimResponse::ViewFacing
        &&mode!=AssetMaterial::NprDiffuse::RimResponse::FaceMasked)
        throw std::invalid_argument("Invalid nprDiffuse.rimResponse");
    if(mode==AssetMaterial::NprDiffuse::RimResponse::FaceMasked
        &&(!material.nprDiffuse.present||material.materialProfileVersion!=2||material.materialClass!=AssetMaterialClass::Face
            ||!material.faceSdf.present||material.faceSdf.schemaVersion!=2
            ||!(material.materialFeatures&MaterialFeatureDualFaceSdf)
            ||!(material.materialFeatures&MaterialFeatureFaceSdfEligible)
            ||!(material.materialFeatures&MaterialFeatureNprDiffuse)
            ||(material.materialFeatures&(MaterialFeatureUnlitSurface|MaterialFeatureMultiplyComposite))))
        throw std::invalid_argument("nprDiffuse.rimResponse face-masked requires a lit version-two dual face surface");
}
}
