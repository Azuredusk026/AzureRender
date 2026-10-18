#pragma once
#include "render/IrradianceVolumeBinding.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& j,const IrradianceVolumeBinding& binding){
    binding.validate();j={{"version",1},{"asset",binding.asset},{"intensity",binding.intensity},{"enabled",binding.enabled}};
}
inline void from_json(const nlohmann::json& j,IrradianceVolumeBinding& binding){
    if(j.value("version",0)!=1)throw std::invalid_argument("Unsupported irradiance volume binding version");
    IrradianceVolumeBinding result;
    result.asset=j.at("asset").get<std::string>();result.intensity=j.value("intensity",1.F);
    result.enabled=j.value("enabled",false);result.validate();binding=std::move(result);
}
}
