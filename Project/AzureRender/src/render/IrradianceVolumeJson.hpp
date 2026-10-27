#pragma once
#include "render/IrradianceVolumeBinding.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& j,const IrradianceVolumeBinding& binding){
    binding.validate();j={{"version",2},{"asset",binding.asset},{"intensity",binding.intensity},{"enabled",binding.enabled},
        {"sampling",{{"normalOffset",binding.sampling.normalOffset},{"jitterAmplitude",binding.sampling.jitterAmplitude},{"phase",binding.sampling.phase}}}};
}
inline void from_json(const nlohmann::json& j,IrradianceVolumeBinding& binding){
    const auto version=j.value("version",0);
    if(version!=1&&version!=2)throw std::invalid_argument("Unsupported irradiance volume binding version");
    if(version==1&&j.contains("sampling"))throw std::invalid_argument("Irradiance sampling requires binding version 2");
    IrradianceVolumeBinding result;
    result.asset=j.at("asset").get<std::string>();result.intensity=j.value("intensity",1.F);
    result.enabled=j.value("enabled",false);
    if(j.contains("sampling")){
        const auto& sampling=j.at("sampling");
        if(!sampling.is_object())throw std::invalid_argument("Irradiance sampling must be an object");
        result.sampling.normalOffset=sampling.value("normalOffset",0.F);
        result.sampling.jitterAmplitude=sampling.value("jitterAmplitude",0.F);
        result.sampling.phase=sampling.value("phase",0.F);
    }
    result.validate();binding=std::move(result);
}
}
