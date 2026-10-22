#pragma once
#include "render/LightShape.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& value,const LightReceivers& receivers) {
    value={{"schemaVersion",1},{"surfaces",receivers.surfaces},{"characters",receivers.characters}};
}
inline void from_json(const nlohmann::json& value,LightReceivers& receivers) {
    if(!value.is_object()||value.size()!=3||!value.contains("schemaVersion")
        ||!value["schemaVersion"].is_number_integer()||value["schemaVersion"]!=1
        ||!value.contains("surfaces")||!value["surfaces"].is_boolean()
        ||!value.contains("characters")||!value["characters"].is_boolean())
        throw std::invalid_argument("Invalid light receiver schema or flags");
    receivers={value["surfaces"].get<bool>(),value["characters"].get<bool>()};
}
inline void to_json(nlohmann::json& value,const LightInfluenceBox& box) {
    box.validate();
    value={{"enabled",box.enabled},{"worldToBox",box.worldToBox},{"falloffThreshold",box.falloffThreshold}};
}
inline void from_json(const nlohmann::json& value,LightInfluenceBox& box) {
    box.enabled=value.value("enabled",false);
    box.worldToBox=value.value("worldToBox",box.worldToBox);
    box.falloffThreshold=value.value("falloffThreshold",.8F);
    box.validate();
}
inline void to_json(nlohmann::json& value, const LightShape& shape) {
    shape.validate();
    value = {{"type", shape.type == LightKind::Directional ? "directional" : shape.type == LightKind::Spot ? "spot" : "point"},
        {"direction", shape.direction}, {"innerCone", shape.innerCone}, {"outerCone", shape.outerCone},
        {"falloff",shape.falloff==LightFalloff::RangePower?"range-power":
            shape.falloff==LightFalloff::SmoothInverseSquare?"smooth-inverse-square":"legacy"},
        {"falloffExponent",shape.falloffExponent},{"specularScale",shape.specularScale},
        {"influenceBox",shape.influenceBox},{"receivers",shape.receivers}};
}
inline void from_json(const nlohmann::json& value, LightShape& shape) {
    const auto type = value.value("type", std::string("point"));
    if (type != "point" && type != "spot" && type != "directional") throw std::invalid_argument("Unsupported light shape");
    shape.type = type == "directional" ? LightKind::Directional : type == "spot" ? LightKind::Spot : LightKind::Point;
    shape.direction = value.value("direction", shape.direction);
    shape.innerCone = value.value("innerCone", shape.innerCone);
    shape.outerCone = value.value("outerCone", shape.outerCone);
    const auto falloff=value.value("falloff",std::string("legacy"));
    if(falloff!="legacy" && falloff!="smooth-inverse-square" && falloff!="range-power")
        throw std::invalid_argument("Unsupported light falloff");
    shape.falloff=falloff=="range-power"?LightFalloff::RangePower:
        falloff=="smooth-inverse-square"?LightFalloff::SmoothInverseSquare:LightFalloff::Legacy;
    shape.falloffExponent=value.value("falloffExponent",2.F);
    shape.specularScale=value.value("specularScale",1.F);
    shape.influenceBox=value.value("influenceBox",LightInfluenceBox{});
    shape.receivers=value.value("receivers",LightReceivers{});
    shape.validate();
}
}
