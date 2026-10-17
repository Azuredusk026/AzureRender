#include "render/EnvironmentAsset.hpp"

#include "assets/image/ImageCodec.hpp"
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfFrameBuffer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace azurerender {

namespace {

constexpr float kPi = 3.14159265358979323846F;

struct ByteImage {
    std::vector<float> pixels;
    int width = 0;
    int height = 0;
};
ByteImage loadByteImage(const std::filesystem::path& path) {
    const auto decoded=decodeImageFile(path);
    return {imageLinearRgba(decoded),static_cast<int>(decoded.width),static_cast<int>(decoded.height)};
}

std::filesystem::path findCubeFace(
    const std::filesystem::path& directory,
    const std::string_view suffix) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string stem = entry.path().stem().string();
        std::transform(
            stem.begin(), stem.end(), stem.begin(),
            [](const unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        const std::string expected = "_" + std::string(suffix);
        if (stem.size() >= expected.size()
            && stem.compare(stem.size() - expected.size(), expected.size(), expected) == 0) {
            return entry.path();
        }
    }
    throw std::runtime_error(
        "Cube environment is missing the _" + std::string(suffix)
        + " face in " + directory.string());
}

EnvironmentImage loadCubeDirectory(const std::filesystem::path& directory) {
    enum Face : std::size_t { Right, Left, Up, Down, Front, Back };
    const std::array<std::string_view, 6> names{
        "right", "left", "up", "down", "front", "back"};
    std::array<ByteImage, 6> faces;
    for (std::size_t index = 0; index < faces.size(); ++index) {
        faces[index] = loadByteImage(findCubeFace(directory, names[index]));
        if (index > 0
            && (faces[index].width != faces[0].width
                || faces[index].height != faces[0].height)) {
            throw std::runtime_error(
                "Cube environment faces must have matching dimensions: "
                + directory.string());
        }
    }

    EnvironmentImage output;
    // A 2K equirectangular map is sufficient for the current full-screen
    // scenes and keeps the six-face conversion below 16 MiB in RGBA16F.
    output.width = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(faces[0].width * 4), 2048U);
    output.height = output.width / 2;
    output.rgba16f.resize(
        static_cast<std::size_t>(output.width) * output.height * 4);
    output.description = directory.string() + " (cube faces -> equirectangular)";

    for (std::uint32_t y = 0; y < output.height; ++y) {
        const float v = (static_cast<float>(y) + 0.5F) / output.height;
        const float theta = v * kPi;
        const float directionY = std::cos(theta);
        const float ring = std::sin(theta);
        for (std::uint32_t x = 0; x < output.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5F) / output.width;
            const float phi = (u - 0.5F) * 2.0F * kPi;
            const std::array<float, 3> direction{
                ring * std::cos(phi), directionY, ring * std::sin(phi)};
            const float ax = std::abs(direction[0]);
            const float ay = std::abs(direction[1]);
            const float az = std::abs(direction[2]);
            Face face = Front;
            float faceU = 0.0F;
            float faceV = 0.0F;
            if (ax >= ay && ax >= az) {
                face = direction[0] >= 0.0F ? Right : Left;
                faceU = direction[0] >= 0.0F
                    ? -direction[2] / ax : direction[2] / ax;
                faceV = -direction[1] / ax;
            } else if (ay >= ax && ay >= az) {
                face = direction[1] >= 0.0F ? Up : Down;
                faceU = direction[0] / ay;
                faceV = direction[1] >= 0.0F
                    ? direction[2] / ay : -direction[2] / ay;
            } else {
                face = direction[2] >= 0.0F ? Front : Back;
                faceU = direction[2] >= 0.0F
                    ? direction[0] / az : -direction[0] / az;
                faceV = -direction[1] / az;
            }
            const ByteImage& image = faces[face];
            const int sampleX = std::clamp(
                static_cast<int>((faceU * 0.5F + 0.5F) * image.width),
                0, image.width - 1);
            const int sampleY = std::clamp(
                static_cast<int>((faceV * 0.5F + 0.5F) * image.height),
                0, image.height - 1);
            const std::size_t sourcePixel =
                (static_cast<std::size_t>(sampleY) * image.width + sampleX) * 4;
            const std::size_t targetPixel =
                (static_cast<std::size_t>(y) * output.width + x) * 4;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float encoded = image.pixels[sourcePixel + channel];
                output.rgba16f[targetPixel + channel] =
                    environmentFloatToHalf(encoded);
            }
            output.rgba16f[targetPixel + 3] = environmentFloatToHalf(1.0F);
        }
    }
    return output;
}

}  // namespace

std::uint16_t environmentFloatToHalf(const float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const std::uint32_t sign = (bits >> 16) & 0x8000U;
    const std::int32_t exponent =
        static_cast<std::int32_t>((bits >> 23) & 0xFFU) - 127 + 15;
    const std::uint32_t mantissa = bits & 0x7FFFFFU;
    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<std::uint16_t>(sign);
        }
        const std::uint32_t shifted = (mantissa | 0x800000U) >> (1 - exponent);
        return static_cast<std::uint16_t>(
            sign | (shifted + 0x0FFFU + ((shifted >> 13) & 1U)) >> 13);
    }
    if (exponent >= 31) {
        return static_cast<std::uint16_t>(sign | 0x7C00U);
    }
    return static_cast<std::uint16_t>(
        sign | (static_cast<std::uint32_t>(exponent) << 10)
            | (mantissa >> 13));
}

EnvironmentImage loadEnvironmentImage(const SceneEnvironmentSource& source) {
    if (source.path.empty()) {
        throw std::invalid_argument("Environment source path is empty");
    }
    const auto path=std::filesystem::u8path(source.path);
    EnvironmentImage output;
    if (path.extension() == ".azenv") {
        std::ifstream input(path, std::ios::binary);
        const auto fail = [&]() { throw std::runtime_error("Invalid authored environment: " + path.u8string()); };
        char magic[8]{}; input.read(magic,8);
        if (!input || std::memcmp(magic,"AZENV001",8)) fail();
        const auto uint32 = [&]() {
            std::uint32_t value=0;
            for(unsigned shift=0;shift<32;shift+=8) {
                const int byte=input.get(); if(byte==EOF) fail();
                value |= std::uint32_t(byte)<<shift;
            }
            return value;
        };
        output.width=uint32();output.height=uint32();output.authoredMipLevels=uint32();
        if (!output.width || output.width>16384 || output.height*2ULL!=output.width
            || !output.authoredMipLevels || source.projection==EnvironmentProjection::CubeFaces) fail();
        unsigned maxLevels=1;for(unsigned size=output.width;size>1;size/=2)++maxLevels;
        if(output.authoredMipLevels>maxLevels)fail();
        std::size_t count=0;unsigned w=output.width,h=output.height;
        for(unsigned level=0;level<output.authoredMipLevels;++level) {
            count+=std::size_t(w)*h*4;w=std::max(w/2,1U);h=std::max(h/2,1U);
        }
        if(std::filesystem::file_size(path)!=20+count*2)fail();
        output.rgba16f.resize(count);
        for(auto& value:output.rgba16f) {
            const int lo=input.get(),hi=input.get();if(lo==EOF||hi==EOF)fail();
            value=static_cast<std::uint16_t>(lo|(hi<<8));
            if((value&0x7c00)==0x7c00)fail();
        }
        output.description=path.u8string()+" (authored linear HDR mip chain)";
        return output;
    }
    const bool cube = source.projection == EnvironmentProjection::CubeFaces
        || (source.projection == EnvironmentProjection::Auto
            && std::filesystem::is_directory(path));
    if (cube) {
        return loadCubeDirectory(path);
    }
    const auto decoded=decodeImageFile(path);
    const auto linear=imageLinearRgba(decoded);
    output.width=decoded.width;output.height=decoded.height;
    output.rgba16f.resize(linear.size());
    for(std::size_t i=0;i<linear.size();++i) {
        if(std::abs(linear[i])>65504.F)throw std::runtime_error("Environment pixels exceed finite RGBA16F range: "+path.u8string());
        output.rgba16f[i]=environmentFloatToHalf(linear[i]);
    }
    output.description=path.u8string();return output;
}

} // namespace azurerender
