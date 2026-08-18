#include "RenderSettings.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace azurerender {

namespace {

struct ShowcaseLook {
    std::string name;
    GradeSettings grade;
    BloomSettings bloom;
    OutlineSettings outline;
};

std::vector<ShowcaseLook>& showcaseLooks() {
    static std::vector<ShowcaseLook> looks;
    return looks;
}

template <std::size_t Size>
std::array<float, Size> readFloatArray(
    const nlohmann::json& json,
    const char* field) {
    const auto values = json.at(field).get<std::vector<float>>();
    if (values.size() != Size) {
        throw std::runtime_error(std::string(field) + " has invalid length");
    }
    std::array<float, Size> result{};
    std::copy(values.begin(), values.end(), result.begin());
    return result;
}

}  // namespace

void loadShowcasePresetCatalog(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open showcase look catalog: " + path.string());
    }
    const nlohmann::json root = nlohmann::json::parse(input);
    if (root.at("schemaVersion").get<std::uint32_t>()
            != kShowcasePresetVersion) {
        throw std::runtime_error("Unsupported showcase look catalog schema");
    }
    std::vector<ShowcaseLook> loaded;
    for (const auto& item : root.at("looks")) {
        ShowcaseLook look;
        look.name = item.at("name").get<std::string>();
        const auto& grade = item.at("grade");
        look.grade.exposureEv = grade.at("exposureEv").get<float>();
        look.grade.saturation = grade.at("saturation").get<float>();
        look.grade.contrast = grade.at("contrast").get<float>();
        look.grade.tint = readFloatArray<3>(grade, "tint");
        const auto& bloom = item.at("bloom");
        look.bloom.threshold = bloom.at("threshold").get<float>();
        look.bloom.strength = bloom.at("strength").get<float>();
        const auto& outline = item.at("outline");
        look.outline.strength = outline.at("strength").get<float>();
        look.outline.color = readFloatArray<3>(outline, "color");
        loaded.push_back(std::move(look));
    }
    if (loaded.size() != 5) {
        throw std::runtime_error("Showcase look catalog must define five looks");
    }
    showcaseLooks() = std::move(loaded);
}

std::string_view blackholeQualityName(const BlackholeQuality quality) {
    switch (quality) {
    case BlackholeQuality::Performance: return "performance";
    case BlackholeQuality::Balanced: return "balanced";
    case BlackholeQuality::Cinematic: return "cinematic";
    }
    return "unknown";
}

std::string_view blackholeCameraName(const BlackholeCamera camera) {
    switch (camera) {
    case BlackholeCamera::Front: return "front";
    case BlackholeCamera::OrbitLeft: return "orbit-left";
    case BlackholeCamera::High: return "high";
    case BlackholeCamera::Close: return "close";
    }
    return "unknown";
}

BlackholeQualityParameters blackholeQualityParameters(
    const BlackholeQuality quality) {
    switch (quality) {
    case BlackholeQuality::Performance: return {600, 1, 0.85F};
    case BlackholeQuality::Balanced: return {1100, 1, 0.65F};
    case BlackholeQuality::Cinematic: return {1800, 4, 0.48F};
    }
    throw std::invalid_argument("Unknown blackhole quality profile");
}

bool blackholeHistoryNeedsReset(
    const BlackholeSettings& previous,
    const BlackholeSettings& current) noexcept {
    return previous.quality != current.quality
        || previous.camera != current.camera;
}

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
    if (sourceSchemaVersion < 5) {
        settings.blackhole = {};
    }
    if (sourceSchemaVersion < 6) {
        settings.characterPresentation = {};
    }
    validateRenderSettings(settings);
    return settings;
}

std::string_view showcasePresetName(const std::uint32_t preset) {
    const auto& looks = showcaseLooks();
    return preset < looks.size() ? std::string_view(looks[preset].name)
                                 : std::string_view("Unknown");
}

void applyShowcasePresetLook(
    RenderSettings& settings,
    const std::uint32_t preset) {
    const auto& looks = showcaseLooks();
    if (preset >= looks.size()) {
        throw std::invalid_argument("showcasePreset must be within 0 and 4");
    }

    settings.showcasePreset = preset;
    settings.stylizedLightingEnabled = true;
    settings.innerOutlineEnabled = true;
    settings.silhouetteOutlineEnabled = true;
    settings.grade.toneMappingEnabled = true;
    settings.bloom.enabled = true;

    settings.grade = looks[preset].grade;
    settings.bloom = looks[preset].bloom;
    settings.outline = looks[preset].outline;
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
    if (blackholeQualityName(settings.blackhole.quality) == "unknown") {
        throw std::invalid_argument("Unknown blackhole quality profile");
    }
    if (blackholeCameraName(settings.blackhole.camera) == "unknown") {
        throw std::invalid_argument("Unknown blackhole camera preset");
    }

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
