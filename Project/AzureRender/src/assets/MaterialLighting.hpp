#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace azurerender {
enum class MaterialEnvironmentSource : std::uint32_t { Scene, GlobalEnvironment };
enum class MaterialMainLightShadow : std::uint32_t { Scene, ObjectShadow };
enum class NprRampComposition : std::uint32_t { MaskVisibility, ViewRampAlpha, ViewFacingMask };
inline void validateNprRampComposition(NprRampComposition policy,bool supportedNpr,unsigned profileVersion,bool diffuseRamp) {
    if(policy!=NprRampComposition::MaskVisibility&&policy!=NprRampComposition::ViewRampAlpha&&policy!=NprRampComposition::ViewFacingMask)
        throw std::invalid_argument("Invalid nprDiffuse.rampComposition policy");
    if(policy!=NprRampComposition::MaskVisibility&&(!supportedNpr||profileVersion!=2||!diffuseRamp))
        throw std::invalid_argument("nprDiffuse.rampComposition requires a lit NPR surface and diffuse-ramp profile version two");
}
inline void validateNprShadowSaturation(float value,bool supportedNpr,unsigned profileVersion) {
    if(!std::isfinite(value)||value<0||value>4||(value!=1&&(!supportedNpr||profileVersion!=2)))
        throw std::invalid_argument("nprDiffuse.shadowSaturation requires zero through four and a lit version-two NPR owner");
}
struct MaterialEnvironmentLighting {
    MaterialEnvironmentSource diffuse=MaterialEnvironmentSource::Scene;
    MaterialEnvironmentSource reflection=MaterialEnvironmentSource::Scene;
    std::optional<float> globalIntensity;
    bool fittedSpecular=false;
    std::optional<unsigned> reflectionLayer;
};
inline unsigned materialEnvironmentFlags(const MaterialEnvironmentLighting& value) {
    const auto valid=[](auto source){return source==MaterialEnvironmentSource::Scene
        ||source==MaterialEnvironmentSource::GlobalEnvironment;};
    if(!valid(value.diffuse)||!valid(value.reflection))
        throw std::invalid_argument("environmentLighting requires scene or global-environment sources");
    if(value.reflectionLayer&&(*value.reflectionLayer>=256||value.reflection!=MaterialEnvironmentSource::Scene))
        throw std::invalid_argument("environmentLighting.reflectionLayer requires a scene reflection source and a layer within zero through 255");
    return (value.diffuse==MaterialEnvironmentSource::GlobalEnvironment?1U:0U)
        |(value.reflection==MaterialEnvironmentSource::GlobalEnvironment?2U:0U)
        |(value.fittedSpecular?4U:0U)
        |(value.reflectionLayer?((*value.reflectionLayer+1U)<<3U):0U);
}
inline void validateMaterialLighting(const MaterialEnvironmentLighting& environment,
    MaterialMainLightShadow shadow,unsigned profileVersion,bool lit,bool ordinaryLit,bool microfacetClass) {
    const auto flags=materialEnvironmentFlags(environment);
    if(environment.fittedSpecular&&!microfacetClass)
        throw std::invalid_argument("environmentLighting.specularModel requires a lit generic, fabric or metal surface");
    if(environment.globalIntensity) {
        const auto value=*environment.globalIntensity;
        if(!std::isfinite(value)||value<0||value>16||(flags&3U)==0)
            throw std::invalid_argument("environmentLighting globalIntensity requires a global source and a finite value within zero and sixteen");
    }
    if(flags&&(profileVersion!=2||!lit))
        throw std::invalid_argument("environmentLighting requires a lit profile version two");
    if(shadow!=MaterialMainLightShadow::Scene&&shadow!=MaterialMainLightShadow::ObjectShadow)
        throw std::invalid_argument("mainLightShadow requires scene or object-shadow");
    if(shadow==MaterialMainLightShadow::ObjectShadow&&(profileVersion!=2||!lit||!ordinaryLit))
        throw std::invalid_argument("mainLightShadow object-shadow requires an ordinary lit generic, fabric or metal profile version two");
}
}
