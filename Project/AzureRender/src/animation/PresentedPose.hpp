#pragma once
#include "assets/GltfLoader.hpp"
#include "render/RenderContext.hpp"
namespace azurerender {
inline AssetPose samplePresentedAnimationPose(const LoadedAsset& asset,const NodeAnimationFrame& frame){
    if(!frame.samples.empty())return sampleAnimationBlend(asset,frame.samples);
    if(asset.animations.empty())return bindAnimationPose(asset);
    auto result=sampleAnimationPose(asset,frame.clip,static_cast<float>(frame.time),frame.loop);
    if(frame.blend<1)result=blendAnimationPoses(asset,
        sampleAnimationPose(asset,frame.previousClip,static_cast<float>(frame.previousTime),frame.previousLoop),
        result,frame.blend);
    return result;
}
}
