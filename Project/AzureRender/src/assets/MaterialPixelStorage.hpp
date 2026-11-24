#pragma once
#include "assets/GltfLoader.hpp"
#include "foundation/ImmutableBytes.hpp"

namespace azurerender {
std::vector<std::uint8_t>& materialPixelBytes(AssetMaterial&,AssetMaterial::PixelKind);
const std::vector<std::uint8_t>& materialPixelBytes(const AssetMaterial&,AssetMaterial::PixelKind);
std::uint64_t materialPixelLogicalBytes(const AssetMaterial&,AssetMaterial::PixelKind);
void prepareMaterialPixels(AssetMaterial&,ImmutableBytePool&,std::size_t minimumBytes=64*1024);
void restoreMaterialPixel(AssetMaterial&,AssetMaterial::PixelKind);
void restoreMaterialPixels(AssetMaterial&);
}
