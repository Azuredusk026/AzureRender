#pragma once
#include "runtime/AssetDatabase.hpp"
#include "render/ColorGradingLut.hpp"
#include "render/RenderSettings.hpp"
namespace azurerender {
// Resolves a validated authoring reference without replacing its serialized identity.
inline void resolveColorGradingLutSettings(ColorGradingLutSettings& binding,const AssetDatabase* assets,
    const std::filesystem::path& relativeDirectory={}){
    if(binding.asset.empty()){binding.resolvedAsset.clear();binding.resourceFingerprint.clear();return;}
    auto path=assets?assets->resolveReference(binding.asset):std::filesystem::u8path(binding.asset);
    if(!assets&&path.is_relative())path=relativeDirectory/path;
    path=std::filesystem::weakly_canonical(path);
    static_cast<void>(loadColorGradingLut(path));
    std::string fingerprint=path.generic_u8string();
    if(assets)for(const auto& record:assets->records())if(record.second.path==path)fingerprint+=":"+std::to_string(record.second.fingerprint);
    binding.resolvedAsset=std::move(path);binding.resourceFingerprint=std::move(fingerprint);
}
}
