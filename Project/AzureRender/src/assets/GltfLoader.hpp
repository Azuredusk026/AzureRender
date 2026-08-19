#pragma once

#include <array>
#include <cstddef>
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
    std::array<float, 3> morph0{};
    std::array<float, 3> morph1{};
};

enum class AssetAlphaMode : std::uint32_t {
    Opaque = 0,
    Mask = 1,
    Blend = 2,
};

enum class AssetMaterialClass : std::uint32_t {
    Generic = 0,
    Skin = 1,
    Face = 2,
    Hair = 3,
    Fabric = 4,
    Metal = 5,
    Eye = 6,
    Overlay = 7,
    Emissive = 8,
    Showcase = 9,
};

enum AssetMaterialFeature : std::uint32_t {
    MaterialFeatureStylizedShadow = 1U << 0U,
    MaterialFeatureHairAnisotropy = 1U << 1U,
    MaterialFeatureFaceSdfEligible = 1U << 2U,
    MaterialFeatureEmissiveMask = 1U << 3U,
    MaterialFeatureOverlay = 1U << 4U,
    MaterialFeatureNeutralFallback = 1U << 5U,
    MaterialFeatureBrowOverlay = 1U << 6U,
};

enum class AssetFaceSdfChannel : std::uint32_t {
    Red = 0,
    Green = 1,
    Blue = 2,
    Alpha = 3,
};

struct AssetFaceSdfProfile {
    static constexpr std::uint32_t kSchemaVersion = 1;

    bool present = false;
    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    AssetFaceSdfChannel channel = AssetFaceSdfChannel::Red;
    AssetFaceSdfChannel maskChannel = AssetFaceSdfChannel::Alpha;
    bool shadowOnLowValues = true;
    bool mirrorHorizontal = false;
    std::uint32_t headNode = 0;
    std::string headNodeName;
};

struct AssetMaterial {
    std::string name = "FallbackMaterial";
    AssetMaterialClass materialClass = AssetMaterialClass::Generic;
    std::uint32_t materialFeatures = MaterialFeatureNeutralFallback;
    std::uint32_t materialProfileVersion = 1;
    bool materialProfileExplicit = false;
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
    // toon, shadow tint, specular, rim
    std::array<float, 4> styleParameters{1.0F, 1.0F, 1.0F, 1.0F};
    // outline, hair highlight, emissive, face overlay
    std::array<float, 4> featureParameters{1.0F, 1.0F, 1.0F, 1.0F};
    AssetFaceSdfProfile faceSdf;
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

struct AssetNode {
    std::string name;
    std::int32_t parent = -1;
    std::array<float, 3> translation{};
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
    std::array<float, 16> localMatrix{};
    bool usesMatrix = false;
};

enum class AssetAnimationPath : std::uint32_t {
    Translation,
    Rotation,
    Scale,
};

enum class AssetAnimationInterpolation : std::uint32_t {
    Step,
    Linear,
};

struct AssetAnimationSampler {
    std::vector<float> inputTimes;
    std::vector<std::array<float, 4>> outputValues;
    AssetAnimationInterpolation interpolation =
        AssetAnimationInterpolation::Linear;
};

struct AssetAnimationChannel {
    std::uint32_t samplerIndex = 0;
    std::uint32_t nodeIndex = 0;
    AssetAnimationPath path = AssetAnimationPath::Translation;
};

struct AssetAnimation {
    std::string name;
    std::vector<AssetAnimationSampler> samplers;
    std::vector<AssetAnimationChannel> channels;
    float startTime = 0.0F;
    float endTime = 0.0F;
};

struct LoadedAsset {
    std::vector<AssetVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<AssetMaterial> materials;
    std::vector<AssetPrimitive> primitives;
    std::array<float, 3> boundsMin{};
    std::array<float, 3> boundsMax{};
    std::vector<AssetNode> nodes;
    std::vector<std::uint32_t> jointNodes;
    std::vector<std::array<float, 16>> inverseBindMatrices;
    std::vector<std::array<float, 16>> jointMatrices;
    std::vector<AssetAnimation> animations;
    std::vector<std::array<float, 16>> nodeWorldMatrices;
    bool hasSkin = false;
    std::uint32_t morphTargetCount = 0;
};

[[nodiscard]] LoadedAsset loadGltfAsset(const std::string& path);
[[nodiscard]] const char* assetMaterialClassName(AssetMaterialClass value);
void sampleAnimation(
    LoadedAsset& asset,
    std::size_t animationIndex,
    float time,
    std::vector<std::array<float, 16>>& jointMatrices);
