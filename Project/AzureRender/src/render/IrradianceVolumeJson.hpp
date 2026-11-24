#pragma once
#include "render/IrradianceVolumeBinding.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& j,const IrradianceVolumeBinding& binding){
    binding.validate();j={{"version",2},{"asset",binding.asset},{"intensity",binding.intensity},{"enabled",binding.enabled},
        {"sampling",{{"normalOffset",binding.sampling.normalOffset},{"jitterAmplitude",binding.sampling.jitterAmplitude},{"phase",binding.sampling.phase}}}};
    if(binding.sampling.pixelOrigin!=IrradianceQueryParameters::PixelOrigin::TopLeft
        ||binding.sampling.jitterSpace!=IrradianceQueryParameters::JitterSpace::World){
        j["version"]=3;
        j["sampling"]["pixelOrigin"]=binding.sampling.pixelOrigin==IrradianceQueryParameters::PixelOrigin::TopLeft?"top-left":"bottom-left";
        j["sampling"]["jitterSpace"]=binding.sampling.jitterSpace==IrradianceQueryParameters::JitterSpace::World?"world":"field";
    }
}
inline void from_json(const nlohmann::json& j,IrradianceVolumeBinding& binding){
    const auto version=j.value("version",0);
    if(version!=1&&version!=2&&version!=3)throw std::invalid_argument("Unsupported irradiance volume binding version");
    if(version==1&&j.contains("sampling"))throw std::invalid_argument("Irradiance sampling requires binding version 2");
    IrradianceVolumeBinding result;
    result.asset=j.at("asset").get<std::string>();result.intensity=j.value("intensity",1.F);
    result.enabled=j.value("enabled",false);
    if(version==3&&(!j.contains("sampling")||!j.at("sampling").is_object()
        ||!j.at("sampling").contains("pixelOrigin")||!j.at("sampling").contains("jitterSpace")))
        throw std::invalid_argument("Irradiance binding version 3 requires explicit query frame policies");
    if(j.contains("sampling")){
        const auto& sampling=j.at("sampling");
        if(!sampling.is_object())throw std::invalid_argument("Irradiance sampling must be an object");
        result.sampling.normalOffset=sampling.value("normalOffset",0.F);
        result.sampling.jitterAmplitude=sampling.value("jitterAmplitude",0.F);
        result.sampling.phase=sampling.value("phase",0.F);
        if(version<3&&(sampling.contains("pixelOrigin")||sampling.contains("jitterSpace")))
            throw std::invalid_argument("Irradiance query frame requires binding version 3");
        if(version==3){
            if(!sampling.at("pixelOrigin").is_string()||!sampling.at("jitterSpace").is_string())
                throw std::invalid_argument("Irradiance query frame policies must be strings");
            const auto origin=sampling.at("pixelOrigin").get<std::string>();
            const auto space=sampling.at("jitterSpace").get<std::string>();
            if(origin!="top-left"&&origin!="bottom-left")throw std::invalid_argument("Unknown irradiance pixel origin");
            if(space!="world"&&space!="field")throw std::invalid_argument("Unknown irradiance jitter space");
            result.sampling.pixelOrigin=origin=="top-left"?IrradianceQueryParameters::PixelOrigin::TopLeft:IrradianceQueryParameters::PixelOrigin::BottomLeft;
            result.sampling.jitterSpace=space=="world"?IrradianceQueryParameters::JitterSpace::World:IrradianceQueryParameters::JitterSpace::Field;
        }
    }
    result.validate();binding=std::move(result);
}
}
