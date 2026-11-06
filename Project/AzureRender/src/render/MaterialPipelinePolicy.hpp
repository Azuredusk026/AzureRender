#pragma once
#include "assets/GltfLoader.hpp"
#include <cstdint>
#include <string>
#include <stdexcept>
namespace azurerender {
inline bool opaqueSurfaceOverlay(const AssetMaterial& material) noexcept {
    return material.unlitTextureLayers.present
        &&material.unlitTextureLayers.submission==UnlitTextureLayers::Submission::OpaqueOverlay;
}
// Matches shadow.frag priority: geometry coverage precedes alpha policies.
inline bool fullyCoveredShadow(const AssetMaterial& material,float nodeOpacity) noexcept {
    return nodeOpacity==1.F&&((material.materialFeatures&MaterialFeatureGeometryShadow)!=0
        ||(material.alphaMode==AssetAlphaMode::Opaque
            &&(material.materialFeatures&MaterialFeatureSqrtAlphaShadow)==0));
}
// character_depth.frag only clips opaque prefill through node opacity.
inline bool fullyCoveredPrefill(const AssetMaterial& material,float nodeOpacity) noexcept {
    return material.alphaMode==AssetAlphaMode::Opaque&&nodeOpacity==1.F;
}
// Cull policy and authored front-face orientation are independent.
inline constexpr unsigned surfaceFacingIndex(bool doubleSided,bool mirrored) noexcept {
    return (doubleSided?2U:0U)+(mirrored?1U:0U);
}
// Plain PBR with optional emission/neutral bits shares one bounded fast path.
inline bool ordinaryOpaqueMaterial(const AssetMaterial& material) noexcept {
    return material.materialClass==AssetMaterialClass::Generic&&material.alphaMode==AssetAlphaMode::Opaque
        &&(material.materialFeatures&~std::uint32_t{MaterialFeatureNeutralFallback|MaterialFeatureEmissiveMask})==0;
}
inline bool fullyCoveredOrdinaryMaterial(const AssetMaterial& material,float nodeOpacity) noexcept {
    return ordinaryOpaqueMaterial(material)&&nodeOpacity==1.F;
}
// mesh.frag clips opaque color only for node fade or screen-normal snapshots.
// Integral, composite and overlay programs retain their own submission rules.
inline bool fullyCoveredClassMaterial(const AssetMaterial& material,float nodeOpacity) noexcept {
    return material.alphaMode==AssetAlphaMode::Opaque&&nodeOpacity==1.F
        &&!material.nprDiffuse.screenLightingNormal&&!opaqueSurfaceOverlay(material)
        &&(material.materialFeatures&(MaterialFeatureHairPixelIntegral|MaterialFeatureSceneTint|MaterialFeatureMultiplyComposite))==0;
}
struct MaterialFeatureSpecialization {
    std::uint32_t fixedMask=0,fixedValue=0;
    [[nodiscard]] constexpr bool matches(std::uint32_t features) const noexcept {
        return (features&fixedMask)==fixedValue;
    }
};
// Empty sets (intersection=~0, union=0) retain the fully dynamic policy.
// The view's temporal sampling and coverage bits are never specialized.
[[nodiscard]] inline constexpr MaterialFeatureSpecialization fixedMaterialFeatures(
    std::uint32_t intersection,std::uint32_t combined) noexcept {
    const auto mask=~(intersection^combined)&std::uint32_t{0x3fffffffU};
    return {mask,intersection&mask};
}
inline bool integralMaterialProgram(const std::string& path) noexcept {
    const auto separator=path.find_last_of("/\\");
    return path.find("_integral",separator==std::string::npos?0:separator+1)!=std::string::npos;
}
inline bool extendedMaterialLighting(const AssetMaterial& material) noexcept {
    return material.environmentLighting.diffuse!=MaterialEnvironmentSource::Scene
        ||material.environmentLighting.reflection!=MaterialEnvironmentSource::Scene
        ||material.environmentLighting.fittedSpecular||material.environmentLighting.reflectionLayer.has_value()
        ||material.mainLightShadow!=MaterialMainLightShadow::Scene
        ||(material.nprDiffuse.present&&(material.nprDiffuse.rampComposition!=NprRampComposition::MaskVisibility
            ||material.nprDiffuse.rimResponse==AssetMaterial::NprDiffuse::RimResponse::FaceMasked));
}
inline std::string sceneFragmentVariant(std::string file,bool multisampled,bool clipmap,bool field=false,bool lighting=false,bool coverage=false,bool earlyDepth=false){
    auto at=file.rfind(".frag.spv");
    if(at==std::string::npos||at+9!=file.size())throw std::invalid_argument("Expected a fragment SPIR-V program");
    if(multisampled){file.insert(at,"_samples");at+=8;}
    if(clipmap){file.insert(at,"_clipmap");at+=8;}
    if(field){file.insert(at,"_field");at+=6;}
    if(earlyDepth){file.insert(at,"_early");at+=6;}
    if(lighting){file.insert(at,"_lighting");at+=9;}
    if(coverage)file.insert(at,"_coverage");
    return file;
}
}
