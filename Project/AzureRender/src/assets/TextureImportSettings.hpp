#pragma once
#include "assets/TextureMipChain.hpp"
#include "rhi/Rhi.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
struct TextureImportSettings {
    enum class MipFilter { MatchFilter,Nearest,Linear };
    enum class MagFilter { MatchFilter,Nearest,Linear };
    std::string usage="albedo",colorSpace="srgb",filter="linear",wrapU="repeat",wrapV="repeat",mipPolicy="generate";
    float anisotropy=8,mipBias=0;
    MipFilter mipFilter=MipFilter::MatchFilter;
    MagFilter magFilter=MagFilter::MatchFilter;
    static TextureImportSettings parse(const nlohmann::json& document);
    static TextureImportSettings parse(const nlohmann::json& document,const TextureImportSettings& defaults);
    nlohmann::json describe() const;
    TextureFilterSemantic semantic() const;
    rhi::SamplerDesc sampler(unsigned levels,float deviceAnisotropy) const;
};
}
