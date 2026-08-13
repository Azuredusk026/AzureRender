#pragma once

#include <array>
#include <cstdint>

namespace azurerender {

struct FaceSdfSettings {
    static constexpr std::uint32_t kSchemaVersion = 1;

    bool enabled = true;
    bool mirrorHorizontal = false;
    float threshold = 0.50F;
    float softness = 0.08F;
    float noseShadowStrength = 0.0F;
    float jawShadowStrength = 0.0F;
    std::array<float, 4> shadowColor{0.55F, 0.36F, 0.34F, 0.55F};
};

struct RenderSettings {
    static constexpr std::uint32_t kSchemaVersion = 1;

    bool stylizedLightingEnabled = true;
    float styleMaskStrength = 1.0F;
    float diffuseBandThreshold = 0.40F;
    std::uint32_t showcasePreset = 0;
    bool innerOutlineEnabled = true;
    bool silhouetteOutlineEnabled = true;
    std::uint32_t diagnosticView = 0;
    FaceSdfSettings faceSdf;
};

void validateRenderSettings(const RenderSettings& settings);

}  // namespace azurerender
