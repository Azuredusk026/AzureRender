#pragma once
#include "render/FogViewProjection.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace azurerender::test {
struct FogProjectionSample {
    std::array<std::uint32_t,2> pixel{};
    float viewDepth=0;
    std::array<float,4> projected{},rgba{};
};
struct FogProjectionFixture {FogViewProjection profile;std::vector<FogProjectionSample> samples;};
inline FogProjectionFixture readFogProjectionFixture(const std::filesystem::path& path){
    std::ifstream file(path);if(!file)throw std::invalid_argument("Cannot open fog projection fixture");FogProjectionFixture result;auto& p=result.profile;unsigned origin=0;
    if(!(file>>p.extent[0]>>p.extent[1]>>origin>>p.framePhase>>p.pixelCenterOffset>>p.jitterRadius
        >>p.depthEncoding[0]>>p.depthEncoding[1]>>p.depthEncoding[2]>>p.minimumViewDepth>>p.nearFadeWidth)||origin>1)throw std::invalid_argument("Invalid fog projection fixture header");
    p.pixelOrigin=origin?FogViewProjection::PixelOrigin::LowerLeft:FogViewProjection::PixelOrigin::UpperLeft;p.validate();FogProjectionSample s;
    while(file>>s.pixel[0]){
        if(!(file>>s.pixel[1]>>s.viewDepth>>s.projected[0]>>s.projected[1]>>s.projected[2]>>s.projected[3]>>s.rgba[0]>>s.rgba[1]>>s.rgba[2]>>s.rgba[3]))throw std::invalid_argument("Truncated fog projection fixture sample");
        if(result.samples.size()>=64)throw std::invalid_argument("Fog projection fixture sample budget exceeded");result.samples.push_back(s);
    }
    if(!file.eof()||result.samples.empty())throw std::invalid_argument("Invalid fog projection fixture payload");return result;
}
}
