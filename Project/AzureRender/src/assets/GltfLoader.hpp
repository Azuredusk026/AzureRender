#pragma once

#include "assets/TextureImportSettings.hpp"
#include "assets/MorphTargetData.hpp"
#include "animation/AnimationCurve.hpp"
#include "animation/AnimationClipSample.hpp"
#include "assets/image/ImageCodec.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
    float browMask = 0.0F;
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
    MaterialFeatureSurfaceAo = 1U << 7U,
    MaterialFeatureSceneTint = 1U << 8U,
    MaterialFeatureHairRamp = 1U << 9U,
    MaterialFeatureSceneRefraction = 1U << 10U,
    MaterialFeatureDualFaceSdf = 1U << 11U,
    MaterialFeatureShadowColorLut = 1U << 12U,
    MaterialFeatureDiffuseRamp = 1U << 13U,
    MaterialFeatureSilhouetteOutline = 1U << 14U,
    MaterialFeatureMultiplyComposite = 1U << 15U,
    MaterialFeatureHairProjectedRamp = 1U << 16U,
    MaterialFeatureEyeRadialSurface = 1U << 17U,
    MaterialFeatureUnlitSurface = 1U << 18U,
    MaterialFeatureNprDiffuse = 1U << 19U,
    MaterialFeatureExplicitHairFrame = 1U << 20U,
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
    std::uint32_t schemaVersion = kSchemaVersion;
    std::array<float,9> headBasis{1,0,0,0,1,0,0,0,1};
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

struct AssetMaterialFrame {
    bool present=false;
    std::uint32_t node=0;
    std::string nodeName;
    std::array<float,9> basis{1,0,0,0,1,0,0,0,1};
};

struct AssetMaterial {
    std::array<unsigned,9> authoredMipLevels{};
    std::array<azurerender::TextureImportSettings,9> textureSettings=[] {
        std::array<azurerender::TextureImportSettings,9> result;
        const char* usage[]={"albedo","normal","roughness","emissive","mask","albedo","packed-normal","linear","albedo"};
        for(std::size_t slot=0;slot<result.size();++slot) {
            result[slot].usage=usage[slot];
            result[slot].colorSpace=slot==0||slot==3||slot==5||slot==8?"srgb":"linear";
        }
        result[8].mipPolicy="none";result[8].wrapU=result[8].wrapV="clamp";result[8].anisotropy=1;
        return result;
    }();
    std::string name = "FallbackMaterial";
    std::string slotId;
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
    // Horizontal blue-slice strip: width = height squared, sRGB encoded.
    std::vector<std::uint8_t> shadowColorLutPixels{255,255,255,255};
    std::uint32_t shadowColorLutWidth=1,shadowColorLutHeight=1;
    float transmission=0,ior=1.5F,thickness=0,attenuationDistance=0;
    std::array<float,3> attenuationColor{1,1,1};
    std::vector<std::uint8_t> transmissionPixels{255,255,255,255};
    std::uint32_t transmissionWidth=1,transmissionHeight=1;
    float emissiveStrength = 0.0F;
    std::array<float, 4> aoColor{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> lamShadowColor{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> matcapColor{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> hairParameters{64.0F, 0.15F, 4.0F, 0.0F};
    std::array<float,4> hairViewParameters{0,0,0,1};
    struct ProjectedHairRamp {
        enum class SurfaceResponse : std::uint8_t { Layered, StrandDiffuse };
        bool present=false;
        AssetMaterialFrame frame;
        bool virtualLight=false;
        float virtualBlend=1,objectGain=1;
        struct SpecularLine {
            bool present=false;
            bool mainDiffuse=false;
            float frequency=1,strandShift=0,range=1,depth=0,alternation=1,saturation=1;
            std::array<float,4> uvTransform{1,1,0,0};
            std::vector<std::uint8_t> pixels;
            std::uint32_t width=0,height=0;
        } line;
        float primaryShift=0,secondaryShift=0,intensity=1,secondaryRange=.5F;
        float edgeFade=1,specularNormalScale=1,directionX=0,surfaceNormalScale=1;
        std::array<float,3> secondaryColor{1,1,1};
        SurfaceResponse surfaceResponse=SurfaceResponse::Layered;
        float diffuseScale=1;
        float tangentSign=1;
        struct Lighting {
            bool present=false;
            float shadowBrightness=1,shadowSaturation=1,shadowScale=1;
            std::array<float,3> ambientColor{1,1,1},mainColor{1,1,1};
        } lighting;
        // Column-major source object axes expressed in the asset frame.
        std::array<float,9> objectBasis{1,0,0,0,1,0,0,0,1};
        std::vector<std::uint8_t> parameterPixels;
        std::uint32_t parameterWidth=0,parameterHeight=0;
    } projectedHairRamp;
    struct EyeSurface {
        bool present=false;
        float parallaxScale=0,matcapNormalScale=0,alphaPremultiply=0,matcapShadowFloor=0;
        std::array<float,3> highlightColor{},scatteringColor{},directWeights{};
        std::array<float,4> matcapColor{},uvScaleOffset{1,1,0,0};
        // Version 1 accepts explicit host-authored, host-lambert lighting.
        // Native capture bindings require a separate provenance contract.
    } eyeSurface;
    struct NprDiffuse {
        enum class IndirectNormal : std::uint8_t { Surface, WorldHorizontal, FaceFrameRadialHorizontal };
        bool present=false;
        float shadowScale=1;
        bool styleMaskGb=false,albedoAlpha=false;
        IndirectNormal indirectNormal=IndirectNormal::Surface;
        struct Lighting {
            bool present=false;
            std::array<float,3> ambientColor{1,1,1},mainColor{1,1,1};
        } lighting;
    } nprDiffuse;
    struct SpecularInput {
        enum class Source : std::uint8_t { EmissiveAlpha, MetallicRoughnessAlpha };
        bool present=false;
        Source source=Source::EmissiveAlpha;
        float scale=1;
    } specularInput;
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
    std::uint32_t nodeIndex = std::numeric_limits<std::uint32_t>::max();
};

struct AssetNode {
    std::string name;
    std::int32_t parent = -1;
    std::array<float, 3> translation{};
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
    std::array<float, 16> localMatrix{};
    bool usesMatrix = false;
    float opacity = 1.0F;
};

enum class AssetAnimationPath : std::uint32_t {
    Translation,
    Rotation,
    Scale,
    Weights,
    Opacity,
};

enum class AssetAnimationInterpolation : std::uint32_t {
    Step,
    Linear,
    CubicSpline,
};

struct AssetAnimationSampler {
    std::vector<float> inputTimes;
    std::vector<std::array<float, 4>> outputValues;
    AssetAnimationInterpolation interpolation =
        AssetAnimationInterpolation::Linear;
    AnimationCurve curve;
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
    std::vector<MorphTargetRange> morphRanges;
    std::vector<MorphWeightState> defaultMorphWeights;
};

[[nodiscard]] LoadedAsset loadGltfAsset(const std::string& path);
struct AssetImage {std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;std::vector<std::string> losses;std::vector<azurerender::ImageMipLevel> mipLevels;std::string sourceCompression;};
[[nodiscard]] AssetImage loadAssetImage(const std::string& path);
// Candidate validation consumes embedded data without filesystem access.
[[nodiscard]] LoadedAsset loadEmbeddedGltfAsset(const std::string& bytes,bool binary=false);
// Material-only documents share the glTF decoder and embedded-resource boundary.
// Optional raw import settings are indexed by source image; null entries use role defaults.
[[nodiscard]] std::vector<AssetMaterial> loadEmbeddedGltfMaterials(const std::string& bytes,bool binary=false,
    const std::vector<nlohmann::json>& imageSettings={});
[[nodiscard]] const char* assetMaterialClassName(AssetMaterialClass value);
struct AssetNodePose {
    std::array<float, 3> translation{};
    std::array<float, 4> rotation{0, 0, 0, 1};
    std::array<float, 3> scale{1, 1, 1};
    float opacity = 1.0F;
};
struct AssetPose {
    std::vector<AssetNodePose> local;
    std::vector<std::array<float, 16>> nodeWorldMatrices;
    std::vector<std::array<float, 16>> jointMatrices;
    std::vector<MorphWeightState> morphWeights;
    std::vector<float> nodeWorldOpacities;
};
[[nodiscard]] AssetPose bindAnimationPose(const LoadedAsset& asset);
void resolveAssetPose(const LoadedAsset& asset,AssetPose& pose);
[[nodiscard]] AssetPose sampleAnimationPose(const LoadedAsset& asset, std::size_t animationIndex, float time, bool loop = true);
[[nodiscard]] AssetPose blendAnimationPoses(const LoadedAsset& asset, const AssetPose& from, const AssetPose& to, float weight);
[[nodiscard]] AssetPose sampleAnimationBlend(const LoadedAsset& asset,
    const std::vector<azurerender::AnimationClipSample>& samples);
void sampleAnimation(
    const LoadedAsset& asset,
    std::size_t animationIndex,
    float time,
    std::vector<std::array<float, 16>>& jointMatrices);
