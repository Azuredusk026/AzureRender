#include "AnimationPreviewService.hpp"
#include <fstream>
#include <cmath>
namespace azurerender {
void AnimationPreviewService::synchronize(EditorContext& context){
    const auto assetVersion=context.isProject()?context.assets().version():0;
    if(revision_==context.revision()&&assetVersion_==assetVersion)return;
    const auto fingerprint=context.documentVersion().contentHash;
    if(!fingerprint_.empty()&&(fingerprint_!=fingerprint||assetVersion_!=assetVersion))tracks_.clear();
    revision_=context.revision();assetVersion_=assetVersion;fingerprint_=fingerprint;
}
void AnimationPreviewService::seek(EditorContext& context,const std::string& node,const std::string& requested,double time,const std::string& previous,double crossfade){
    if(!std::isfinite(time)||time<0||time>600)throw std::invalid_argument("Timeline time must be between 0 and 600 seconds");
    if(!context.isProject())throw std::invalid_argument("Animation timeline requires a project");
    const auto data=context.componentData(node,"azure.animator");if(data.is_null())throw std::invalid_argument("Timeline requires an animator node");
    const auto state=requested.empty()?data.at("state").get<std::string>():requested;
    static_cast<void>(context.animationPose(node,state,time,previous,crossfade));
    std::ifstream input(context.assets().resolveReference(data.at("asset")));nlohmann::json graph;input>>graph;
    auto machine=AnimationStateMachine::parse(graph);if(!previous.empty())machine.select(previous);machine.select(state,previous.empty()?0:crossfade);machine.advance(time);
    synchronize(context);const auto found=tracks_.find(node);if(found==tracks_.end()&&tracks_.size()>=32)throw std::invalid_argument("Animation timeline capacity is 32 objects");
    const auto sceneNode=std::find_if(context.scene().nodes.begin(),context.scene().nodes.end(),[&](const auto& value){return value.id==node;});
    const auto resource=std::find_if(context.scene().resources.begin(),context.scene().resources.end(),[&](const auto& value){return value.id==sceneNode->resourceId;});
    if(resource==context.scene().resources.end())throw std::invalid_argument("Animation timeline requires a model resource");
    auto skeleton=found==tracks_.end()?std::make_shared<LoadedAsset>(loadGltfAsset(resource->path.u8string())):found->second.skeleton;
    if(machine.clip()>=skeleton->animations.size()||machine.previousClip()>=skeleton->animations.size())throw std::invalid_argument("Animation clips do not match the bound model");
    if(skeleton->nodes.size()>4096||skeleton->jointNodes.size()>1024)throw std::invalid_argument("Skeleton exceeds preview node budget");
    std::vector<AssetVertex>().swap(skeleton->vertices);std::vector<std::uint32_t>().swap(skeleton->indices);std::vector<AssetMaterial>().swap(skeleton->materials);std::vector<AssetPrimitive>().swap(skeleton->primitives);
    Track track{std::move(machine),{data.value("morph0",0.F),data.value("morph1",0.F)},found!=tracks_.end()&&found->second.playing,crossfade,previous,found==tracks_.end()?1:found->second.speed,found==tracks_.end()?true:found->second.loop,std::move(skeleton)};
    track.morphOverrides=morphOverridesFromJson(data.value("morphOverrides",std::string()));
    const auto expression=data.value("expression",std::string());
    if(!expression.empty())track.expression=std::make_shared<ExpressionPose>(expressionPoseFromJson(nlohmann::json::parse(expression)));
    track.expressionWeight=data.value("expressionWeight",1.F);
    if(track.expression)static_cast<void>(applyExpressionPose(*track.skeleton,bindAnimationPose(*track.skeleton),*track.expression,track.expressionWeight));
    if(found==tracks_.end())track.loop=track.machine.loop();tracks_[node]=std::move(track);
}
void AnimationPreviewService::play(EditorContext& context,const std::string& node){synchronize(context);if(!tracks_.count(node))seek(context,node,{},0);tracks_.at(node).playing=true;}
void AnimationPreviewService::pause(const std::string& node){const auto found=tracks_.find(node);if(found==tracks_.end())throw std::invalid_argument("No animation timeline for node");found->second.playing=false;}
void AnimationPreviewService::clear(const std::string& node){if(node.empty())tracks_.clear();else tracks_.erase(node);}
void AnimationPreviewService::advance(EditorContext& context,double delta){
    if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Preview delta must be finite and nonnegative");synchronize(context);
    for(auto& [node,track]:tracks_)if(track.playing){
        auto step=delta;if(!track.loop){const auto duration=track.skeleton->animations.at(track.machine.clip()).endTime;step=std::min(step,std::max(0.,(duration-track.machine.time())/track.speed));}
        track.machine.advance(step,track.speed);if(!track.loop&&track.machine.time()>=track.skeleton->animations.at(track.machine.clip()).endTime-1e-6)track.playing=false;
    }
}
void AnimationPreviewService::configure(EditorContext& context,const std::string& node,double speed,bool loop){
    if(!std::isfinite(speed)||speed<.05||speed>4)throw std::invalid_argument("Animation preview speed must be between 0.05 and 4");
    synchronize(context);if(!tracks_.count(node))seek(context,node,{},0);auto& track=tracks_.at(node);track.speed=speed;track.loop=loop;
}
std::vector<ViewportOverlayLine> AnimationPreviewService::skeletonLines(EditorContext& context){
    synchronize(context);const auto scene=context.scene().renderDescription();const auto world=scene::resolveNodeWorldTransforms(scene);std::vector<ViewportOverlayLine> lines;
    for(const auto& [id,track]:tracks_){const auto node=std::find_if(scene.nodes.begin(),scene.nodes.end(),[&](const auto& value){return value.id==id;});if(node==scene.nodes.end())continue;
        const auto& asset=*track.skeleton;auto pose=sampleAnimationPose(asset,track.machine.clip(),static_cast<float>(track.machine.time()),track.loop);
        if(track.machine.blend()<1)pose=blendAnimationPoses(asset,sampleAnimationPose(asset,track.machine.previousClip(),static_cast<float>(track.machine.previousTime()),track.loop),pose,track.machine.blend());
        if(track.expression)pose=applyExpressionPose(asset,pose,*track.expression,track.expressionWeight);
        const auto& transform=world.at(static_cast<std::size_t>(node-scene.nodes.begin()));
        for(const auto joint:asset.jointNodes){const auto parent=asset.nodes.at(joint).parent;if(parent<0)continue;
            const auto position=[&](std::size_t index){const auto& matrix=pose.nodeWorldMatrices.at(index);return internal::transformPosition(transform,{matrix[12],matrix[13],matrix[14]});};
            if(lines.size()>=4096)throw std::invalid_argument("Skeleton helper line budget exceeded");lines.push_back({position(joint),position(static_cast<std::size_t>(parent)),{1,.75F,.2F,1},id});
        }
    }return lines;
}
nlohmann::json AnimationPreviewService::describe() const{
    auto result=nlohmann::json::object();for(const auto& [node,track]:tracks_)result[node]={{"state",track.machine.state()},{"time",track.machine.time()},{"playing",track.playing},{"clip",track.machine.clip()},{"blend",track.machine.blend()},{"crossfade",track.crossfade},{"previous",track.previous},{"speed",track.speed},{"loop",track.loop},{"joints",track.skeleton->jointNodes.size()}};return result;
}
std::vector<NodeAnimationFrame> AnimationPreviewService::frames() const{
    std::vector<NodeAnimationFrame> result;for(const auto& [node,track]:tracks_){const auto& m=track.machine;result.push_back({node,m.clip(),m.time(),track.loop,m.previousClip(),m.previousTime(),m.blend(),track.loop,track.morph});auto& frame=result.back();frame.morphOverrides=track.morphOverrides;frame.expression=track.expression;frame.expressionWeight=track.expressionWeight;}return result;
}
}
