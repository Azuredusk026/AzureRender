#pragma once
#include "render/FogTransport.hpp"
#include <cstdint>
#include <filesystem>
#include <vector>

namespace azurerender {
// X-fastest, then Y, then Z. Linear RGBA16F stores scattering RGB and scalar T.
struct FogVolumeData {
    static constexpr std::uint32_t maxDimension=256;
    static constexpr std::size_t maxPayloadBytes=64U*1024U*1024U;
    std::array<std::uint32_t,3> dimensions{};
    std::vector<std::uint16_t> rgba16f;
    void validate() const;
    std::vector<std::uint8_t> encode() const;
    static FogVolumeData decode(const std::vector<std::uint8_t>&);
    static FogVolumeData load(const std::filesystem::path&);
    void save(const std::filesystem::path&) const;
    // Linear filtering with normalized coordinates and clamp-to-edge addressing.
    FogTransport sample(std::array<float,3> uvw) const;
};
}
