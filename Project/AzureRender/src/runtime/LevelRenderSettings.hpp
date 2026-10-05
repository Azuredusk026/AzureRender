#pragma once
#include "render/RenderSettings.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline nlohmann::json encodeLevelRenderSettings(const RenderSettings& settings){
    return {{"cameraNear",settings.cameraNear},{"cameraFar",settings.cameraFar},{"shadowDistance",settings.shadowDistance},
        {"showcasePreset",settings.showcasePreset},{"background",settings.characterPresentation.backgroundEnabled},{"platform",settings.characterPresentation.platformEnabled},
        {"faceSdf",settings.faceSdf.enabled},{"faceThreshold",settings.faceSdf.threshold},{"faceSoftness",settings.faceSdf.softness},{"outline",settings.outline.strength},
        {"shadowRadius",settings.shadow.maximumFilterRadiusTexels},{"exposure",settings.grade.exposureEv},
        {"saturation",settings.grade.saturation},{"contrast",settings.grade.contrast},{"tint",settings.grade.tint},{"toneMapping",settings.grade.toneMappingEnabled},
        {"bloomEnabled",settings.bloom.enabled},{"bloomThreshold",settings.bloom.threshold},{"bloomStrength",settings.bloom.strength},
        {"outlineDepth",settings.outline.depthThreshold},{"outlineNormal",settings.outline.normalThreshold},{"outlineColor",settings.outline.color},
        {"stylized",settings.stylizedLightingEnabled},{"innerOutline",settings.innerOutlineEnabled},{"silhouetteOutline",settings.silhouetteOutlineEnabled},
        {"styleMaskStrength",settings.styleMaskStrength},{"diffuseBandThreshold",settings.diffuseBandThreshold},{"diagnosticView",settings.diagnosticView}};
}
inline void decodeLevelRenderSettings(RenderSettings& target,const nlohmann::json& data){
    if(!data.is_object())throw std::invalid_argument("Level renderSettings must be an object");
    auto settings=target;
    settings.cameraNear=data.value("cameraNear",settings.cameraNear);
    settings.cameraFar=data.value("cameraFar",settings.cameraFar);
    settings.shadowDistance=data.value("shadowDistance",settings.shadowDistance);
    settings.showcasePreset=data.value("showcasePreset",settings.showcasePreset);
    settings.characterPresentation.backgroundEnabled=data.value("background",settings.characterPresentation.backgroundEnabled);
    settings.characterPresentation.platformEnabled=data.value("platform",settings.characterPresentation.platformEnabled);
    settings.faceSdf.enabled=data.value("faceSdf",settings.faceSdf.enabled);settings.faceSdf.threshold=data.value("faceThreshold",settings.faceSdf.threshold);settings.faceSdf.softness=data.value("faceSoftness",settings.faceSdf.softness);
    settings.outline.strength=data.value("outline",settings.outline.strength);settings.shadow.maximumFilterRadiusTexels=data.value("shadowRadius",settings.shadow.maximumFilterRadiusTexels);settings.grade.exposureEv=data.value("exposure",settings.grade.exposureEv);
    settings.grade.saturation=data.value("saturation",settings.grade.saturation);settings.grade.contrast=data.value("contrast",settings.grade.contrast);settings.grade.tint=data.value("tint",settings.grade.tint);settings.grade.toneMappingEnabled=data.value("toneMapping",settings.grade.toneMappingEnabled);
    settings.bloom.enabled=data.value("bloomEnabled",settings.bloom.enabled);settings.bloom.threshold=data.value("bloomThreshold",settings.bloom.threshold);settings.bloom.strength=data.value("bloomStrength",settings.bloom.strength);
    settings.outline.depthThreshold=data.value("outlineDepth",settings.outline.depthThreshold);settings.outline.normalThreshold=data.value("outlineNormal",settings.outline.normalThreshold);settings.outline.color=data.value("outlineColor",settings.outline.color);
    settings.stylizedLightingEnabled=data.value("stylized",settings.stylizedLightingEnabled);settings.innerOutlineEnabled=data.value("innerOutline",settings.innerOutlineEnabled);settings.silhouetteOutlineEnabled=data.value("silhouetteOutline",settings.silhouetteOutlineEnabled);
    settings.styleMaskStrength=data.value("styleMaskStrength",settings.styleMaskStrength);
    settings.diffuseBandThreshold=data.value("diffuseBandThreshold",settings.diffuseBandThreshold);
    settings.diagnosticView=data.value("diagnosticView",settings.diagnosticView);
    validateRenderSettings(settings);
    target=settings;
}
}
