#pragma once
#include "render/ReflectionProbe.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& json,const ReflectionLodMapping& mapping){
    mapping.validate();const bool logarithmic=mapping.kind==ReflectionLodMapping::Kind::Logarithmic;
    json={{"schemaVersion",1},{"kind",logarithmic?"logarithmic":"linear"}};
    if(logarithmic){json["scale"]=mapping.scale;json["bias"]=mapping.bias;json["minimumRoughness"]=mapping.minimumRoughness;}
}
inline void from_json(const nlohmann::json& json,ReflectionLodMapping& mapping){
    if(!json.is_object()||!json.contains("schemaVersion")||!json.at("schemaVersion").is_number_integer()
        ||json.at("schemaVersion")!=1||!json.contains("kind"))
        throw std::invalid_argument("Unsupported reflection LOD mapping");
    ReflectionLodMapping result;
    if(json.at("kind")=="linear"){
        if(json.size()!=2)throw std::invalid_argument("Linear LOD mapping requires only version and kind");
    }else if(json.at("kind")=="logarithmic"){
        if(json.size()!=5)throw std::invalid_argument("Logarithmic LOD mapping requires three parameters");
        for(const auto* field:{"scale","bias","minimumRoughness"})
            if(!json.contains(field)||!json.at(field).is_number())throw std::invalid_argument("LOD parameters must be numeric");
        result.kind=ReflectionLodMapping::Kind::Logarithmic;
        result.scale=json.at("scale").get<float>();result.bias=json.at("bias").get<float>();
        result.minimumRoughness=json.at("minimumRoughness").get<float>();
    }else throw std::invalid_argument("Unknown reflection LOD mapping kind");
    result.validate();mapping=result;
}
inline void to_json(nlohmann::json& json,const ReflectionProbe& probe){
    probe.validate();
    json={{"version",probe.lodMapping?ReflectionProbe::kSchemaVersion:2},{"texture",probe.texture},{"center",probe.center},{"halfExtents",probe.halfExtents},
        {"capturePosition",probe.capturePosition},{"intensity",probe.intensity},
        {"blendDistance",probe.blendDistance},{"boxProjection",probe.boxProjection}};
    if(probe.projectionOrigin)json["projectionOrigin"]=*probe.projectionOrigin;
    if(probe.lodMapping)json["lodMapping"]=*probe.lodMapping;
}
inline void from_json(const nlohmann::json& json,ReflectionProbe& probe){
    if(!json.is_object()||!json.contains("version")||!json.at("version").is_number_integer()
        ||(json.at("version")!=1&&json.at("version")!=2&&json.at("version")!=ReflectionProbe::kSchemaVersion))
        throw std::invalid_argument("Unsupported reflection probe version");
    if(json.at("version")==1&&json.contains("projectionOrigin"))
        throw std::invalid_argument("Projection origin requires reflection probe version 2");
    if((json.at("version")==3)!=json.contains("lodMapping"))
        throw std::invalid_argument("Explicit LOD mapping requires reflection probe version 3");
    ReflectionProbe result;
    if(json.contains("lodMapping"))result.lodMapping=json.at("lodMapping").get<ReflectionLodMapping>();
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
