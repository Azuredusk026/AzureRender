#include "render/FogViewProjection.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace azurerender {
namespace {
void finite(float value){if(!std::isfinite(value))throw std::invalid_argument("Fog view projection requires finite values");}
std::array<float,2> pixelJitter(std::uint32_t x,std::uint32_t y,std::uint32_t phase){
    std::uint32_t a=x*1664525U+1013904223U,b=y*1664525U+1013904223U,c=phase*1664525U+1013904223U;
    a+=b*c;b+=c*a;c+=a*b;a+=b*c;b+=c*a;
    return {float(a>>16U)/32768.F-1,float(b>>16U)/32768.F-1};
}
}
void FogViewProjection::validate() const {
    for(auto n:extent)if(!n||n>16384)throw std::invalid_argument("Fog viewport dimensions must be in [1,16384]");
    if(framePhase>7||(pixelOrigin!=PixelOrigin::UpperLeft&&pixelOrigin!=PixelOrigin::LowerLeft))throw std::invalid_argument("Fog projection phase or origin is invalid");
    for(float value:{pixelCenterOffset,jitterRadius,minimumViewDepth,nearFadeWidth})finite(value);for(float value:depthEncoding)finite(value);
    if(pixelCenterOffset<0||pixelCenterOffset>1||jitterRadius<0||jitterRadius>16||minimumViewDepth<0||nearFadeWidth<=0
        ||depthEncoding[0]<0||depthEncoding[1]<=0||depthEncoding[2]<=0)throw std::invalid_argument("Fog projection parameters are outside their domain");
}
PackedFogViewProjection FogViewProjection::pack() const {
    validate();PackedFogViewProjection result;
    result.rows[0]={1.F/extent[0],1.F/extent[1],jitterRadius,pixelCenterOffset};
    result.rows[1]={depthEncoding[0],depthEncoding[1],depthEncoding[2],minimumViewDepth};
    result.rows[2]={nearFadeWidth,float(extent[0]),float(extent[1]),float(framePhase|(pixelOrigin==PixelOrigin::LowerLeft?8U:0U))};return result;
}
std::array<float,4> FogViewProjection::coordinates(std::array<std::uint32_t,2> pixel,float depth) const {
    validate();finite(depth);if(pixel[0]>=extent[0]||pixel[1]>=extent[1])throw std::invalid_argument("Fog pixel is outside its viewport");
    if(pixelOrigin==PixelOrigin::LowerLeft)pixel[1]=extent[1]-1-pixel[1];
    const auto jitter=pixelJitter(pixel[0],pixel[1],framePhase);const float distance=std::abs(depth);
    const float encoded=std::fma(distance,depthEncoding[0],depthEncoding[1]);finite(encoded);
    std::array<float,4> result{
        std::fma(jitter[0],jitterRadius,float(pixel[0])+pixelCenterOffset)*(1.F/extent[0]),
        std::fma(jitter[1],jitterRadius,float(pixel[1])+pixelCenterOffset)*(1.F/extent[1]),
        std::log2(encoded)*depthEncoding[2],std::clamp((distance-minimumViewDepth)/nearFadeWidth,0.F,1.F)};
    for(float value:result)finite(value);return result;
}
}
