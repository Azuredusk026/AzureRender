#include "TextureImportSettings.hpp"
#include <set>
namespace azurerender {
TextureImportSettings TextureImportSettings::parse(const nlohmann::json& document) {
    return parse(document,TextureImportSettings{});
}
TextureImportSettings TextureImportSettings::parse(const nlohmann::json& document,const TextureImportSettings& defaults) {
    if(!document.is_object())throw std::invalid_argument("Texture settings require an object");
    const std::set<std::string> known{"schemaVersion","usage","colorSpace","filter","wrapU","wrapV","mipPolicy","anisotropy","mipBias","mipFilter"};
    for(const auto& field:document.items())if(!known.count(field.key()))throw std::invalid_argument("Unknown texture setting: "+field.key());
    if(document.contains("schemaVersion")&&(!document.at("schemaVersion").is_number_integer()||document.at("schemaVersion")!=1))throw std::invalid_argument("Unsupported texture settings version");
    TextureImportSettings result=defaults;result.usage=document.value("usage",result.usage);
    const bool color=result.usage=="albedo"||result.usage=="emissive";
    result.colorSpace=document.value("colorSpace",document.contains("usage")?(color?std::string("srgb"):std::string("linear")):defaults.colorSpace);
    result.filter=document.value("filter",result.filter);result.wrapU=document.value("wrapU",result.wrapU);result.wrapV=document.value("wrapV",result.wrapV);
    result.mipPolicy=document.value("mipPolicy",result.mipPolicy);result.anisotropy=document.value("anisotropy",result.anisotropy);
    if(document.contains("mipBias")&&!document.at("mipBias").is_number())throw std::invalid_argument("Texture mip bias requires a number");
    result.mipBias=document.value("mipBias",result.mipBias);
    if(document.contains("mipFilter")) {
        const auto& value=document.at("mipFilter");
        if(!value.is_string()||(value!="linear"&&value!="nearest"))throw std::invalid_argument("Unsupported texture mip filter");
        result.mipFilter=value=="linear"?MipFilter::Linear:MipFilter::Nearest;
    }
    if(!std::set<std::string>{"albedo","emissive","normal","mask","roughness","linear","packed-normal"}.count(result.usage))throw std::invalid_argument("Unsupported texture usage");
    if(result.colorSpace!="srgb"&&result.colorSpace!="linear")throw std::invalid_argument("Unsupported texture color space");
    if(!color&&result.colorSpace!="linear")throw std::invalid_argument("Data textures require linear color space");
    if(result.filter!="linear"&&result.filter!="nearest")throw std::invalid_argument("Unsupported texture filter");
    for(const auto& wrap:{result.wrapU,result.wrapV})if(wrap!="repeat"&&wrap!="clamp"&&wrap!="mirror")throw std::invalid_argument("Unsupported texture wrap");
    if(result.mipPolicy!="generate"&&result.mipPolicy!="none")throw std::invalid_argument("Unsupported texture mip policy");
    if(!std::isfinite(result.anisotropy)||result.anisotropy<1||result.anisotropy>16)throw std::invalid_argument("Texture anisotropy must be between 1 and 16");
    if(!std::isfinite(result.mipBias)||result.mipBias< -16||result.mipBias>16)throw std::invalid_argument("Texture mip bias must be between -16 and 16");
    return result;
}
nlohmann::json TextureImportSettings::describe() const {return {{"schemaVersion",1},{"usage",usage},{"colorSpace",colorSpace},{"filter",filter},{"wrapU",wrapU},{"wrapV",wrapV},{"mipPolicy",mipPolicy},{"anisotropy",anisotropy},{"mipBias",mipBias},
    {"mipFilter",mipFilter==MipFilter::MatchFilter?filter:mipFilter==MipFilter::Linear?"linear":"nearest"}};}
TextureFilterSemantic TextureImportSettings::semantic() const {
    if(usage=="normal")return TextureFilterSemantic::Normal;
    if(usage=="packed-normal")return TextureFilterSemantic::PackedNormal;
    return colorSpace=="srgb"?TextureFilterSemantic::Srgb:TextureFilterSemantic::Linear;
}
rhi::SamplerDesc TextureImportSettings::sampler(unsigned levels,float deviceAnisotropy) const {
    if(!levels||!std::isfinite(deviceAnisotropy)||deviceAnisotropy<1)throw std::invalid_argument("Invalid texture device limits");
    const auto wrap=[](const std::string& value){return value=="clamp"?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:value=="mirror"?VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:VK_SAMPLER_ADDRESS_MODE_REPEAT;};
    rhi::SamplerDesc result;result.filter=filter=="nearest"?VK_FILTER_NEAREST:VK_FILTER_LINEAR;result.mipLodBias=mipBias;
    result.addressU=wrap(wrapU);result.addressV=wrap(wrapV);result.maxLod=static_cast<float>(levels-1);
    result.mipmapLinear=mipFilter==MipFilter::Linear||(mipFilter==MipFilter::MatchFilter&&filter=="linear");
    result.maxAnisotropy=filter=="nearest"?1.F:std::min(anisotropy,deviceAnisotropy);return result;
}
}
