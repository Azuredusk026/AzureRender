#pragma once
#include <cstdint>
namespace azurerender {
// This bit belongs to a submitted view's shading policy, not authored assets.
inline constexpr std::uint32_t kTemporalMaterialSamples=1U<<31U;
inline constexpr std::uint32_t materialSamplingFeatures(std::uint32_t authored,bool temporalSamples) noexcept {
    return (authored&~kTemporalMaterialSamples)|(temporalSamples?kTemporalMaterialSamples:0U);
}
}
