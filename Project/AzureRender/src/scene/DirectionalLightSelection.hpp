#pragma once
#include "scene/SceneDescription.hpp"
#include <cmath>
#include <stdexcept>

namespace azurerender::scene {
// Borrowed result remains valid while the source light collection is unchanged.
// Linear luminance selects the most influential sun; identity breaks ties.
enum class DirectionalLightReceiver { All, Surfaces, Characters };
inline const SceneLightDesc* primaryDirectionalLight(const std::vector<SceneLightDesc>& lights,
    DirectionalLightReceiver receiver=DirectionalLightReceiver::All) {
    if(receiver!=DirectionalLightReceiver::All&&receiver!=DirectionalLightReceiver::Surfaces
        &&receiver!=DirectionalLightReceiver::Characters)
        throw std::invalid_argument("Unknown directional light receiver");
    const SceneLightDesc* selected=nullptr;
    float greatestPower=0;
    for(const auto& light:lights) {
        if(!light.enabled||light.shape.type!=LightKind::Directional)continue;
        if(receiver==DirectionalLightReceiver::Surfaces&&!light.shape.receivers.surfaces)continue;
        if(receiver==DirectionalLightReceiver::Characters&&!light.shape.receivers.characters)continue;
        const float power=light.intensity*(light.color[0]*.2126F+light.color[1]*.7152F+light.color[2]*.0722F);
        if(!std::isfinite(power)||power<=0)continue;
        if(!selected||power>greatestPower||(power==greatestPower&&light.id<selected->id)) {
            selected=&light;greatestPower=power;
        }
    }
    return selected;
}
}
