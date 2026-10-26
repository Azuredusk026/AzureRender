#pragma once
#include "render/RenderSettings.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
inline nlohmann::json encodeNprLighting(const NprLightingSettings& s){
    validateNprLighting(s);
    return {{"schemaVersion",1},{"enabled",s.enabled},{"useSceneIrradiance",s.useSceneIrradiance},
        {"ambientColor",s.ambientColor},{"ambientDirection",s.ambientDirection},{"directionBias",s.directionBias},
        {"directionGain",s.directionGain},{"directionFloor",s.directionFloor},{"outdoorGain",s.outdoorGain},
        {"shadowAmbientGain",s.shadowAmbientGain},{"ambientWeight",s.ambientWeight},{"indoorMix",s.indoorMix},{"litAmbientMix",s.litAmbientMix},
        {"mainColorMix",s.mainColorMix},{"shadowOverride",s.shadowOverride},{"specularFloor",s.specularFloor}};
}
inline void decodeNprLighting(NprLightingSettings& target,const nlohmann::json& data){
    if(!data.is_object()||!data.contains("schemaVersion")||!data.at("schemaVersion").is_number_integer()||data.at("schemaVersion")!=1)
        throw std::invalid_argument("Unsupported NPR lighting settings version");
    auto value=target;const auto shape=encodeNprLighting(value);
    for(const auto& field:data.items()){
        const auto& key=field.key();
        if(!shape.contains(key))throw std::invalid_argument("Unknown NPR lighting setting: "+key);
        if(key=="schemaVersion")continue;
        if(key=="enabled"||key=="useSceneIrradiance"){
            if(!field.value().is_boolean())throw std::invalid_argument("NPR lighting boolean required");
        }else if(key=="ambientColor"||key=="ambientDirection"){
            if(!field.value().is_array()||field.value().size()!=3)throw std::invalid_argument("NPR lighting vector requires three numbers");
            for(const auto& component:field.value())if(!component.is_number())throw std::invalid_argument("NPR lighting vector requires numbers");
        }else if(!field.value().is_number())throw std::invalid_argument("NPR lighting scalar requires a number");
    }
    value.enabled=data.value("enabled",value.enabled);value.useSceneIrradiance=data.value("useSceneIrradiance",value.useSceneIrradiance);
    if(data.contains("ambientColor"))value.ambientColor=data.at("ambientColor").get<std::array<float,3>>();
    if(data.contains("ambientDirection"))value.ambientDirection=data.at("ambientDirection").get<std::array<float,3>>();
    value.directionBias=data.value("directionBias",value.directionBias);value.directionGain=data.value("directionGain",value.directionGain);
    value.directionFloor=data.value("directionFloor",value.directionFloor);value.outdoorGain=data.value("outdoorGain",value.outdoorGain);
    value.shadowAmbientGain=data.value("shadowAmbientGain",value.shadowAmbientGain);value.indoorMix=data.value("indoorMix",value.indoorMix);
    value.ambientWeight=data.value("ambientWeight",value.ambientWeight);
    value.litAmbientMix=data.value("litAmbientMix",value.litAmbientMix);value.mainColorMix=data.value("mainColorMix",value.mainColorMix);
    value.shadowOverride=data.value("shadowOverride",value.shadowOverride);
    value.specularFloor=data.value("specularFloor",value.specularFloor);validateNprLighting(value);target=value;
}
inline nlohmann::json encodeScreenSpaceReflection(const ScreenSpaceReflectionSettings& s){
    return {{"schemaVersion",ScreenSpaceReflectionSettings::kSchemaVersion},{"enabled",s.enabled},
        {"maxDistance",s.maxDistance},{"thickness",s.thickness},{"normalBias",s.normalBias},
        {"maxSteps",s.maxSteps},{"maxRoughness",s.maxRoughness},{"edgeFade",s.edgeFade},{"strength",s.strength}};
}
inline void decodeScreenSpaceReflection(ScreenSpaceReflectionSettings& target,const nlohmann::json& data){
    if(!data.is_object()||!data.contains("schemaVersion")||!data.at("schemaVersion").is_number_integer()
        ||data.at("schemaVersion")!=ScreenSpaceReflectionSettings::kSchemaVersion)
        throw std::invalid_argument("Unsupported screen reflection settings version");
    for(const auto& field:data.items()){
        const auto& key=field.key();
        if(key!="schemaVersion"&&key!="enabled"&&key!="maxDistance"&&key!="thickness"&&key!="normalBias"
            &&key!="maxSteps"&&key!="maxRoughness"&&key!="edgeFade"&&key!="strength")
            throw std::invalid_argument("Unknown screen reflection setting");
    }
    auto value=target;
    value.enabled=data.value("enabled",value.enabled);
    value.maxDistance=data.value("maxDistance",value.maxDistance);
    value.thickness=data.value("thickness",value.thickness);
    value.normalBias=data.value("normalBias",value.normalBias);
    value.maxRoughness=data.value("maxRoughness",value.maxRoughness);
    value.edgeFade=data.value("edgeFade",value.edgeFade);
    value.strength=data.value("strength",value.strength);
    if(data.contains("maxSteps")){
        const auto& steps=data.at("maxSteps");
        if(!steps.is_number_integer()||steps<8||steps>256)
            throw std::invalid_argument("screen reflection steps must be an integer within 8 and 256");
        value.maxSteps=steps.get<std::uint32_t>();
    }
    validateScreenSpaceReflection(value);target=value;
}
inline nlohmann::json encodeLevelRenderSettings(const RenderSettings& settings){
    return {{"schemaVersion",RenderSettings::kSchemaVersion},{"nprLighting",encodeNprLighting(settings.nprLighting)},{"screenSpaceReflection",encodeScreenSpaceReflection(settings.screenSpaceReflection)},{"renderer",sceneTypeName(settings.sceneType)},{"pathTraceSamples",settings.pathTraceSamples},{"pathTraceBounces",settings.pathTraceBounces},{"pathTraceDenoise",settings.pathTraceDenoise},{"pathTraceSeed",settings.pathTraceSeed},{"skyMode",settings.sky.mode},{"skyColor",settings.sky.color},{"skyIntensity",settings.sky.intensity},{"skyRotation",settings.sky.rotationDegrees},{"outlinePixels",settings.outline.silhouetteWidthPixels},{"antiAliasing",settings.antiAliasing},{"cameraNear",settings.cameraNear},{"cameraFar",settings.cameraFar},{"shadowDistance",settings.shadowDistance},
        {"showcasePreset",settings.showcasePreset},{"background",settings.characterPresentation.backgroundEnabled},{"platform",settings.characterPresentation.platformEnabled},
        {"faceSdf",settings.faceSdf.enabled},{"faceThreshold",settings.faceSdf.threshold},{"faceSoftness",settings.faceSdf.softness},{"outline",settings.outline.strength},
        {"shadowRadius",settings.shadow.maximumFilterRadiusTexels},{"exposure",settings.grade.exposureEv},
        {"saturation",settings.grade.saturation},{"contrast",settings.grade.contrast},{"tint",settings.grade.tint},{"toneMapping",settings.grade.toneMappingEnabled},
        {"gradeLut",{{"schemaVersion",ColorGradingLutSettings::kSchemaVersion},{"asset",settings.grade.lut.asset},{"exposureMultiplier",settings.grade.lut.exposureMultiplier}}},
        {"bloomEnabled",settings.bloom.enabled},{"bloomThreshold",settings.bloom.threshold},{"bloomStrength",settings.bloom.strength},
        {"outlineDepth",settings.outline.depthThreshold},{"outlineNormal",settings.outline.normalThreshold},{"outlineColor",settings.outline.color},
        {"stylized",settings.stylizedLightingEnabled},{"sceneLighting",settings.sceneLightingEnabled},{"environmentSpecularModel",std::string(environmentSpecularModelName(settings.environmentSpecularModel))},{"innerOutline",settings.innerOutlineEnabled},{"silhouetteOutline",settings.silhouetteOutlineEnabled},
        {"styleMaskStrength",settings.styleMaskStrength},{"diffuseBandThreshold",settings.diffuseBandThreshold},{"diagnosticView",settings.diagnosticView}};
}
inline void decodeLevelRenderSettings(RenderSettings& target,const nlohmann::json& data){
    if(!data.is_object())throw std::invalid_argument("Level renderSettings must be an object");
    std::uint64_t version=17;
    if(data.contains("schemaVersion")){
        const auto& authored=data.at("schemaVersion");
        if(!authored.is_number_integer())throw std::invalid_argument("Render settings version must be an integer");
        version=authored.get<std::uint64_t>();
        if(!version||version>RenderSettings::kSchemaVersion)throw std::invalid_argument("Unsupported level render settings version");
    }
    if(data.contains("antiAliasing")){
        const auto& mode=data.at("antiAliasing");
        if(!mode.is_number_integer())throw std::invalid_argument("Anti-aliasing mode must be an integer");
        const auto value=mode.get<std::uint64_t>();
        if(value>4||(version<19&&value>3)||(version<18&&value>2))throw std::invalid_argument("Anti-aliasing mode requires its render settings version: temporal 18, multisample 19");
    }
    auto settings=target;
    if(data.contains("nprLighting"))decodeNprLighting(settings.nprLighting,data.at("nprLighting"));
    if(data.contains("screenSpaceReflection"))decodeScreenSpaceReflection(settings.screenSpaceReflection,data.at("screenSpaceReflection"));
    settings.sceneType=sceneTypeFromString(data.value("renderer",std::string(sceneTypeName(settings.sceneType))));
    settings.pathTraceSamples=data.value("pathTraceSamples",settings.pathTraceSamples);
    settings.pathTraceBounces=data.value("pathTraceBounces",settings.pathTraceBounces);
    settings.pathTraceDenoise=data.value("pathTraceDenoise",settings.pathTraceDenoise);
    settings.pathTraceSeed=data.value("pathTraceSeed",settings.pathTraceSeed);
    settings.sky.mode=data.value("skyMode",settings.sky.mode);
    if(data.contains("skyColor")&&(!data.at("skyColor").is_array()||data.at("skyColor").size()!=3))throw std::invalid_argument("Sky color requires three channels");
    settings.sky.color=data.value("skyColor",settings.sky.color);
    settings.sky.intensity=data.value("skyIntensity",settings.sky.intensity);
    settings.sceneLightingEnabled=data.value("sceneLighting",settings.sceneLightingEnabled);
    if(data.contains("environmentSpecularModel"))
        settings.environmentSpecularModel=environmentSpecularModelFromString(data.at("environmentSpecularModel").get<std::string>());
    settings.sky.rotationDegrees=data.value("skyRotation",settings.sky.rotationDegrees);
    settings.outline.silhouetteWidthPixels=data.value("outlinePixels",settings.outline.silhouetteWidthPixels);
    settings.antiAliasing=data.value("antiAliasing",settings.antiAliasing);
    settings.cameraNear=data.value("cameraNear",settings.cameraNear);
    settings.cameraFar=data.value("cameraFar",settings.cameraFar);
    settings.shadowDistance=data.value("shadowDistance",settings.shadowDistance);
    settings.showcasePreset=data.value("showcasePreset",settings.showcasePreset);
    settings.characterPresentation.backgroundEnabled=data.value("background",settings.characterPresentation.backgroundEnabled);
    settings.characterPresentation.platformEnabled=data.value("platform",settings.characterPresentation.platformEnabled);
    settings.faceSdf.enabled=data.value("faceSdf",settings.faceSdf.enabled);settings.faceSdf.threshold=data.value("faceThreshold",settings.faceSdf.threshold);settings.faceSdf.softness=data.value("faceSoftness",settings.faceSdf.softness);
    settings.outline.strength=data.value("outline",settings.outline.strength);settings.shadow.maximumFilterRadiusTexels=data.value("shadowRadius",settings.shadow.maximumFilterRadiusTexels);settings.grade.exposureEv=data.value("exposure",settings.grade.exposureEv);
    settings.grade.saturation=data.value("saturation",settings.grade.saturation);settings.grade.contrast=data.value("contrast",settings.grade.contrast);settings.grade.tint=data.value("tint",settings.grade.tint);settings.grade.toneMappingEnabled=data.value("toneMapping",settings.grade.toneMappingEnabled);
    if(data.contains("gradeLut")){
        const auto& lut=data.at("gradeLut");
        if(!lut.is_object()||!lut.contains("schemaVersion")||!lut.at("schemaVersion").is_number_integer()||lut.at("schemaVersion")!=1)
            throw std::invalid_argument("Unsupported color grading LUT settings version");
        for(const auto& field:lut.items())if(field.key()!="schemaVersion"&&field.key()!="asset"&&field.key()!="exposureMultiplier")
            throw std::invalid_argument("Unknown color grading LUT setting");
        settings.grade.lut.asset=lut.at("asset").get<std::string>();
        settings.grade.lut.exposureMultiplier=lut.at("exposureMultiplier").get<float>();
        settings.grade.lut.resolvedAsset.clear();settings.grade.lut.resourceFingerprint.clear();
    }
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
