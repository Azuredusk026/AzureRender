#pragma once
#include "render/LightShape.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline void to_json(nlohmann::json& value, const LightShape& shape) {
    shape.validate();
    value = {{"type", shape.type == LightKind::Spot ? "spot" : "point"},
        {"direction", shape.direction}, {"innerCone", shape.innerCone}, {"outerCone", shape.outerCone}};
}
inline void from_json(const nlohmann::json& value, LightShape& shape) {
    const auto type = value.value("type", std::string("point"));
    if (type != "point" && type != "spot") throw std::invalid_argument("Unsupported light shape");
    shape.type = type == "spot" ? LightKind::Spot : LightKind::Point;
    shape.direction = value.value("direction", shape.direction);
    shape.innerCone = value.value("innerCone", shape.innerCone);
    shape.outerCone = value.value("outerCone", shape.outerCone);
    shape.validate();
}
}
