#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct AssetVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{0.0F, 1.0F, 0.0F};
    std::array<float, 4> tangent{1.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 2> texcoord{};
    std::array<std::uint32_t, 4> joints{};
    std::array<float, 4> weights{1.0F, 0.0F, 0.0F, 0.0F};
};

enum class AssetAlphaMode : std::uint32_t {
    Opaque = 0,
    Mask = 1,
    Blend = 2,
};

struct AssetMaterial {
    std::vector<std::uint8_t> baseColorPixels;
    std::uint32_t baseColorWidth = 0;
    std::uint32_t baseColorHeight = 0;
    std::vector<std::uint8_t> normalPixels;
    std::uint32_t normalWidth = 0;
    std::uint32_t normalHeight = 0;
    std::vector<std::uint8_t> metallicRoughnessPixels;
    std::uint32_t metallicRoughnessWidth = 0;
    std::uint32_t metallicRoughnessHeight = 0;
    std::vector<std::uint8_t> specularEmissivePixels;
    std::uint32_t specularEmissiveWidth = 0;
    std::uint32_t specularEmissiveHeight = 0;
    std::vector<std::uint8_t> styleMaskPixels;
    std::uint32_t styleMaskWidth = 0;
    std::uint32_t styleMaskHeight = 0;
    std::vector<std::uint8_t> matcapPixels;
    std::uint32_t matcapWidth = 0;
    std::uint32_t matcapHeight = 0;
    std::vector<std::uint8_t> hairDataPixels;
    std::uint32_t hairDataWidth = 0;
    std::uint32_t hairDataHeight = 0;
    float emissiveStrength = 0.0F;
    std::array<float, 4> aoColor{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> lamShadowColor{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> matcapColor{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> hairParameters{64.0F, 0.15F, 4.0F, 0.0F};
    float showcasePlatform = 0.0F;
    AssetAlphaMode alphaMode = AssetAlphaMode::Opaque;
    float alphaCutoff = 0.5F;
    bool doubleSided = false;
};

struct AssetPrimitive {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t materialIndex = 0;
    std::array<float, 3> center{};
};

struct LoadedAsset {
    std::vector<AssetVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<AssetMaterial> materials;
    std::vector<AssetPrimitive> primitives;
    std::array<float, 3> boundsMin{};
    std::array<float, 3> boundsMax{};
    std::vector<std::array<float, 16>> jointMatrices;
    bool hasSkin = false;
};

[[nodiscard]] LoadedAsset loadGltfAsset(const std::string& path);
