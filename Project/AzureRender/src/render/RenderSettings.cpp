#include "RenderSettings.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace azurerender {

RenderSettings migrateRenderSettings(
    RenderSettings settings,
    const std::uint32_t sourceSchemaVersion) {
    if (sourceSchemaVersion == 0
        || sourceSchemaVersion > RenderSettings::kSchemaVersion) {
        throw std::invalid_argument("Unsupported RenderSettings schema version");
    }
    if (sourceSchemaVersion < 4) {
        settings.sceneType = SceneType::Character;
    }
    validateRenderSettings(settings);
    return settings;
}

std::string_view showcasePresetName(const std::uint32_t preset) {
    constexpr std::array<std::string_view, 5> kNames = {
        "Azure Gallery",
        "Endfield Industrial",
        "Neutral Material Check",
        "Specular Rim",
        "Rear Emissive",
    };
    return preset < kNames.size() ? kNames[preset] : "Unknown";
}

void applyShowcasePresetLook(
    RenderSettings& settings,
    const std::uint32_t preset) {
    if (preset > 4) {
        throw std::invalid_argument("showcasePreset must be within 0 and 4");
    }

    settings.showcasePreset = preset;
    settings.stylizedLightingEnabled = true;
    settings.innerOutlineEnabled = true;
    settings.silhouetteOutlineEnabled = true;
    settings.grade.toneMappingEnabled = true;
    settings.bloom.enabled = true;

    switch (preset) {
    case 0:
        settings.grade.exposureEv = 0.0F;
        settings.grade.saturation = 1.0F;
        settings.grade.contrast = 1.0F;
        settings.grade.tint = {1.0F, 1.0F, 1.0F};
        settings.bloom.threshold = 1.05F;
        settings.bloom.strength = 0.16F;
        settings.outline.strength = 0.40F;
        settings.outline.color = {0.008F, 0.013F, 0.022F};
        break;
    case 1:
        settings.grade.exposureEv = -0.15F;
        settings.grade.saturation = 0.88F;
        settings.grade.contrast = 1.10F;
        settings.grade.tint = {0.96F, 1.00F, 1.03F};
        settings.bloom.threshold = 1.15F;
        settings.bloom.strength = 0.10F;
        settings.outline.strength = 0.46F;
        settings.outline.color = {0.010F, 0.018F, 0.024F};
        break;
    case 2:
        settings.grade.exposureEv = 0.0F;
        settings.grade.saturation = 0.92F;
        settings.grade.contrast = 1.0F;
        settings.grade.tint = {1.0F, 1.0F, 1.0F};
        settings.bloom.threshold = 1.35F;
        settings.bloom.strength = 0.04F;
        settings.outline.strength = 0.32F;
        settings.outline.color = {0.012F, 0.014F, 0.018F};
        break;
    case 3:
        settings.grade.exposureEv = -0.08F;
        settings.grade.saturation = 0.94F;
        settings.grade.contrast = 1.06F;
        settings.grade.tint = {0.96F, 0.99F, 1.05F};
        settings.bloom.threshold = 1.0F;
        settings.bloom.strength = 0.13F;
        settings.outline.strength = 0.38F;
        settings.outline.color = {0.008F, 0.012F, 0.022F};
        break;
    case 4:
        settings.grade.exposureEv = -0.28F;
        settings.grade.saturation = 0.90F;
        settings.grade.contrast = 1.12F;
        settings.grade.tint = {1.04F, 0.98F, 0.94F};
        settings.bloom.threshold = 0.85F;
        settings.bloom.strength = 0.18F;
        settings.outline.strength = 0.48F;
        settings.outline.color = {0.006F, 0.008F, 0.012F};
        break;
    }
}

void validateRenderSettings(const RenderSettings& settings) {
    const auto requireRange = [](
                                  const float value,
                                  const float minimum,
                                  const float maximum,
                                  const char* name) {
        if (!std::isfinite(value) || value < minimum || value > maximum) {
            throw std::invalid_argument(
                std::string(name) + " must be within "
                + std::to_string(minimum) + " and "
                + std::to_string(maximum));
        }
    };

    requireRange(settings.styleMaskStrength, 0.0F, 2.0F, "styleMaskStrength");
    requireRange(
        settings.diffuseBandThreshold,
        0.05F,
        0.95F,
        "diffuseBandThreshold");
    if (settings.showcasePreset > 4) {
        throw std::invalid_argument("showcasePreset must be within 0 and 4");
    }
    if (settings.diagnosticView > 4) {
        throw std::invalid_argument("diagnosticView must be within 0 and 4");
    }
    requireRange(settings.faceSdf.threshold, 0.0F, 1.0F, "faceSdf.threshold");
    requireRange(settings.faceSdf.softness, 0.001F, 0.5F, "faceSdf.softness");
    requireRange(
        settings.faceSdf.noseShadowStrength,
        0.0F,
        1.0F,
        "faceSdf.noseShadowStrength");
    requireRange(
        settings.faceSdf.jawShadowStrength,
        0.0F,
        1.0F,
        "faceSdf.jawShadowStrength");
    for (const float channel : settings.faceSdf.shadowColor) {
        requireRange(channel, 0.0F, 1.0F, "faceSdf.shadowColor channel");
    }
    requireRange(settings.bloom.threshold, 0.0F, 8.0F, "bloom.threshold");
    requireRange(settings.bloom.strength, 0.0F, 2.0F, "bloom.strength");
    requireRange(settings.outline.strength, 0.0F, 2.0F, "outline.strength");
    requireRange(
        settings.outline.depthThreshold,
        0.001F,
        2.0F,
        "outline.depthThreshold");
    requireRange(
        settings.outline.normalThreshold,
        0.001F,
        1.0F,
        "outline.normalThreshold");
    for (const float channel : settings.outline.color) {
        requireRange(channel, 0.0F, 8.0F, "outline.color channel");
    }
    requireRange(settings.grade.exposureEv, -8.0F, 8.0F, "grade.exposureEv");
    requireRange(settings.grade.saturation, 0.0F, 2.0F, "grade.saturation");
    requireRange(settings.grade.contrast, 0.0F, 2.0F, "grade.contrast");
    for (const float channel : settings.grade.tint) {
        requireRange(channel, 0.0F, 2.0F, "grade.tint channel");
    }
}

}  // namespace azurerender
