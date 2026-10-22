#pragma once
#include "assets/GltfLoader.hpp"
#include <optional>
#include <stdexcept>
namespace azurerender {
inline std::optional<AssetMaterialFrame> resolveMaterialFrame(const LoadedAsset& mesh){
    std::optional<AssetMaterialFrame> result;
    const auto accept=[&](const AssetMaterialFrame& frame){
        if(!frame.present)return;
        if(frame.node>=mesh.nodes.size())throw std::invalid_argument("Material frame node is outside the model");
        if(result&&(result->node!=frame.node||result->basis!=frame.basis))throw std::invalid_argument("Conflicting material frames in one mesh resource");
        result=frame;
    };
    for(const auto& material:mesh.materials){
        if(material.faceSdf.present&&material.faceSdf.schemaVersion==2)
            accept({true,material.faceSdf.headNode,material.faceSdf.headNodeName,material.faceSdf.headBasis});
        if(material.projectedHairRamp.present)accept(material.projectedHairRamp.frame);
    }
    return result;
}
}
