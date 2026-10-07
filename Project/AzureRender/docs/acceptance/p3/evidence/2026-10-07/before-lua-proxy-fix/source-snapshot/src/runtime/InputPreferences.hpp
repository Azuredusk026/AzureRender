#pragma once
#include "foundation/SettingRegistry.hpp"
namespace azurerender {
inline void registerInputPreferences(SettingRegistry& registry) {
    registry.add({"input.cameraSensitivity","Mouse camera sensitivity multiplier",1.0,.1,4.,false,true,false});
}
inline float cameraSensitivity(const SettingRegistry& registry) {
    return registry.get("input.cameraSensitivity").get<float>();
}
}
