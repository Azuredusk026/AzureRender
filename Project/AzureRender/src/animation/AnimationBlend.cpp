#include "assets/GltfLoader.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

AssetPose sampleAnimationBlend(const LoadedAsset& asset,
        const std::vector<azurerender::AnimationClipSample>& samples){
    if(samples.empty()||samples.size()>32)throw std::invalid_argument("Animation blend needs one to 32 samples");
    double total=0;
    for(const auto& sample:samples){
        if(sample.clip>=asset.animations.size()||!std::isfinite(sample.time)||sample.time<0
            ||!std::isfinite(sample.weight)||sample.weight<0||!std::isfinite(sample.duration)||sample.duration<0)
            throw std::invalid_argument("Invalid animation blend sample");
        const auto& clip=asset.animations[sample.clip];
        const double actual=clip.endTime-clip.startTime;
        if(sample.duration>0&&std::abs(actual-sample.duration)>1e-4*std::max(1.,sample.duration))
            throw std::invalid_argument("Animation blend duration does not match bound clip");
        total+=sample.weight;
    }
    if(std::abs(total-1)>1e-5)throw std::invalid_argument("Animation blend weights must sum to one");
    auto result=bindAnimationPose(asset);std::vector<std::array<float,4>> reference;
    for(auto& node:result.local){node.translation={};node.scale={};node.rotation={};}
    for(auto& weights:result.morphWeights)std::fill(weights.begin(),weights.end(),0.F);
    for(const auto& sample:samples){
        if(sample.weight==0)continue;
        const auto pose=sampleAnimationPose(asset,sample.clip,static_cast<float>(sample.time),sample.loop);
        if(reference.empty())for(const auto& node:pose.local)reference.push_back(node.rotation);
        const float weight=static_cast<float>(sample.weight/total);
        for(std::size_t i=0;i<pose.local.size();++i){
            auto& dest=result.local[i];const auto& source=pose.local[i];float dot=0;
            for(unsigned axis=0;axis<4;++axis)dot+=reference[i][axis]*source.rotation[axis];
            for(unsigned axis=0;axis<3;++axis){
                dest.translation[axis]+=weight*source.translation[axis];
                dest.scale[axis]+=weight*source.scale[axis];
            }
            for(unsigned axis=0;axis<4;++axis)dest.rotation[axis]+=weight*(dot<0?-1.F:1.F)*source.rotation[axis];
        }
        for(std::size_t i=0;i<pose.morphWeights.size();++i)
            for(std::size_t j=0;j<pose.morphWeights[i].size();++j)
                result.morphWeights[i][j]+=weight*pose.morphWeights[i][j];
    }
    for(std::size_t i=0;i<result.local.size();++i){
        auto& q=result.local[i].rotation;double norm=0;for(float v:q)norm+=v*v;
        if(norm<1e-16)q=reference[i];
        else for(float& v:q)v=static_cast<float>(v/std::sqrt(norm));
    }
    resolveAssetPose(asset,result);return result;
}
