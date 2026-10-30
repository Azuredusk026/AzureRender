#pragma once
#include "assets/GltfLoader.hpp"
#include <cstdint>
#include <string>
#include <stdexcept>
namespace azurerender {
// Cull policy and authored front-face orientation are independent.
inline constexpr unsigned surfaceFacingIndex(bool doubleSided,bool mirrored) noexcept {
    return (doubleSided?2U:0U)+(mirrored?1U:0U);
}
// Plain PBR with optional emission/neutral bits shares one bounded fast path.
inline bool ordinaryOpaqueMaterial(const AssetMaterial& material) noexcept {
    return material.materialClass==AssetMaterialClass::Generic&&material.alphaMode==AssetAlphaMode::Opaque
        &&(material.materialFeatures&~std::uint32_t{MaterialFeatureNeutralFallback|MaterialFeatureEmissiveMask})==0;
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
inline std::string sceneFragmentVariant(std::string file,bool multisampled,bool clipmap,bool field=false){
    auto at=file.rfind(".frag.spv");
    if(at==std::string::npos||at+9!=file.size())throw std::invalid_argument("Expected a fragment SPIR-V program");
    if(multisampled){file.insert(at,"_samples");at+=8;}
    if(clipmap){file.insert(at,"_clipmap");at+=8;}
    if(field)file.insert(at,"_field");
    return file;
}
}
