#pragma once
#include "assets/AssetFormatRegistry.hpp"
#include <cstdint>

namespace azurerender {
enum class ImagePrecision { UInt8, UInt16, Float32 };
enum class ImageColorSpace { Srgb, Linear };
struct ImageMipLevel {
    std::uint32_t width=0,height=0;
    std::vector<std::uint8_t> rgba8;
};
// RenderCore owns decoding. Calls own their buffers and decoder state.
// Pixels are straight-alpha RGBA in top-left row order.
struct DecodedImage {
    std::uint32_t width=0, height=0, sourceChannels=0;
    ImagePrecision precision=ImagePrecision::UInt8;
    ImageColorSpace colorSpace=ImageColorSpace::Srgb;
    std::vector<std::uint8_t> rgba8;
    std::vector<std::uint16_t> rgba16;
    std::vector<float> rgba32;
    // Authored levels include the base level. Empty means a single ordinary image.
    std::vector<ImageMipLevel> mipLevels;
    std::string sourceCompression;
    std::vector<std::string> sourceLosses;
};
struct ImageRgba8 {
    std::vector<std::uint8_t> pixels;
    std::vector<std::string> losses;
};
DecodedImage decodeImageBytes(const std::vector<unsigned char>& bytes,
    const AssetCancellation& cancelled={});
DecodedImage decodeImageFile(const std::filesystem::path& path,
    const AssetCancellation& cancelled={});
ImageRgba8 convertImageToRgba8(const DecodedImage& image,bool encodeLinearRgb=true);
std::vector<float> imageLinearRgba(const DecodedImage& image);
void reconstructBc5NormalPixels(std::vector<std::uint8_t>& pixels);
void registerImageFormats(AssetFormatRegistry& registry);
}
