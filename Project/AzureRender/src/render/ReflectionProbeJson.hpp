#pragma once
#include "render/ReflectionProbe.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& json,const ReflectionProbe& probe){
    probe.validate();
    json={{"version",ReflectionProbe::kSchemaVersion},{"texture",probe.texture},{"center",probe.center},{"halfExtents",probe.halfExtents},
        {"capturePosition",probe.capturePosition},{"intensity",probe.intensity},
        {"blendDistance",probe.blendDistance},{"boxProjection",probe.boxProjection}};
    if(probe.projectionOrigin)json["projectionOrigin"]=*probe.projectionOrigin;
}
inline void from_json(const nlohmann::json& json,ReflectionProbe& probe){
    if(!json.is_object()||!json.contains("version")||!json.at("version").is_number_integer()
        ||(json.at("version")!=1&&json.at("version")!=ReflectionProbe::kSchemaVersion))
        throw std::invalid_argument("Unsupported reflection probe version");
    if(json.at("version")==1&&json.contains("projectionOrigin"))
        throw std::invalid_argument("Projection origin requires reflection probe version 2");
    ReflectionProbe result;
    result.texture=json.at("texture").get<std::string>();
    result.center=json.value("center",result.center);result.halfExtents=json.value("halfExtents",result.halfExtents);
    result.capturePosition=json.value("capturePosition",result.capturePosition);
    if(json.contains("projectionOrigin")){
        const auto& origin=json.at("projectionOrigin");
        if(!origin.is_array()||origin.size()!=3)
            throw std::invalid_argument("Projection origin requires three numeric coordinates");
        for(const auto& coordinate:origin)if(!coordinate.is_number())
            throw std::invalid_argument("Projection origin requires three numeric coordinates");
        result.projectionOrigin=origin.get<std::array<float,3>>();
    }
    result.intensity=json.value("intensity",result.intensity);result.blendDistance=json.value("blendDistance",result.blendDistance);
    result.boxProjection=json.value("boxProjection",result.boxProjection);result.validate();probe=std::move(result);
}
}
