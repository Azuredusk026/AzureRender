#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace azurerender {
// Immutable authored HDR levels, mip-major then array-layer-major. Coordinates
// describe the usable atlas rectangle; packed texels retain unsigned 11/11/10.
class OctahedralEnvironment {
public:
    enum class FoldZeroSign:std::uint32_t { Mathematical=0,Positive=1 };
    static constexpr std::size_t headerBytes=48,maxPayloadBytes=256U<<20;
    static constexpr unsigned maxDimension=4096,maxLayers=256;
    std::uint32_t width=1,height=1,layers=1,mipLevels=1;
    std::array<float,4> uvScaleOffset{1,1,0,0};
    FoldZeroSign foldZeroSign=FoldZeroSign::Positive;
    std::vector<std::uint32_t> texels;

    void validate() const {
        validateShape();
        for(auto word:texels)if((word&0x7c0U)==0x7c0U||((word>>11)&0x7c0U)==0x7c0U||((word>>22)&0x3e0U)==0x3e0U)
            throw std::invalid_argument("Octahedral environment requires finite packed HDR");
    }
    [[nodiscard]] std::size_t subresourceOffset(unsigned mip,unsigned layer) const {
        validateShape();if(mip>=mipLevels||layer>=layers)throw std::invalid_argument("Octahedral subresource exceeds asset range");
        std::size_t offset=0;for(unsigned i=0;i<mip;++i){const auto side=std::max(1U,width>>i);offset+=std::size_t(side)*side*layers;}
        const auto side=std::max(1U,width>>mip);return offset+std::size_t(side)*side*layer;
    }
    [[nodiscard]] std::array<float,2> directionUv(const std::array<float,3>& direction) const {
        validateShape();double magnitude=0;
        for(auto value:direction){if(!std::isfinite(value))throw std::invalid_argument("Nonfinite environment direction");magnitude+=std::abs(double(value));}
        if(!(magnitude>0))throw std::invalid_argument("Environment direction must be nonzero");
        const double x=direction[0]/magnitude,y=direction[1]/magnitude,z=direction[2]/magnitude;
        const auto sign=[&](double value){return value<0?-1.:value>0?1.:foldZeroSign==FoldZeroSign::Positive?1.:0.;};
        const double u=z<0?(1-std::abs(y))*sign(x):x,v=z<0?(1-std::abs(x))*sign(y):y;
        return {float((u*.5+.5)*uvScaleOffset[0]+uvScaleOffset[2]),float((v*.5+.5)*uvScaleOffset[1]+uvScaleOffset[3])};
    }
    [[nodiscard]] std::vector<std::byte> encode() const {
        validate();std::vector<std::byte> bytes;bytes.reserve(headerBytes+texels.size()*4);
        for(char c:std::array<char,8>{'A','Z','O','A','0','0','0','1'})bytes.push_back(std::byte(c));
        const auto put=[&](std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(std::byte((value>>shift)&255));};
        put(width);put(height);put(layers);put(mipLevels);
        for(float value:uvScaleOffset){std::uint32_t bits;std::memcpy(&bits,&value,4);put(bits);}
        put(1);put(static_cast<std::uint32_t>(foldZeroSign));for(auto word:texels)put(word);return bytes;
    }
    [[nodiscard]] static OctahedralEnvironment decode(const std::vector<std::byte>& bytes){
        if(bytes.size()<headerBytes||bytes.size()>headerBytes+maxPayloadBytes||std::memcmp(bytes.data(),"AZOA0001",8)!=0)
            throw std::invalid_argument("Invalid octahedral environment version or length");
        std::size_t cursor=8;const auto take=[&](){std::uint32_t value=0;
            for(unsigned shift=0;shift<32;shift+=8)value|=std::uint32_t(std::to_integer<unsigned char>(bytes[cursor++]))<<shift;return value;};
        OctahedralEnvironment result;result.width=take();result.height=take();result.layers=take();result.mipLevels=take();
        for(auto& value:result.uvScaleOffset){const auto bits=take();std::memcpy(&value,&bits,4);}
        if(take()!=1)throw std::invalid_argument("Unknown octahedral HDR encoding");
        result.foldZeroSign=static_cast<FoldZeroSign>(take());const auto count=result.texelCount();
        result.validateMetadata();
        if(bytes.size()!=headerBytes+count*4)throw std::invalid_argument("Octahedral payload length differs");
        result.texels.reserve(count);for(std::size_t i=0;i<count;++i)result.texels.push_back(take());result.validate();return result;
    }
    [[nodiscard]] static OctahedralEnvironment load(const std::filesystem::path& path){
        std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Cannot open octahedral environment");
        const auto length=input.tellg();if(length<static_cast<std::streamoff>(headerBytes)||length>static_cast<std::streamoff>(headerBytes+maxPayloadBytes))
            throw std::invalid_argument("Octahedral environment file exceeds bounded length");
        std::vector<std::byte> bytes(static_cast<std::size_t>(length));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("Cannot read octahedral environment");
        return decode(bytes);
    }
private:
    [[nodiscard]] std::size_t texelCount() const {
        if(width==0||height!=width||width>maxDimension||layers==0||layers>maxLayers||mipLevels==0)
            throw std::invalid_argument("Invalid octahedral dimensions or layers");
        unsigned maximum=1;for(auto side=width;side>1;side>>=1)++maximum;
        if(mipLevels>maximum)throw std::invalid_argument("Octahedral mip count exceeds dimensions");
        constexpr auto budget=maxPayloadBytes/4;std::size_t total=0;
        for(unsigned mip=0;mip<mipLevels;++mip){const auto side=std::max(1U,width>>mip);
            if(side>budget/side/layers)throw std::invalid_argument("Octahedral payload exceeds budget");
            const auto count=std::size_t(side)*side*layers;
            if(count>budget-total)throw std::invalid_argument("Octahedral payload exceeds budget");total+=count;
        }return total;
    }
    void validateMetadata() const {
        if(foldZeroSign!=FoldZeroSign::Positive&&foldZeroSign!=FoldZeroSign::Mathematical)
            throw std::invalid_argument("Unknown octahedral zero-sign convention");
        for(float value:uvScaleOffset)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite octahedral atlas region");
        for(unsigned axis=0;axis<2;++axis)if(uvScaleOffset[axis]<=0||uvScaleOffset[axis]>1||uvScaleOffset[axis+2]<0
            ||double(uvScaleOffset[axis])+uvScaleOffset[axis+2]>1)
            throw std::invalid_argument("Octahedral atlas region exceeds the unit image");
    }
    void validateShape() const {const auto count=texelCount();validateMetadata();if(texels.size()!=count)throw std::invalid_argument("Octahedral texel count differs");}
};
}
