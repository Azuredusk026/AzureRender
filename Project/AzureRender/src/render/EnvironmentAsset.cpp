#include "render/EnvironmentAsset.hpp"

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace azurerender {

namespace {

constexpr float kPi = 3.14159265358979323846F;

std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    return extension;
}

struct ByteImage {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
};

ByteImage loadByteImage(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(
        path.string().c_str(), &width, &height, &channels, 4);
    if (data == nullptr) {
        throw std::runtime_error(
            "Failed to decode environment image: " + path.string());
    }
    ByteImage image;
    image.width = width;
    image.height = height;
    image.pixels.assign(
        data, data + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(data);
    return image;
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

float srgbToLinear(const float value) {
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
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
                const float encoded = image.pixels[sourcePixel + channel] / 255.0F;
                output.rgba16f[targetPixel + channel] =
                    environmentFloatToHalf(srgbToLinear(encoded));
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
    const std::filesystem::path path(source.path);
    const bool cube = source.projection == EnvironmentProjection::CubeFaces
        || (source.projection == EnvironmentProjection::Auto
            && std::filesystem::is_directory(path));
    if (cube) {
        return loadCubeDirectory(path);
    }
    if (lowerExtension(path) == ".exr") {
        throw std::runtime_error(
            "OpenEXR is not supported by this build; use Radiance .hdr or a "
            "tonemapped PNG/JPG environment: " + path.string());
    }

    EnvironmentImage output;
    int width = 0;
    int height = 0;
    int channels = 0;
    if (lowerExtension(path) == ".hdr") {
        float* data = stbi_loadf(path.string().c_str(), &width, &height, &channels, 4);
        if (data == nullptr) {
            throw std::runtime_error("Failed to decode HDR environment: " + path.string());
        }
        const std::size_t count = static_cast<std::size_t>(width) * height * 4;
        output.rgba16f.resize(count);
        for (std::size_t index = 0; index < count; ++index) {
            output.rgba16f[index] = environmentFloatToHalf(
                std::clamp(data[index], 0.0F, 64.0F));
        }
        stbi_image_free(data);
    } else {
        const ByteImage image = loadByteImage(path);
        width = image.width;
        height = image.height;
        output.rgba16f.resize(image.pixels.size());
        for (std::size_t index = 0; index < image.pixels.size(); ++index) {
            const float encoded = image.pixels[index] / 255.0F;
            output.rgba16f[index] = environmentFloatToHalf(
                index % 4 == 3 ? encoded : srgbToLinear(encoded));
        }
    }
    output.width = static_cast<std::uint32_t>(width);
    output.height = static_cast<std::uint32_t>(height);
    output.description = path.string();
    return output;
}

}  // namespace azurerender
