#pragma once
#include "assets/GltfLoader.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace azurerender {
// Layout v1: a three-word table per vertex (delta base, relative weight base,
// target count), followed by nine float words per target: POSITION/NORMAL/TANGENT.
// Offset zero denotes a legacy mesh; data and weights have independent ownership.
struct MorphGpuMesh {std::uint32_t tableBase=0,weightCount=0;};
inline constexpr std::size_t kMorphGpuBudgetBytes=128ULL*1024*1024;
inline MorphGpuMesh appendMorphGpuMesh(std::vector<std::uint32_t>& words,const LoadedAsset& asset){
 if(asset.morphRanges.empty())return {};
 if(words.size()<3)throw std::invalid_argument("Morph GPU sentinel is missing");
 std::uint64_t count=static_cast<std::uint64_t>(asset.vertices.size())*3;
 for(const auto& range:asset.morphRanges){if(range.targets.size()>kMorphTargetBudget||std::uint64_t(range.firstVertex)+range.vertexCount>asset.vertices.size())throw std::invalid_argument("Morph GPU range exceeds budget or geometry");count+=std::uint64_t(range.vertexCount)*range.targets.size()*9;}
 if((std::uint64_t(words.size())+count)*4>kMorphGpuBudgetBytes)throw std::invalid_argument("Morph GPU data exceeds 128 MiB storage budget");
 const auto before=words.size();MorphGpuMesh result{static_cast<std::uint32_t>(before),0};
 try{
  words.resize(words.size()+asset.vertices.size()*3,0);std::vector<bool> assigned(asset.vertices.size());
  for(const auto& range:asset.morphRanges){
   if(range.node>=asset.nodes.size())throw std::invalid_argument("Morph GPU node is missing");
   for(std::uint32_t vertex=0;vertex<range.vertexCount;++vertex){
    const auto index=range.firstVertex+vertex;if(assigned[index])throw std::invalid_argument("Overlapping morph GPU ranges");assigned[index]=true;
    const auto table=result.tableBase+index*3;words[table]=static_cast<std::uint32_t>(words.size());words[table+1]=result.weightCount;words[table+2]=static_cast<std::uint32_t>(range.targets.size());
    for(const auto& target:range.targets)for(const auto* values:{&target.positions,&target.normals,&target.tangents}){
     if(!values->empty()&&values->size()!=range.vertexCount)throw std::invalid_argument("Morph GPU delta width differs from range");
     for(unsigned axis=0;axis<3;++axis){const float value=values->empty()?0:values->at(vertex)[axis];if(!std::isfinite(value))throw std::invalid_argument("Nonfinite morph GPU delta");std::uint32_t word;std::memcpy(&word,&value,4);words.push_back(word);}
    }
   }
   result.weightCount+=static_cast<std::uint32_t>(range.targets.size());
  }
 }catch(...){words.resize(before);throw;}
 return result;
}
inline std::vector<float> flattenMorphGpuWeights(const LoadedAsset& asset,const AssetPose& pose){
 std::vector<float> result;
 for(const auto& range:asset.morphRanges){if(range.node>=pose.morphWeights.size()||pose.morphWeights[range.node].size()!=range.targets.size())throw std::invalid_argument("Morph GPU pose width differs from targets");
  for(const float value:pose.morphWeights[range.node]){if(!std::isfinite(value))throw std::invalid_argument("Nonfinite morph GPU weight");result.push_back(value);}
 }
 return result;
}
}
