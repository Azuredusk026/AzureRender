#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace azurerender {

enum class EnvironmentProjection {
    Auto,
    Equirectangular,
    CubeFaces,
};

struct SceneEnvironmentSource {
    std::string path;
    EnvironmentProjection projection = EnvironmentProjection::Auto;
};

struct EnvironmentImage {
    std::vector<std::uint16_t> rgba16f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string description;
};

// Loads an equirectangular image or a directory containing
// *_Right/Left/Up/Down/Front/Back images. Cube faces are normalized to the
// renderer's shared equirectangular sampling convention.
[[nodiscard]] EnvironmentImage loadEnvironmentImage(
    const SceneEnvironmentSource& source);

[[nodiscard]] std::uint16_t environmentFloatToHalf(float value);

}  // namespace azurerender
