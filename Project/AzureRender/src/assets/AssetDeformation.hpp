#pragma once
#include "assets/GltfLoader.hpp"
#include "render/RenderMath.hpp"
namespace azurerender {
// Renderer and picking consume the same per-instance deformation order.
inline internal::Vector3 deformedVertexPosition(const LoadedAsset& asset, std::size_t index,
    const AssetPose* pose, const std::array<float,2>& morph) {
    const auto& vertex=asset.vertices.at(index);
    internal::Vector3 position{};
    for(unsigned axis=0;axis<3;++axis)
        position[axis]=vertex.position[axis]+vertex.morph0[axis]*morph[0]+vertex.morph1[axis]*morph[1];
    if(!pose)return position;
    internal::Vector3 skinned{};
    for(unsigned joint=0;joint<4;++joint) {
        if(vertex.weights[joint]==0)continue;
        const auto value=internal::transformPosition(pose->jointMatrices.at(vertex.joints[joint]),position);
        for(unsigned axis=0;axis<3;++axis)skinned[axis]+=value[axis]*vertex.weights[joint];
    }
    return skinned;
}
}
