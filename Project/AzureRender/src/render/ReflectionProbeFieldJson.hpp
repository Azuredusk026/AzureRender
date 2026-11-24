#pragma once
#include "render/ReflectionProbeField.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void validateProbeFieldNumbers(const nlohmann::json& value,std::size_t count){
    if(!value.is_array()||value.size()!=count)throw std::invalid_argument("Reflection field array length differs");
    for(const auto& x:value)if(!x.is_number())throw std::invalid_argument("Reflection field array requires numeric values");
}
inline void validateProbeFieldMatrix(const nlohmann::json& value,std::size_t columns){
    if(!value.is_array()||value.size()!=3)throw std::invalid_argument("Reflection field matrix requires three rows");
    for(const auto& row:value)validateProbeFieldNumbers(row,columns);
}
inline void to_json(nlohmann::json& json,const ReflectionProbeVolume& p){
    p.validate(256);json={{"luminanceSH",p.luminanceSH},{"halfExtents",p.halfExtents},{"worldToLocal",p.worldToLocal},
        {"inverseBlendWidths",p.inverseBlendWidths},{"arrayLayer",p.arrayLayer},{"intensity",p.intensity},
        {"fade",p.fade},{"shape",p.shape==ReflectionProbeVolume::Shape::Box?"box":"sphere"},{"projection",p.projection}};
}
inline void from_json(const nlohmann::json& json,ReflectionProbeVolume& p){
    if(!json.is_object()||json.size()!=9||!json.at("arrayLayer").is_number_integer()||json.at("arrayLayer").get<std::int64_t>()<0
        ||json.at("arrayLayer").get<std::int64_t>()>=256||!json.at("projection").is_boolean())
        throw std::invalid_argument("Invalid reflection volume fields");
    validateProbeFieldNumbers(json.at("luminanceSH"),4);validateProbeFieldNumbers(json.at("halfExtents"),3);
    validateProbeFieldNumbers(json.at("inverseBlendWidths"),3);validateProbeFieldMatrix(json.at("worldToLocal"),4);
    ReflectionProbeVolume result;result.luminanceSH=json.at("luminanceSH").get<std::array<float,4>>();
    result.halfExtents=json.at("halfExtents").get<std::array<float,3>>();
    result.worldToLocal=json.at("worldToLocal").get<std::array<std::array<float,4>,3>>();
    result.inverseBlendWidths=json.at("inverseBlendWidths").get<std::array<float,3>>();
    result.arrayLayer=json.at("arrayLayer").get<unsigned>();
    for(const auto* key:{"intensity","fade"})if(!json.at(key).is_number())throw std::invalid_argument("Reflection volume weights must be numeric");
    result.intensity=json.at("intensity").get<float>();result.fade=json.at("fade").get<float>();
    result.projection=json.at("projection").get<bool>();
    if(json.at("shape")=="box")result.shape=ReflectionProbeVolume::Shape::Box;
    else if(json.at("shape")=="sphere")result.shape=ReflectionProbeVolume::Shape::Sphere;
    else throw std::invalid_argument("Unknown reflection volume shape");
    result.validate(256);p=result;
}
inline void to_json(nlohmann::json& json,const ReflectionProbeField& field){
    field.validate(256);json={{"schemaVersion",1},{"probes",field.probes},{"defaultLayer",field.defaultLayer},
        {"worldToField",field.worldToField},{"defaultLuminanceSH",field.defaultLuminanceSH},{"adaptation",field.adaptation},{"intensity",field.intensity}};
}
inline void from_json(const nlohmann::json& json,ReflectionProbeField& field){
    if(!json.is_object()||json.size()!=7||!json.at("schemaVersion").is_number_integer()||json.at("schemaVersion")!=1
        ||!json.at("probes").is_array()||json.at("probes").size()>ReflectionProbeField::maxProbes
        ||!json.at("defaultLayer").is_number_integer()||json.at("defaultLayer").get<std::int64_t>()<0
        ||json.at("defaultLayer").get<std::int64_t>()>=256)
        throw std::invalid_argument("Invalid reflection field version, count or fallback layer");
    validateProbeFieldNumbers(json.at("defaultLuminanceSH"),4);validateProbeFieldMatrix(json.at("worldToField"),3);
    ReflectionProbeField result;result.probes=json.at("probes").get<std::vector<ReflectionProbeVolume>>();
    result.worldToField=json.at("worldToField").get<std::array<std::array<float,3>,3>>();
    result.defaultLayer=json.at("defaultLayer").get<unsigned>();result.defaultLuminanceSH=json.at("defaultLuminanceSH").get<std::array<float,4>>();
    for(const auto* key:{"adaptation","intensity"})if(!json.at(key).is_number())throw std::invalid_argument("Reflection field weights must be numeric");
    result.adaptation=json.at("adaptation").get<float>();result.intensity=json.at("intensity").get<float>();result.validate(256);field=std::move(result);
}
}
