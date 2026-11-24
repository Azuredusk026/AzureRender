#include "animation/ExpressionPose.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace {
std::vector<std::string> paths(const LoadedAsset& asset){
    std::vector<std::string> result(asset.nodes.size());std::vector<int> state(asset.nodes.size());std::set<std::string> unique;
    std::function<void(std::size_t)> visit=[&](std::size_t i){
        if(state.at(i)==2)return;if(state[i]==1)throw std::invalid_argument("Cyclic expression skeleton");state[i]=1;
        const auto& node=asset.nodes.at(i);const auto name=node.name.empty()?"@node:"+std::to_string(i):node.name;
        if(node.parent>=0){visit(static_cast<std::size_t>(node.parent));result[i]=result.at(node.parent)+"/";}
        result[i]+=name;if(!unique.insert(result[i]).second)throw std::invalid_argument("Ambiguous expression node path");state[i]=2;
    };for(std::size_t i=0;i<result.size();++i)visit(i);return result;
}
template<std::size_t N> void finite(const std::array<float,N>& value){for(float v:value)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite expression value");}
void validate(const ExpressionPose& value){
    if(value.schemaVersion!=1||value.id.empty()||value.skeletonFingerprint.empty())throw std::invalid_argument("Invalid expression identity or version");
    if(value.bones.size()>4096||value.morphs.size()>4096)throw std::invalid_argument("Expression target budget exceeded");
    std::set<std::string> bones,morphs;
    for(const auto& b:value.bones){finite(b.translation);finite(b.rotation);float norm=0;for(auto x:b.rotation)norm+=x*x;
        if(b.path.empty()||!bones.insert(b.path).second||norm<1e-12F)throw std::invalid_argument("Invalid expression bone delta");}
    for(const auto& m:value.morphs)if(m.nodePath.empty()||m.targetId.empty()||!std::isfinite(m.weight)||!morphs.insert(m.nodePath+"\n"+m.targetId).second)throw std::invalid_argument("Invalid expression morph target");
}
std::array<float,4> multiply(const std::array<float,4>& a,const std::array<float,4>& b){return {
    a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};}
}
std::string expressionSkeletonFingerprint(const LoadedAsset& asset){
    const auto names=paths(asset);nlohmann::json records=nlohmann::json::array();
    for(std::size_t i=0;i<asset.nodes.size();++i){const auto& n=asset.nodes[i];records.push_back({names[i],n.translation,n.rotation,n.scale,n.usesMatrix,n.localMatrix});}
    const std::string serialized=nlohmann::json({records,asset.jointNodes,asset.inverseBindMatrices}).dump();
    std::uint64_t hash=14695981039346656037ULL;for(unsigned char c:serialized){hash^=c;hash*=1099511628211ULL;}
    std::ostringstream out;out<<"expression-v1-fnv64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();
}
ExpressionPose expressionPoseFromJson(const nlohmann::json& json){
    ExpressionPose result;result.schemaVersion=json.at("schemaVersion").get<std::uint32_t>();result.id=json.at("id");result.skeletonFingerprint=json.at("skeletonFingerprint");
    for(const auto& b:json.value("bones",nlohmann::json::array()))result.bones.push_back({b.at("path"),b.value("translation",std::array<float,3>{}),b.value("rotation",std::array<float,4>{0,0,0,1})});
    for(const auto& m:json.value("morphs",nlohmann::json::array()))result.morphs.push_back({m.at("nodePath"),m.at("targetId"),m.at("weight")});
    validate(result);return result;
}
nlohmann::json expressionPoseToJson(const ExpressionPose& value){validate(value);nlohmann::json result={{"schemaVersion",1},{"id",value.id},{"skeletonFingerprint",value.skeletonFingerprint},{"bones",nlohmann::json::array()},{"morphs",nlohmann::json::array()}};
    for(const auto& b:value.bones)result["bones"].push_back({{"path",b.path},{"translation",b.translation},{"rotation",b.rotation}});
    for(const auto& m:value.morphs)result["morphs"].push_back({{"nodePath",m.nodePath},{"targetId",m.targetId},{"weight",m.weight}});return result;
}
std::vector<float> morphOverridesFromJson(const std::string& text){
    if(text.empty())return {};const auto json=nlohmann::json::parse(text);if(json.at("schemaVersion")!=1)throw std::invalid_argument("Unknown morph override version");
    auto values=json.at("weights").get<std::vector<float>>();if(values.size()>kMorphTargetBudget)throw std::invalid_argument("Morph override budget exceeded");
    for(float v:values)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite morph override");return values;
}
AssetPose applyExpressionPose(const LoadedAsset& asset,const AssetPose& base,const ExpressionPose& preset,float weight){
    validate(preset);if(!std::isfinite(weight)||weight<0||weight>1)throw std::invalid_argument("Expression blend must be in [0,1]");
    if(preset.skeletonFingerprint!=expressionSkeletonFingerprint(asset))throw std::invalid_argument("Expression skeleton fingerprint mismatch");
    if(base.local.size()!=asset.nodes.size())throw std::invalid_argument("Expression base pose size mismatch");
    auto result=base;const auto names=paths(asset);std::map<std::string,std::size_t> lookup;for(std::size_t i=0;i<names.size();++i)lookup[names[i]]=i;
    for(const auto& b:preset.bones){const auto found=lookup.find(b.path);if(found==lookup.end())throw std::invalid_argument("Missing expression bone: "+b.path);
        const auto index=found->second;if(asset.nodes[index].usesMatrix)throw std::invalid_argument("Expression requires decomposed TRS target");auto& local=result.local[index];
        for(std::size_t i=0;i<3;++i)local.translation[i]+=b.translation[i]*weight;
        auto delta=b.rotation;float norm=0;for(auto v:delta)norm+=v*v;for(auto& v:delta)v/=std::sqrt(norm);if(delta[3]<0)for(auto& v:delta)v=-v;
        const float angle=std::acos(std::clamp(delta[3],-1.F,1.F));const float sine=std::sin(angle);
        if(sine>1e-6F){for(std::size_t i=0;i<3;++i)delta[i]*=std::sin(angle*weight)/sine;delta[3]=std::cos(angle*weight);}else delta={0,0,0,1};
        local.rotation=multiply(local.rotation,delta);
    }
    if(result.morphWeights.size()!=asset.nodes.size())result.morphWeights=asset.defaultMorphWeights;
    result.morphWeights.resize(asset.nodes.size());
    for(const auto& m:preset.morphs){const auto found=lookup.find(m.nodePath);if(found==lookup.end())throw std::invalid_argument("Missing expression morph node");bool matched=false;
        for(const auto& range:asset.morphRanges)if(range.node==found->second){
            for(std::size_t i=0;i<range.targets.size();++i)if(range.targets[i].name==m.targetId){
                if(matched)continue;auto& weights=result.morphWeights[found->second];weights.resize(range.targets.size(),0);weights[i]+=(m.weight-weights[i])*weight;matched=true;
            }
        }if(!matched)throw std::invalid_argument("Missing expression morph target: "+m.targetId);
    }
    resolveAssetPose(asset,result);return result;
}
