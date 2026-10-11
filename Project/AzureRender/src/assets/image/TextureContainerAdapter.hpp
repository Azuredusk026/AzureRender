#pragma once
#include "assets/image/ImageCodec.hpp"
namespace azurerender {
bool isTextureContainer(const std::vector<unsigned char>& bytes);
DecodedImage decodeTextureContainer(const std::vector<unsigned char>& bytes,const AssetCancellation& cancelled={});
std::vector<unsigned char> encodeTextureContainer(const DecodedImage& image,const std::string& format,
    const nlohmann::json& settings,const AssetCancellation& cancelled={});
}
