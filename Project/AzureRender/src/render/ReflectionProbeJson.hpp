#pragma once
#include "render/ReflectionProbe.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& json,const ReflectionProbe& probe){
    probe.validate();
    json={{"version",1},{"texture",probe.texture},{"center",probe.center},{"halfExtents",probe.halfExtents},
        {"capturePosition",probe.capturePosition},{"intensity",probe.intensity},
        {"blendDistance",probe.blendDistance},{"boxProjection",probe.boxProjection}};
}
inline void from_json(const nlohmann::json& json,ReflectionProbe& probe){
    if(json.value("version",0)!=1)throw std::invalid_argument("Unsupported reflection probe version");
    ReflectionProbe result;
    result.texture=json.at("texture").get<std::string>();
    result.center=json.value("center",result.center);result.halfExtents=json.value("halfExtents",result.halfExtents);
    result.capturePosition=json.value("capturePosition",result.capturePosition);
    result.intensity=json.value("intensity",result.intensity);result.blendDistance=json.value("blendDistance",result.blendDistance);
    result.boxProjection=json.value("boxProjection",result.boxProjection);result.validate();probe=std::move(result);
}
}
