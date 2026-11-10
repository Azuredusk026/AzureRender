#pragma once
#include "render/ReferenceCameraView.hpp"
#include <nlohmann/json_fwd.hpp>
namespace azurerender {
void to_json(nlohmann::json&,const ReferenceCameraView&);
void from_json(const nlohmann::json&,ReferenceCameraView&);
}
