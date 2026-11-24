#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace azurerender {
enum class ScreenSpaceReflectionAlgorithm {Linear,Projected};
inline const char* screenSpaceReflectionAlgorithmName(ScreenSpaceReflectionAlgorithm algorithm){
    if(algorithm==ScreenSpaceReflectionAlgorithm::Linear)return "linear";
    if(algorithm==ScreenSpaceReflectionAlgorithm::Projected)return "projected";
    throw std::invalid_argument("Unknown screen reflection algorithm");
}
inline ScreenSpaceReflectionAlgorithm parseScreenSpaceReflectionAlgorithm(const std::string& name){
    if(name=="linear")return ScreenSpaceReflectionAlgorithm::Linear;
    if(name=="projected")return ScreenSpaceReflectionAlgorithm::Projected;
    throw std::invalid_argument("Unknown screen reflection algorithm");
}
struct ScreenSpaceReflectionSettings {
    static constexpr std::uint32_t kSchemaVersion=2;
    bool enabled=false;
    float maxDistance=20.F; // World metres.
    float thickness=.15F; // Positive camera-forward linear depth in metres.
    float normalBias=.03F; // World metres along the shaded normal.
    std::uint32_t maxSteps=96;
    float maxRoughness=.7F;
    float edgeFade=.08F; // Normalized screen distance to the closest edge.
    float strength=1.F;
    ScreenSpaceReflectionAlgorithm algorithm=ScreenSpaceReflectionAlgorithm::Linear;
};
inline void validateScreenSpaceReflection(const ScreenSpaceReflectionSettings& settings){
    (void)screenSpaceReflectionAlgorithmName(settings.algorithm);
    const auto range=[](float value,float low,float high,const char* name){
        if(!std::isfinite(value)||value<low||value>high)
            throw std::invalid_argument(std::string("screenSpaceReflection.")+name+" outside legal domain");
    };
    range(settings.maxDistance,.1F,200.F,"maxDistance");
    range(settings.thickness,.001F,2.F,"thickness");
    range(settings.normalBias,.001F,.5F,"normalBias");
    range(settings.maxRoughness,.05F,1.F,"maxRoughness");
    range(settings.edgeFade,.001F,.5F,"edgeFade");
    range(settings.strength,0.F,1.F,"strength");
    if(settings.maxSteps<8||settings.maxSteps>256)
        throw std::invalid_argument("screenSpaceReflection.maxSteps must be within 8 and 256");
}
// Shared producer/consumer gate; non-specular isolation needs no SSR snapshot.
inline bool screenSpaceReflectionActive(const ScreenSpaceReflectionSettings& settings,
    bool sceneLighting,std::uint32_t isolation,bool specularEnabled) noexcept {
    return settings.enabled&&settings.strength>0&&sceneLighting&&specularEnabled
        &&(isolation==0||isolation==6||isolation==20);
}
}
