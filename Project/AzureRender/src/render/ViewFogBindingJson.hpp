#pragma once
#include "render/ViewFogBinding.hpp"
#include <nlohmann/json_fwd.hpp>
namespace azurerender {
void to_json(nlohmann::json&,const FogViewProjection&);
void from_json(const nlohmann::json&,FogViewProjection&);
void to_json(nlohmann::json&,const ViewFogBinding&);
void from_json(const nlohmann::json&,ViewFogBinding&);
}
