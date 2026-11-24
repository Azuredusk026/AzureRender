#pragma once
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace azurerender {
enum class IrradianceResourceKind {Grid,Clipmap};
inline IrradianceResourceKind readIrradianceResourceKind(const std::filesystem::path& path){
    std::array<char,8> header{};std::ifstream file(path,std::ios::binary);
    if(!file.read(header.data(),header.size()))throw std::invalid_argument("Cannot read irradiance resource header");
    if(std::memcmp(header.data(),"AZIV0001",8)==0)return IrradianceResourceKind::Grid;
    if(std::memcmp(header.data(),"AZIC0001",8)==0)return IrradianceResourceKind::Clipmap;
    throw std::invalid_argument("Unknown irradiance resource version");
}
}
