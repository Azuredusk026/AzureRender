#pragma once
#include <array>
#include <cstdint>
#include <type_traits>

namespace azurerender {
struct alignas(16) PackedFogViewProjection {
    static constexpr unsigned layoutVersion=1;
    std::array<std::array<float,4>,3> rows{};
};
static_assert(sizeof(PackedFogViewProjection)==48&&alignof(PackedFogViewProjection)==16);
static_assert(std::is_standard_layout_v<PackedFogViewProjection>);
struct FogViewProjection {
    enum class PixelOrigin { UpperLeft, LowerLeft };
    std::array<std::uint32_t,2> extent{1,1};
    PixelOrigin pixelOrigin=PixelOrigin::UpperLeft;
    std::uint32_t framePhase=0;
    float pixelCenterOffset=.5F,jitterRadius=0;
    std::array<float,3> depthEncoding{0,1,1};
    float minimumViewDepth=0,nearFadeWidth=.000001F;
    void validate() const;
    PackedFogViewProjection pack() const;
    // Pixel coordinates use the caller's upper-left view origin. UVW addresses
    // the volume's declared origin; the fourth component is near-plane weight.
    std::array<float,4> coordinates(std::array<std::uint32_t,2> pixel,float viewDepth) const;
};
}
