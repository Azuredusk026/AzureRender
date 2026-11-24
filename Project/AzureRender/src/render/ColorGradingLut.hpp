#pragma once
#include "assets/image/ImageCodec.hpp"
#include <array>
#include <string>

namespace azurerender {
enum class ColorGradingInputDomain : std::uint32_t { Linear = 0, LogC3FullLog = 1 };
enum class ColorGradingOutputEncoding : std::uint32_t { Linear = 0, Srgb = 1 };
enum class ColorGradingEvidence : std::uint32_t { Authored = 0, Candidate = 1, RuntimeCapture = 2 };

struct ColorGradingLutSource {
    std::string generator;
    std::uint32_t generatorVersion = 1;
    std::string inputFingerprint;
    ColorGradingEvidence evidence = ColorGradingEvidence::Authored;
};
struct ColorGradingLut {
    static constexpr std::uint32_t kSchemaVersion = 1;
    std::uint32_t dimension = 2;
    ColorGradingInputDomain inputDomain = ColorGradingInputDomain::Linear;
    ColorGradingLutSource source;
    // Top-left rows. Width=N*N, height=N; R varies within a slice, G by row, B by slice.
    DecodedImage image;
};
void validateColorGradingLut(const ColorGradingLut& lut);
ColorGradingLut loadColorGradingLut(const std::filesystem::path& path);
void saveColorGradingLut(const std::filesystem::path& path, const ColorGradingLut& lut);
ColorGradingLut makeIdentityColorGradingLut(std::uint32_t dimension = 2,
    ColorGradingInputDomain domain = ColorGradingInputDomain::Linear);
float colorGradingInputCoordinate(float value, ColorGradingInputDomain domain);
std::array<float,3> sampleColorGradingLut(const ColorGradingLut& lut,
    const std::array<float,3>& sceneLinear, float exposureMultiplier = 1.0F,
    ColorGradingOutputEncoding outputEncoding = ColorGradingOutputEncoding::Linear);
}
