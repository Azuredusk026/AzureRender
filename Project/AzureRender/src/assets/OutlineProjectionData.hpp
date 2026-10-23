#pragma once
#include "assets/GltfLoader.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace azurerender {
struct alignas(16) PackedOutlineProjectionRecord {
    static constexpr std::uint32_t kLayoutVersion=1;
    std::array<float,4> parameters{};
    std::array<std::uint32_t,4> indices{};
    std::array<float,4> uvScaleOffset{};
    std::array<float,4> color{};
};
static_assert(sizeof(PackedOutlineProjectionRecord)==64);
static_assert(offsetof(PackedOutlineProjectionRecord,indices)==16);
static_assert(offsetof(PackedOutlineProjectionRecord,uvScaleOffset)==32);
static_assert(offsetof(PackedOutlineProjectionRecord,color)==48);

inline std::uint64_t outlineProjectionStorageBytes(std::uint64_t materials,std::uint64_t coordinates) {
    constexpr std::uint64_t maximum=std::uint64_t{std::numeric_limits<std::uint32_t>::max()}*4;
    if(materials>(maximum-16)/64)throw std::invalid_argument("Outline material table exceeds word addressing");
    const auto prefix=16+materials*64;
    if(coordinates>(maximum-prefix)/8)throw std::invalid_argument("Outline UV1 table exceeds word addressing");
    return prefix+coordinates*8;
}

struct OutlineProjectionData {
    std::array<std::uint32_t,4> header{};
    std::vector<PackedOutlineProjectionRecord> records;
    std::vector<std::array<float,2>> texcoord1;
    // Borrowed until the upload consumes the pixels. The loading host keeps
    // these asset materials alive and serializes all mutations.
    std::vector<AssetMaterial::OutlineSurface*> masks;
    std::uint64_t byteSize() const {return outlineProjectionStorageBytes(records.size(),texcoord1.size());}
};

inline void validateOutlineProjectionShape(const AssetMaterial& material) {
    const auto& shape=material.outlineSurface;
    if(!shape.present)return;
    const auto range=[&](float value,float low,float high) {
        if(!std::isfinite(value)||value<low||value>high)
            throw std::invalid_argument(material.name+" outlineSurface parameter outside legal range");
    };
    if(material.alphaMode==AssetAlphaMode::Blend||material.materialClass==AssetMaterialClass::Overlay)
        throw std::invalid_argument(material.name+" outlineSurface requires an opaque or masked surface");
    range(shape.width,0,8);range(shape.minimumPixels,0,8);range(shape.depthOffset,-4,4);
    for(float value:shape.uvScaleOffset)range(value,-65536,65536);
    for(float value:shape.color)range(value,0,64);
    if(shape.normalSource!=AssetMaterial::OutlineSurface::NormalSource::Geometry
        &&shape.normalSource!=AssetMaterial::OutlineSurface::NormalSource::TangentUv1
        &&shape.normalSource!=AssetMaterial::OutlineSurface::NormalSource::TangentOctahedralUv1)
        throw std::invalid_argument(material.name+" outlineSurface has an unknown normal source");
    const auto pixels=std::uint64_t{shape.maskWidth}*shape.maskHeight;
    if(!pixels||pixels>std::numeric_limits<std::size_t>::max()/4||shape.maskPixels.size()!=pixels*4)
        throw std::invalid_argument(material.name+" outlineSurface mask dimensions do not match pixels");
}

inline OutlineProjectionData buildOutlineProjectionData(const std::vector<LoadedAsset*>& assets,
    std::uint64_t maximumBytes=0) {
    std::uint64_t materialCount=0,coordinateCount=0;
    std::vector<bool> usesCoordinates;usesCoordinates.reserve(assets.size());
    for(const auto* asset:assets) {
        if(!asset)throw std::invalid_argument("Outline asset input must be present");
        validateOutlineMaterialInputs(*asset);
        materialCount+=asset->materials.size();
        bool used=false;
        for(const auto& material:asset->materials) {
            validateOutlineProjectionShape(material);
            used|=material.outlineSurface.present
                &&material.outlineSurface.normalSource!=AssetMaterial::OutlineSurface::NormalSource::Geometry;
        }
        if(used)coordinateCount+=asset->vertices.size();
        usesCoordinates.push_back(used);
        // Check after each addition, before allocating the flattened table.
        static_cast<void>(outlineProjectionStorageBytes(materialCount,coordinateCount));
    }
    const auto bytes=outlineProjectionStorageBytes(materialCount,coordinateCount);
    if(!materialCount||(maximumBytes&&bytes>maximumBytes))
        throw std::invalid_argument("Outline projection storage exceeds the device budget");
    OutlineProjectionData result;
    result.header={PackedOutlineProjectionRecord::kLayoutVersion,static_cast<std::uint32_t>(materialCount),
        static_cast<std::uint32_t>(4+16*materialCount),static_cast<std::uint32_t>(coordinateCount)};
    result.records.reserve(static_cast<std::size_t>(materialCount));result.masks.reserve(static_cast<std::size_t>(materialCount));
    result.texcoord1.reserve(static_cast<std::size_t>(coordinateCount));
    for(std::size_t index=0;index<assets.size();++index) {
        auto& asset=*assets[index];
        const auto base=static_cast<std::uint32_t>(result.header[2]+result.texcoord1.size()*2);
        if(usesCoordinates[index]) {
            if(asset.texcoord1.size()!=asset.vertices.size())
                throw std::invalid_argument("Outline UV1 consumers require a complete vertex stream");
            result.texcoord1.insert(result.texcoord1.end(),asset.texcoord1.begin(),asset.texcoord1.end());
        }
        for(auto& material:asset.materials) {
            auto& shape=material.outlineSurface;PackedOutlineProjectionRecord record{};
            if(shape.present) {
                const bool tangent=shape.normalSource!=AssetMaterial::OutlineSurface::NormalSource::Geometry;
                record.parameters={shape.width,shape.minimumPixels,shape.depthOffset,1};
                record.indices={static_cast<std::uint32_t>(shape.normalSource),shape.depthPrepass?1U:0U,tangent?base:0U,
                    tangent?static_cast<std::uint32_t>(asset.vertices.size()):0U};
                record.uvScaleOffset=shape.uvScaleOffset;
                record.color={shape.color[0],shape.color[1],shape.color[2],1};
            }
            result.records.push_back(record);result.masks.push_back(&shape);
        }
    }
    return result;
}
} // namespace azurerender
