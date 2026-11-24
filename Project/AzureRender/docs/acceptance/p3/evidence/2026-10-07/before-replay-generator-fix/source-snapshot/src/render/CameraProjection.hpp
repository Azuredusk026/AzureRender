#pragma once
#include "render/RenderMath.hpp"
#include "render/RenderSettings.hpp"

namespace azurerender {
inline internal::Matrix4 characterProjection(const RenderSettings& settings, float aspect) {
    auto result=internal::perspective(3.14159265358979323846F/3,aspect,settings.cameraNear,settings.cameraFar);
    result[0]*=internal::kCharacterLensScale;result[5]*=internal::kCharacterLensScale;
    return result;
}
}
