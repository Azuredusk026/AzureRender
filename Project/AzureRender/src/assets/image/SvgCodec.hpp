#pragma once
#include "assets/image/ImageCodec.hpp"
namespace azurerender {
bool isSvgImage(const std::vector<unsigned char>& bytes);
DecodedImage decodeSvgImage(const std::vector<unsigned char>& bytes,const AssetCancellation& cancelled);
}
