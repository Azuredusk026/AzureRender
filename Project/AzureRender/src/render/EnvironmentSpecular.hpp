#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace azurerender {
enum class EnvironmentSpecularModel : std::uint32_t {
    LegacySchlick=0,
    GgxMultiscatterFit=1,
};
[[nodiscard]] inline std::string_view environmentSpecularModelName(EnvironmentSpecularModel model) noexcept {
    switch(model){
    case EnvironmentSpecularModel::LegacySchlick:return "legacy-schlick";
    case EnvironmentSpecularModel::GgxMultiscatterFit:return "ggx-multiscatter-fit";
    default:return "unknown";
    }
}
[[nodiscard]] inline EnvironmentSpecularModel environmentSpecularModelFromString(std::string_view name) {
    if(name=="legacy-schlick")return EnvironmentSpecularModel::LegacySchlick;
    if(name=="ggx-multiscatter-fit")return EnvironmentSpecularModel::GgxMultiscatterFit;
    throw std::invalid_argument("Unknown environment specular model");
}
// Linear, unit-range reflectance and perceptual roughness. This CPU path also
// supports offline material analysis independently from the scene renderer.
[[nodiscard]] inline std::array<float,3> ggxMultiscatterEnvironment(
    const std::array<float,3>& f0,float roughness,float ndotv) {
    const auto legal=[](float value){return std::isfinite(value)&&value>=0&&value<=1;};
    if(!legal(roughness)||!legal(ndotv))throw std::invalid_argument("Invalid environment specular input");
    for(float value:f0)if(!legal(value))throw std::invalid_argument("Invalid environment specular reflectance");
    const float r2=roughness*roughness,r6=r2*r2*r2,v2=ndotv*ndotv,v3=v2*ndotv;
    const float a=(3.327070F*ndotv+.036546F+(-9.047560F*ndotv+9.063200F)*r2)
        /(3.596850F*v2-1.367720F*v3+1.F
          +(-16.317400F*v2+9.044010F+9.229490F*v3)*r2
          +(5.565890F+19.788601F*v2-20.212299F*v3)*r6);
    const float b=(-1.285140F*ndotv+.990440F+(1.296780F-.755907F*ndotv)*r2)
        /(2.923380F*ndotv+59.418800F*v3+1.F
          +(20.322500F-27.030199F*ndotv+222.591995F*v3)*r2
          +(626.130005F*ndotv+316.627014F*v3+121.563004F)*r6);
    std::array<float,3> result{};
    for(unsigned i=0;i<3;++i)result[i]=(f0[i]*a+b)*(1.F+(1.F-a-b)/(a+b)*f0[i]);
    return result;
}
}
