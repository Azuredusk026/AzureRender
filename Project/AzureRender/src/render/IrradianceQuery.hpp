#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace azurerender {
// Pixel coordinates use a top-left origin. Phase truncation follows the source
// interleaved-gradient sequence; the caller owns its progression.
inline float irradianceQuerySignedNoise(std::array<float,2> pixel,float phase){
    if(!std::isfinite(phase)||std::abs(phase)>16777216.F)
        throw std::invalid_argument("Irradiance query phase exceeds finite integer precision");
    for(auto& coordinate:pixel){
        if(!std::isfinite(coordinate)||coordinate<0||coordinate>=16777216.F)
            throw std::invalid_argument("Invalid irradiance query pixel");
        coordinate=std::floor(coordinate);
    }
    phase=std::trunc(phase);
    const float x=std::fma(phase,0x1.0a9fbep+1F,pixel[0]);
    const float y=std::fma(phase,0x1.377ceep+2F,pixel[1]);
    float gradient=std::fma(y,0x1.7e8b2p-8F,x*0x1.12e286p-4F);
    gradient-=std::floor(gradient);
    gradient*=0x1.a7dd04p+5F;
    return std::fma(gradient-std::floor(gradient),2.F,-1.F);
}
struct IrradianceQueryParameters {
    float normalOffset=0,jitterAmplitude=0,phase=0;
    void validate() const {
        if(!std::isfinite(normalOffset)||normalOffset<0||!std::isfinite(jitterAmplitude)||jitterAmplitude<0
            ||!std::isfinite(phase)||std::abs(phase)>16777216.F)
            throw std::invalid_argument("Invalid irradiance receiver query parameters");
    }
    [[nodiscard]] std::array<float,3> queryPosition(const std::array<float,3>& surface,
        const std::array<float,3>& normal,const std::array<float,2>& pixel) const {
        validate();float length=0;
        for(unsigned axis=0;axis<3;++axis){
            if(!std::isfinite(surface[axis])||!std::isfinite(normal[axis]))
                throw std::invalid_argument("Nonfinite irradiance receiver input");
            length+=normal[axis]*normal[axis];
        }
        if(std::abs(length-1)>1e-4F)throw std::invalid_argument("Irradiance receiver requires unit world normal");
        const float noise=irradianceQuerySignedNoise(pixel,phase);
        if(normalOffset==0&&jitterAmplitude==0)return surface;
        auto result=surface;
        for(unsigned axis=0;axis<3;++axis){
            result[axis]=std::fma(normal[axis],normalOffset,result[axis]);
            result[axis]=std::fma(noise,jitterAmplitude,result[axis]);
            if(!std::isfinite(result[axis]))throw std::invalid_argument("Irradiance receiver position overflow");
        }
        return result;
    }
};
}
