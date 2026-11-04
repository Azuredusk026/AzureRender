#pragma once
#include "assets/GltfLoader.hpp"
#include <stdexcept>
namespace azurerender {
inline void validateMaterialRimResponse(const AssetMaterial& material) {
    const auto& npr=material.nprDiffuse;
    if(npr.radialLightingNormal&&(!npr.present||material.materialProfileVersion!=2||!npr.rimOrigin.present))
        throw std::invalid_argument("nprDiffuse.lightingNormal requires an explicit material rim origin in profile version two");
    if(npr.transparentHighlights&&(!npr.present||material.materialProfileVersion!=2||material.alphaMode!=AssetAlphaMode::Blend))
        throw std::invalid_argument("nprDiffuse.transparentHighlights requires a blended NPR surface in profile version two");
    const auto mode=npr.rimResponse;
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
