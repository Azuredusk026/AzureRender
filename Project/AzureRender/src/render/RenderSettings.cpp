#include "RenderSettings.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace azurerender {

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
