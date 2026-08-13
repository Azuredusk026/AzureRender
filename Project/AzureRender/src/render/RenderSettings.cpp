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
}

}  // namespace azurerender
