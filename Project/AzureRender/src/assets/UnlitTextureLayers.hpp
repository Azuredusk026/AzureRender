#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace azurerender {
struct UnlitTextureLayer {
    enum class Role { Base, Mask, Blend };
    enum class SampleMode { Rgba, RedAsAlpha };
    Role role=Role::Base;
    SampleMode sampleMode=SampleMode::Rgba;
    int texture=-1;
    std::array<float,4> scaleOffset{1,1,0,0},rotation{1,0,0,1};
    std::array<float,2> speed{};
};
struct UnlitTextureLayers {
    bool present=false;
    unsigned layerCount=0;
    std::array<UnlitTextureLayer,2> layers{};
    std::array<float,4> tint{1,1,1,1},blendTint{};
    float clockRate=1,clockOffsetSeconds=0;
    float softDistance=1,softBias=0;
    float highlightThreshold=1,highlightGain=0;
    float postExposureWeight=1,globalColorWeight=1;
    bool viewDepthFadePresent=false;
    std::array<float,4> viewDepthFade{};
};
struct UnlitEffectFrame {
    float exposureGain=1,saturation=1;
    std::array<float,3> colorGain{1,1,1};
};
void validateUnlitEffectFrame(const UnlitEffectFrame&);
// Parsing returns an owned value; resource and feature compatibility is checked
// by the material loader before accepting this surface model.
UnlitTextureLayers parseUnlitTextureLayers(const nlohmann::json&,std::size_t textureCount);
double evaluateUnlitMaterialTime(const UnlitTextureLayers&,double presentationSeconds);
std::array<float,2> evaluateUnlitLayerUv(const UnlitTextureLayer&,std::array<float,2>,double materialSeconds);
float evaluateSoftIntersection(float sceneViewZ,float effectViewZ,float distance,float bias);
float evaluateUnlitViewDepthFade(const UnlitTextureLayers&,float effectViewZ);
// Samples and output are linear, straight RGBA. Node opacity is applied once.
std::array<float,4> evaluateUnlitTextureLayers(const UnlitTextureLayers&,
    std::array<float,4> baseSample,std::array<float,4> secondSample,
    float softIntersection,float nodeOpacity,const UnlitEffectFrame&,
    std::optional<float> effectViewZ=std::nullopt);
}
