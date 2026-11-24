#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
struct IrradianceQueryFrame {
    std::uint32_t viewportHeight=0;
    std::array<float,9> worldToField{1,0,0,0,1,0,0,0,1};
};
struct IrradianceQueryParameters {
    enum class PixelOrigin : std::uint8_t { TopLeft,BottomLeft };
    enum class JitterSpace : std::uint8_t { World,Field };
    float normalOffset=0,jitterAmplitude=0,phase=0;
    PixelOrigin pixelOrigin=PixelOrigin::TopLeft;
    JitterSpace jitterSpace=JitterSpace::World;
    void validate() const {
        if(!std::isfinite(normalOffset)||normalOffset<0||!std::isfinite(jitterAmplitude)||jitterAmplitude<0
            ||!std::isfinite(phase)||std::abs(phase)>16777216.F
            ||static_cast<unsigned>(pixelOrigin)>1||static_cast<unsigned>(jitterSpace)>1)
            throw std::invalid_argument("Invalid irradiance receiver query parameters");
    }
    [[nodiscard]] unsigned enabledFlags(bool enabled) const {
        validate();return enabled?1U|(static_cast<unsigned>(pixelOrigin)<<1U)
            |(static_cast<unsigned>(jitterSpace)<<2U):0U;
    }
    [[nodiscard]] std::array<float,2> noisePixel(std::array<float,2> pixel,
        std::uint32_t viewportHeight=0) const {
        validate();
        for(float coordinate:pixel)if(!std::isfinite(coordinate)||coordinate<0||coordinate>=16777216.F)
            throw std::invalid_argument("Invalid irradiance query pixel");
        pixel={std::floor(pixel[0]),std::floor(pixel[1])};
        if(pixelOrigin==PixelOrigin::BottomLeft){
            if(viewportHeight==0||viewportHeight>16777216U||pixel[1]>=float(viewportHeight))
                throw std::invalid_argument("Irradiance pixel origin requires a valid viewport height");
            pixel[1]=float(viewportHeight-1)-pixel[1];
        }
        return pixel;
    }
    [[nodiscard]] std::array<float,3> noiseDirection(const IrradianceQueryFrame& frame) const {
        validate();
        if(jitterSpace==JitterSpace::World)return {1,1,1};
        const auto& basis=frame.worldToField;
        for(float value:basis)if(!std::isfinite(value))
            throw std::invalid_argument("Nonfinite irradiance query frame");
        for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
            float dot=0;for(unsigned c=0;c<3;++c)dot+=basis[a*3+c]*basis[b*3+c];
            if(std::abs(dot-(a==b?1.F:0.F))>1e-5F)
                throw std::invalid_argument("Irradiance query frame must be orthogonal");
        }
        return {basis[0]+basis[3]+basis[6],basis[1]+basis[4]+basis[7],basis[2]+basis[5]+basis[8]};
    }
    [[nodiscard]] std::array<float,3> queryPosition(const std::array<float,3>& surface,
        const std::array<float,3>& normal,const std::array<float,2>& pixel,
        const IrradianceQueryFrame& frame={}) const {
        validate();float length=0;
        for(unsigned axis=0;axis<3;++axis){
            if(!std::isfinite(surface[axis])||!std::isfinite(normal[axis]))
                throw std::invalid_argument("Nonfinite irradiance receiver input");
            length+=normal[axis]*normal[axis];
        }
        if(std::abs(length-1)>1e-4F)throw std::invalid_argument("Irradiance receiver requires unit world normal");
        const float noise=irradianceQuerySignedNoise(noisePixel(pixel,frame.viewportHeight),phase);
        const auto direction=noiseDirection(frame);
        if(normalOffset==0&&jitterAmplitude==0)return surface;
        auto result=surface;
        for(unsigned axis=0;axis<3;++axis){
            result[axis]=std::fma(normal[axis],normalOffset,result[axis]);
            result[axis]=std::fma(noise,jitterAmplitude*direction[axis],result[axis]);
            if(!std::isfinite(result[axis]))throw std::invalid_argument("Irradiance receiver position overflow");
        }
        return result;
    }
};
}
