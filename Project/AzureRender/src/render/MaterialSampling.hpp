#pragma once
#include <cstdint>
namespace azurerender {
// This bit belongs to a submitted view's shading policy, not authored assets.
inline constexpr std::uint32_t kTemporalMaterialSamples=1U<<31U;
inline constexpr std::uint32_t kTemporalAlphaCoverage=1U<<30U;
inline constexpr std::uint32_t materialSamplingFeatures(std::uint32_t authored,bool temporalSamples,bool alphaCoverage=false) noexcept {
    return (authored&~(kTemporalMaterialSamples|kTemporalAlphaCoverage))
        |(temporalSamples?kTemporalMaterialSamples:0U)|(alphaCoverage?kTemporalAlphaCoverage:0U);
}
}
