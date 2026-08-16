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

struct BloomSettings {
    static constexpr std::uint32_t kSchemaVersion = 1;

    bool enabled = true;
    float threshold = 1.05F;
    float strength = 0.16F;
};

struct OutlineSettings {
    static constexpr std::uint32_t kSchemaVersion = 1;

    float strength = 0.40F;
    float depthThreshold = 0.18F;
    float normalThreshold = 0.20F;
    std::array<float, 3> color{0.008F, 0.013F, 0.022F};
};

struct GradeSettings {
    static constexpr std::uint32_t kSchemaVersion = 1;

    float exposureEv = 0.0F;
    float saturation = 1.0F;
    float contrast = 1.0F;
    std::array<float, 3> tint{1.0F, 1.0F, 1.0F};
    bool toneMappingEnabled = true;
};

struct RenderSettings {
    static constexpr std::uint32_t kSchemaVersion = 2;

    bool stylizedLightingEnabled = true;
    float styleMaskStrength = 1.0F;
    float diffuseBandThreshold = 0.40F;
    std::uint32_t showcasePreset = 0;
    bool innerOutlineEnabled = true;
    bool silhouetteOutlineEnabled = true;
    std::uint32_t diagnosticView = 0;
    std::array<float, 2> morphWeights{{0.0F, 0.0F}};
    FaceSdfSettings faceSdf;
    BloomSettings bloom;
    OutlineSettings outline;
    GradeSettings grade;
};

void validateRenderSettings(const RenderSettings& settings);

}  // namespace azurerender
