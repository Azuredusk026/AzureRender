#pragma once
#include "assets/GltfLoader.hpp"
#include "render/RenderMath.hpp"
namespace azurerender {
inline void applyMorphOverrides(const LoadedAsset& asset, AssetPose& pose,const std::vector<float>& weights) {
    if(weights.empty())return;
    if(weights.size()>kMorphTargetBudget)throw std::invalid_argument("Morph override budget exceeded");
    for(float w:weights)if(!std::isfinite(w))throw std::invalid_argument("Nonfinite morph override");
    for(const auto& range:asset.morphRanges)
        if(range.targets.size()!=weights.size()||range.node>=pose.morphWeights.size())
            throw std::invalid_argument("Morph override shape mismatch");
    if(asset.morphRanges.empty())throw std::invalid_argument("Morph overrides require targets");
    for(const auto& range:asset.morphRanges)pose.morphWeights[range.node]=weights;
}
inline internal::Vector3 morphedVertexAttribute(const LoadedAsset& asset,std::size_t index,
    const AssetPose* pose,const std::array<float,2>& legacy,unsigned attribute) {
    const auto& v=asset.vertices.at(index);
    internal::Vector3 value=attribute==0?v.position:attribute==1?v.normal:internal::Vector3{v.tangent[0],v.tangent[1],v.tangent[2]};
    if(asset.morphRanges.empty()) {
        if(attribute==0)for(unsigned a=0;a<3;++a)value[a]+=v.morph0[a]*legacy[0]+v.morph1[a]*legacy[1];
        return value;
    }
    for(const auto& range:asset.morphRanges) {
        if(index<range.firstVertex||index-range.firstVertex>=range.vertexCount)continue;
        const auto& states=pose?pose->morphWeights:asset.defaultMorphWeights;
        const auto& weights=states.at(range.node);
        if(weights.size()!=range.targets.size())throw std::invalid_argument("Morph pose shape mismatch");
        for(std::size_t t=0;t<weights.size();++t){
            const auto& target=range.targets[t];
            const auto& delta=attribute==0?target.positions:attribute==1?target.normals:target.tangents;
            if(!delta.empty())for(unsigned a=0;a<3;++a)value[a]+=delta.at(index-range.firstVertex)[a]*weights[t];
        }
        break;
    }
    return value;
}
// Morphs are evaluated in mesh space before the instance joint pose.
inline internal::Vector3 deformedVertexPosition(const LoadedAsset& asset,std::size_t index,
    const AssetPose* pose,const std::array<float,2>& morph) {
    const auto position=morphedVertexAttribute(asset,index,pose,morph,0);
    if(!pose||pose->jointMatrices.empty())return position;
    const auto& vertex=asset.vertices.at(index);internal::Vector3 skinned{};
    for(unsigned joint=0;joint<4;++joint) {
        if(vertex.weights[joint]==0)continue;
        const auto value=internal::transformPosition(pose->jointMatrices.at(vertex.joints[joint]),position);
        for(unsigned axis=0;axis<3;++axis)skinned[axis]+=value[axis]*vertex.weights[joint];
    }
    return skinned;
}
inline internal::Vector3 deformedVertexDirection(const LoadedAsset& asset,std::size_t index,
    const AssetPose* pose,unsigned attribute) {
    auto value=morphedVertexAttribute(asset,index,pose,{0,0},attribute);
    if(pose&&asset.hasSkin) {
        internal::Vector3 sum{};const auto& vertex=asset.vertices.at(index);
        for(unsigned j=0;j<4;++j)if(vertex.weights[j]!=0){
            const auto& m=pose->jointMatrices.at(vertex.joints[j]);
            for(unsigned a=0;a<3;++a)sum[a]+=(m[a]*value[0]+m[4+a]*value[1]+m[8+a]*value[2])*vertex.weights[j];
        }
        value=sum;
    }
    const float length=std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
    if(length>1e-12F)for(auto& a:value)a/=length;
    return value;
}
}
