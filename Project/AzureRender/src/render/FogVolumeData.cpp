#include "render/FogVolumeData.hpp"
#include <Imath/half.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace azurerender {
namespace {
constexpr std::array<std::uint8_t,8> magic{'A','Z','F','O','G','0','0','1'};
std::size_t payloadBytes(const FogVolumeData& volume){
    std::size_t count=8;
    for(auto n:volume.dimensions){if(!n||n>FogVolumeData::maxDimension)throw std::invalid_argument("Fog volume dimensions must be in [1,256]");count*=n;}
    if(count>FogVolumeData::maxPayloadBytes)throw std::invalid_argument("Fog volume exceeds the 64 MiB payload budget");
    return count;
}
void shape(const FogVolumeData& volume){if(volume.rgba16f.size()!=payloadBytes(volume)/2)throw std::invalid_argument("Fog volume payload size mismatch");}
float fromHalf(std::uint16_t bits){Imath::half value;value.setBits(bits);return float(value);}
FogTransport texel(const FogVolumeData& volume,std::size_t index){
    FogTransport result;
    for(unsigned i=0;i<3;++i)result.scattering[i]=fromHalf(volume.rgba16f[index*4+i]);
    const float t=fromHalf(volume.rgba16f[index*4+3]);result.transmittance={t,t,t};validateFogTransport(result);return result;
}
}
void FogVolumeData::validate() const {
    shape(*this);for(std::size_t i=0;i<rgba16f.size()/4;++i)(void)texel(*this,i);
}
std::vector<std::uint8_t> FogVolumeData::encode() const {
    validate();std::vector<std::uint8_t> bytes;bytes.reserve(20+rgba16f.size()*2);bytes.insert(bytes.end(),magic.begin(),magic.end());
    for(auto n:dimensions)for(unsigned b=0;b<4;++b)bytes.push_back(static_cast<std::uint8_t>(n>>(b*8)));
    for(auto bits:rgba16f){bytes.push_back(static_cast<std::uint8_t>(bits));bytes.push_back(static_cast<std::uint8_t>(bits>>8));}
    return bytes;
}
FogVolumeData FogVolumeData::decode(const std::vector<std::uint8_t>& bytes) {
    if(bytes.size()<20||bytes.size()>20+maxPayloadBytes||!std::equal(magic.begin(),magic.end(),bytes.begin()))throw std::invalid_argument("Invalid fog volume header or version");
    FogVolumeData result;
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<4;++b)result.dimensions[a]|=std::uint32_t(bytes[8+a*4+b])<<(b*8);
    const auto size=payloadBytes(result);if(bytes.size()!=20+size)throw std::invalid_argument("Fog volume has truncated or trailing payload");
    result.rgba16f.resize(size/2);for(std::size_t i=0;i<size/2;++i)result.rgba16f[i]=std::uint16_t(bytes[20+i*2])|(std::uint16_t(bytes[21+i*2])<<8);
    result.validate();return result;
}
FogVolumeData FogVolumeData::load(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::invalid_argument("Cannot open fog volume");
    const auto size=file.tellg();if(size<20||size>std::streamoff(20+maxPayloadBytes))throw std::invalid_argument("Fog volume file size exceeds its bounds");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!file)throw std::invalid_argument("Cannot read complete fog volume");return decode(bytes);
}
void FogVolumeData::save(const std::filesystem::path& path) const {
    const auto bytes=encode();std::ofstream file(path,std::ios::binary|std::ios::trunc);if(!file)throw std::invalid_argument("Cannot create fog volume");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!file)throw std::invalid_argument("Cannot write complete fog volume");
}
FogTransport FogVolumeData::sample(std::array<float,3> uvw) const {
    shape(*this);std::array<int,3> low{};std::array<double,3> fraction{};
    for(unsigned a=0;a<3;++a){if(!std::isfinite(uvw[a]))throw std::invalid_argument("Fog volume coordinates must be finite");const double p=double(std::clamp(uvw[a],0.F,1.F))*dimensions[a]-.5;low[a]=static_cast<int>(std::floor(p));fraction[a]=p-low[a];}
    std::array<double,3> scattering{};double transmission=0;
    for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){
        const unsigned corner[3]={x,y,z};std::array<std::size_t,3> index{};double weight=1;
        for(unsigned a=0;a<3;++a){index[a]=static_cast<std::size_t>(std::clamp(low[a]+static_cast<int>(corner[a]),0,static_cast<int>(dimensions[a])-1));weight*=corner[a]?fraction[a]:1-fraction[a];}
        const auto value=texel(*this,(index[2]*dimensions[1]+index[1])*dimensions[0]+index[0]);
        for(unsigned a=0;a<3;++a)scattering[a]+=weight*value.scattering[a];transmission+=weight*value.transmittance[0];
    }
    FogTransport result;for(unsigned a=0;a<3;++a){result.scattering[a]=static_cast<float>(scattering[a]);result.transmittance[a]=static_cast<float>(transmission);}return result;
}
}
