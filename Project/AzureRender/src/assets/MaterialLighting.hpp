#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace azurerender {
enum class MaterialEnvironmentSource : std::uint32_t { Scene, GlobalEnvironment };
enum class MaterialMainLightShadow : std::uint32_t { Scene, ObjectShadow };
struct MaterialEnvironmentLighting {
    MaterialEnvironmentSource diffuse=MaterialEnvironmentSource::Scene;
    MaterialEnvironmentSource reflection=MaterialEnvironmentSource::Scene;
    std::optional<float> globalIntensity;
};
inline unsigned materialEnvironmentFlags(const MaterialEnvironmentLighting& value) {
    const auto valid=[](auto source){return source==MaterialEnvironmentSource::Scene
        ||source==MaterialEnvironmentSource::GlobalEnvironment;};
    if(!valid(value.diffuse)||!valid(value.reflection))
        throw std::invalid_argument("environmentLighting requires scene or global-environment sources");
    return (value.diffuse==MaterialEnvironmentSource::GlobalEnvironment?1U:0U)
        |(value.reflection==MaterialEnvironmentSource::GlobalEnvironment?2U:0U);
}
inline void validateMaterialLighting(const MaterialEnvironmentLighting& environment,
    MaterialMainLightShadow shadow,unsigned profileVersion,bool lit,bool ordinaryLit) {
    const auto flags=materialEnvironmentFlags(environment);
    if(environment.globalIntensity) {
        const auto value=*environment.globalIntensity;
        if(!std::isfinite(value)||value<0||value>16||flags==0)
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
