#pragma once
#include "assets/GltfLoader.hpp"
#include <string>
#include <stdexcept>
namespace azurerender {
// Plain PBR with optional emission/neutral bits shares one bounded fast path.
inline bool ordinaryOpaqueMaterial(const AssetMaterial& material) noexcept {
    return material.materialClass==AssetMaterialClass::Generic&&material.alphaMode==AssetAlphaMode::Opaque
        &&(material.materialFeatures&~std::uint32_t{MaterialFeatureNeutralFallback|MaterialFeatureEmissiveMask})==0;
}
inline std::string sceneFragmentVariant(std::string file,bool multisampled,bool clipmap,bool field=false){
    auto at=file.rfind(".frag.spv");
    if(at==std::string::npos||at+9!=file.size())throw std::invalid_argument("Expected a fragment SPIR-V program");
    if(multisampled){file.insert(at,"_samples");at+=8;}
    if(clipmap){file.insert(at,"_clipmap");at+=8;}
    if(field)file.insert(at,"_field");
    return file;
}
}
