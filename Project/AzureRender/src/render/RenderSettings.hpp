#pragma once

#include "extensions/SceneType.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace azurerender {

inline constexpr std::uint32_t kShowcasePresetVersion = 1;

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

enum class BlackholeQuality : std::uint32_t {
    Performance = 0,
    Balanced = 1,
    Cinematic = 2,
};

enum class BlackholeCamera : std::uint32_t {
    Front = 0,
    OrbitLeft = 1,
    High = 2,
    Close = 3,
    OverShoulder = 4,
};

struct BlackholeSettings {
    static constexpr std::uint32_t kSchemaVersion = 1;
    BlackholeQuality quality = BlackholeQuality::Cinematic;
    BlackholeCamera camera = BlackholeCamera::Front;
};

struct BlackholeQualityParameters {
    std::uint32_t maxTraceSteps;
    std::uint32_t samplesPerPixel;
    float nearStepScale;
};

struct CharacterPresentationSettings {
    bool backgroundEnabled = true;
    bool platformEnabled = true;
};

struct RenderSettings {
    static constexpr std::uint32_t kSchemaVersion = 6;

    // Selects the pluggable scene renderer that draws the current frame.
    // Character is the default stylized character pipeline; Blackhole is the
    // Schwarzschild geodesic-tracing renderer. New scene types register an
    // ISceneRenderer and extend SceneType.
    SceneType sceneType = SceneType::Character;

    // FYP benchmark: render path under evaluation. Traditional = separate
    // render passes; Subpasses / DynamicRendering select the alternative
    // execution model (subpass-local reads / VK_KHR_dynamic_rendering).
    enum class RenderPath : std::uint32_t {
        Traditional = 0,
        Subpasses = 1,
        DynamicRendering = 2,
    };

    RenderPath renderPath = RenderPath::Traditional;
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
    BlackholeSettings blackhole;
    CharacterPresentationSettings characterPresentation;
};

void validateRenderSettings(const RenderSettings& settings);
[[nodiscard]] RenderSettings migrateRenderSettings(
    RenderSettings settings,
    std::uint32_t sourceSchemaVersion);

[[nodiscard]] std::string_view showcasePresetName(std::uint32_t preset);
[[nodiscard]] std::string_view blackholeQualityName(BlackholeQuality quality);
[[nodiscard]] std::string_view blackholeCameraName(BlackholeCamera camera);
[[nodiscard]] BlackholeQualityParameters blackholeQualityParameters(
    BlackholeQuality quality);
[[nodiscard]] bool blackholeHistoryNeedsReset(
    const BlackholeSettings& previous,
    const BlackholeSettings& current) noexcept;
void applyShowcasePresetLook(
    RenderSettings& settings,
    std::uint32_t preset);
void loadShowcasePresetCatalog(const std::filesystem::path& path);

}  // namespace azurerender
