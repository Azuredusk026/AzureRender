#pragma once
#include "scene/SceneDescription.hpp"
#include <cmath>

namespace azurerender::scene {
// Borrowed result remains valid while the source light collection is unchanged.
// Linear luminance selects the most influential sun; identity breaks ties.
inline const SceneLightDesc* primaryDirectionalLight(const std::vector<SceneLightDesc>& lights) {
    const SceneLightDesc* selected=nullptr;
    float greatestPower=0;
    for(const auto& light:lights) {
        if(!light.enabled||light.shape.type!=LightKind::Directional)continue;
        const float power=light.intensity*(light.color[0]*.2126F+light.color[1]*.7152F+light.color[2]*.0722F);
        if(!std::isfinite(power)||power<=0)continue;
        if(!selected||power>greatestPower||(power==greatestPower&&light.id<selected->id)) {
            selected=&light;greatestPower=power;
        }
    }
    return selected;
}
}
