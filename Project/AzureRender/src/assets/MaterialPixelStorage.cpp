#include "assets/MaterialPixelStorage.hpp"
#include <stdexcept>

namespace azurerender {
namespace {
template<class Material> auto& pixels(Material& m,AssetMaterial::PixelKind kind) {
    using Kind=AssetMaterial::PixelKind;
    switch(kind) {
    case Kind::BaseColor:return m.baseColorPixels;
    case Kind::Normal:return m.normalPixels;
    case Kind::MetallicRoughness:return m.metallicRoughnessPixels;
    case Kind::SpecularEmissive:return m.specularEmissivePixels;
    case Kind::StyleMask:return m.styleMaskPixels;
    case Kind::Matcap:return m.matcapPixels;
    case Kind::HairData:return m.hairDataPixels;
    case Kind::FaceSdf:return m.faceSdf.pixels;
    case Kind::ShadowColorLut:return m.shadowColorLutPixels;
    case Kind::ProjectedParameters:return m.projectedHairRamp.parameterPixels;
    case Kind::ProjectedLine:return m.projectedHairRamp.line.pixels;
    case Kind::OutlineMask:return m.outlineSurface.maskPixels;
    case Kind::Transmission:return m.transmissionPixels;
    default:throw std::invalid_argument("Unknown material pixel field");
    }
}
}
std::vector<std::uint8_t>& materialPixelBytes(AssetMaterial& m,AssetMaterial::PixelKind kind){return pixels(m,kind);}
const std::vector<std::uint8_t>& materialPixelBytes(const AssetMaterial& m,AssetMaterial::PixelKind kind){return pixels(m,kind);}
std::uint64_t materialPixelLogicalBytes(const AssetMaterial& m,AssetMaterial::PixelKind kind) {
    const auto& bytes=pixels(m,kind);const auto& prepared=m.preparedPixels.at(static_cast<std::size_t>(kind));
    return !bytes.empty()?bytes.size():(prepared?prepared->size():0);
}
void prepareMaterialPixels(AssetMaterial& material,ImmutableBytePool& pool,std::size_t minimumBytes) {
    for(std::size_t i=0;i<material.preparedPixels.size();++i) {
        auto& bytes=pixels(material,static_cast<AssetMaterial::PixelKind>(i));auto& prepared=material.preparedPixels[i];
        if(!bytes.empty()) {
            if(bytes.size()<minimumBytes){prepared.reset();continue;}
            auto payload=pool.intern(bytes);prepared=std::move(payload);
            std::vector<std::uint8_t>{}.swap(bytes);
        }else if(prepared)prepared=pool.adopt(prepared);
    }
}
void restoreMaterialPixel(AssetMaterial& material,AssetMaterial::PixelKind kind) {
    auto& bytes=pixels(material,kind);auto& prepared=material.preparedPixels.at(static_cast<std::size_t>(kind));
    if(prepared&&bytes.empty()){auto decoded=prepared->read();bytes.swap(decoded);}
    prepared.reset();
}
void restoreMaterialPixels(AssetMaterial& material) {
    for(std::size_t i=0;i<material.preparedPixels.size();++i)restoreMaterialPixel(material,static_cast<AssetMaterial::PixelKind>(i));
}
}
