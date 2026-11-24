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
struct IrradianceSample {
    // RGB, each containing world linear X/Y/Z and DC. Values include validity weight.
    std::array<std::array<float,4>,3> coefficients{};
    float weight=0;
    [[nodiscard]] std::array<float,3> evaluate(const std::array<float,3>& normal) const {
        float length=0;
        for(auto n:normal){if(!std::isfinite(n))throw std::invalid_argument("Nonfinite irradiance normal");length+=n*n;}
        if(std::abs(length-1)>1e-4F)throw std::invalid_argument("Irradiance requires unit world normal");
        std::array<float,3> result{};
        for(unsigned c=0;c<3;++c){
            auto value=coefficients[c][3];
            for(unsigned a=0;a<3;++a)value+=coefficients[c][a]*normal[a];
            result[c]=std::max(value,0.F);
        }
        return result;
    }
};
struct IrradianceCell {
    // Unweighted coefficients. Zero validity leaves the environment contribution intact.
    std::array<std::array<float,4>,3> coefficients{};
    float weight=0;
};
class IrradianceVolume {
public:
    static constexpr std::size_t maxSamples=1U<<20;
    static constexpr std::size_t headerBytes=36, cellBytes=64;
    std::array<float,3> origin{};
    float spacing=1;
    std::array<std::uint32_t,3> dimensions{1,1,1};
    std::vector<IrradianceCell> cells;
    void validate() const {
        validateGrid();
        for(const auto& cell:cells){
            if(!std::isfinite(cell.weight)||cell.weight<0||cell.weight>1)
                throw std::invalid_argument("Invalid irradiance validity weight");
            for(const auto& color:cell.coefficients)for(auto f:color)
                if(!std::isfinite(f))throw std::invalid_argument("Nonfinite irradiance coefficient");
        }
    }
    [[nodiscard]] IrradianceSample query(const std::array<float,3>& position) const {
        validateGrid();
        std::array<unsigned,3> lo{},hi{};std::array<float,3> fraction{};
        for(unsigned a=0;a<3;++a){
            if(!std::isfinite(position[a]))throw std::invalid_argument("Nonfinite irradiance position");
            const auto q=(position[a]-origin[a])/spacing;
            if(q<0||q>float(dimensions[a]-1))return {};
            lo[a]=unsigned(std::floor(q));hi[a]=std::min(lo[a]+1,dimensions[a]-1);fraction[a]=q-float(lo[a]);
        }
        IrradianceSample result;
        for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){
            const auto ix=x?hi[0]:lo[0],iy=y?hi[1]:lo[1],iz=z?hi[2]:lo[2];
            const auto& cell=cells[ix+std::size_t(dimensions[0])*(iy+std::size_t(dimensions[1])*iz)];
            const auto w=(x?fraction[0]:1-fraction[0])*(y?fraction[1]:1-fraction[1])
                *(z?fraction[2]:1-fraction[2])*cell.weight;
            result.weight+=w;
            for(unsigned c=0;c<3;++c)for(unsigned a=0;a<4;++a)result.coefficients[c][a]+=w*cell.coefficients[c][a];
        }
        result.weight=std::clamp(result.weight,0.F,1.F);
        return result;
    }
    [[nodiscard]] std::vector<std::byte> encode() const {
        validate();std::vector<std::byte> bytes;
        for(char c:std::array<char,8>{'A','Z','I','V','0','0','0','1'})bytes.push_back(std::byte(c));
        auto put=[&](std::uint32_t v){for(unsigned s=0;s<32;s+=8)bytes.push_back(std::byte((v>>s)&255));};
        auto putFloat=[&](float f){std::uint32_t v;std::memcpy(&v,&f,4);put(v);};
        for(auto f:origin)putFloat(f);putFloat(spacing);for(auto d:dimensions)put(d);
        for(const auto& cell:cells){
            for(const auto& color:cell.coefficients)for(auto f:color)putFloat(f);
            putFloat(cell.weight);put(0);put(0);put(0);
        }
        return bytes;
    }
    [[nodiscard]] static IrradianceVolume decode(const std::vector<std::byte>& bytes) {
        if(bytes.size()<headerBytes||std::memcmp(bytes.data(),"AZIV0001",8)!=0)
            throw std::invalid_argument("Invalid irradiance volume header");
        std::size_t offset=8;
        auto get=[&](){std::uint32_t v=0;for(unsigned s=0;s<32;s+=8)v|=std::uint32_t(bytes[offset++])<<s;return v;};
        auto getFloat=[&](){auto v=get();float f;std::memcpy(&f,&v,4);return f;};
        IrradianceVolume result;
        for(auto& f:result.origin)f=getFloat();result.spacing=getFloat();for(auto& d:result.dimensions)d=get();
        const auto count=result.sampleCount();
        if(bytes.size()!=headerBytes+cellBytes*count)throw std::invalid_argument("Irradiance volume size differs");
        result.cells.resize(count);
        for(auto& cell:result.cells){
            for(auto& color:cell.coefficients)for(auto& f:color)f=getFloat();
            cell.weight=getFloat();for(unsigned i=0;i<3;++i)if(get()!=0)throw std::invalid_argument("Invalid irradiance reserved field");
        }
        result.validate();return result;
    }
    [[nodiscard]] static IrradianceVolume load(const std::filesystem::path& path) {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input)throw std::invalid_argument("Cannot open irradiance volume");
        const auto size=input.tellg();
        if(size<0||std::uint64_t(size)>headerBytes+maxSamples*cellBytes)
            throw std::invalid_argument("Irradiance volume exceeds budget");
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))
            throw std::invalid_argument("Cannot read irradiance volume");
        return decode(bytes);
    }
private:
    [[nodiscard]] std::size_t sampleCount() const {
        std::size_t count=1;
        for(auto d:dimensions){if(d==0||d>maxSamples/count)throw std::invalid_argument("Irradiance dimensions exceed budget");count*=d;}
        return count;
    }
    void validateGrid() const {
        if(!std::isfinite(spacing)||spacing<=0||sampleCount()!=cells.size())
            throw std::invalid_argument("Invalid irradiance grid");
        for(unsigned a=0;a<3;++a)
            if(!std::isfinite(origin[a])||!std::isfinite(origin[a]+spacing*float(dimensions[a]-1)))
                throw std::invalid_argument("Invalid irradiance grid extent");
    }
};
}
