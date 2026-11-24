#include "animation/ExpressionPose.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <numeric>
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
    if ((value.schemaVersion != 1 && value.schemaVersion != 2) || value.id.empty() || value.skeletonFingerprint.empty())
        throw std::invalid_argument("Invalid expression identity or version");
    if (value.bones.size() > 4096 || value.morphs.size() > 4096 || value.humanoidRotations.size() > 4096)
        throw std::invalid_argument("Expression target budget exceeded");
    if (value.schemaVersion == 1 && (!value.humanoidRotations.empty() ||
            std::any_of(value.morphs.begin(), value.morphs.end(), [](const auto& morph) { return morph.additive; })))
        throw std::invalid_argument("Additive expression fields require version two");
    std::set<std::string> bones,morphs;
    for(const auto& b:value.bones){finite(b.translation);finite(b.rotation);float norm=0;for(auto x:b.rotation)norm+=x*x;
        if(b.path.empty()||!bones.insert(b.path).second||norm<1e-12F)throw std::invalid_argument("Invalid expression bone delta");}
    for(const auto& m:value.morphs)if(m.nodePath.empty()||m.targetId.empty()||!std::isfinite(m.weight)||!morphs.insert(m.nodePath+"\n"+m.targetId).second)throw std::invalid_argument("Invalid expression morph target");
    std::set<std::string> humanoid;
    for (const auto& joint : value.humanoidRotations) {
        finite(joint.zero);
        finite(joint.offset);
        const auto norm = std::inner_product(joint.zero.begin(), joint.zero.end(), joint.zero.begin(), 0.F);
        if (joint.path.empty() || !humanoid.insert(joint.path).second || std::abs(norm - 1.F) > .001F)
            throw std::invalid_argument("Invalid humanoid rotation binding");
        for (std::size_t axis = 0; axis < joint.axes.size(); ++axis) {
            finite(joint.axes[axis]);
            finite(joint.rates[axis]);
            for (std::size_t other = 0; other <= axis; ++other) {
                const auto dot = std::inner_product(joint.axes[axis].begin(), joint.axes[axis].end(), joint.axes[other].begin(), 0.F);
                if (std::abs(dot - (other == axis ? 1.F : 0.F)) > .001F)
                    throw std::invalid_argument("Humanoid rotation axes must be orthonormal");
            }
            if (joint.rates[axis][0] <= 0.F || joint.rates[axis][1] <= 0.F)
                throw std::invalid_argument("Humanoid rotation rates must be positive");
        }
    }
}
std::array<float,4> multiply(const std::array<float,4>& a,const std::array<float,4>& b){return {
    a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};}

using Rotation = std::array<float,4>;

Rotation conjugate(Rotation q) {
    return {-q[0], -q[1], -q[2], q[3]};
}

Rotation normalized(Rotation q) {
    const auto length = std::sqrt(std::inner_product(q.begin(), q.end(), q.begin(), 0.F));
    if (length <= 1e-7F || !std::isfinite(length))
        throw std::invalid_argument("Singular humanoid rotation");
    for (auto& value : q) value /= length;
    return q;
}

float project(const Rotation& q, const std::array<float,3>& axis) {
    return std::inner_product(axis.begin(), axis.end(), q.begin(), 0.F);
}

Rotation applyHumanoidOffset(const Rotation& current, const ExpressionHumanoidRotation& joint, float weight) {
    const auto relative = multiply(conjugate(joint.zero), current);
    const auto& twistAxis = joint.axes[2];
    const auto axial = project(relative, twistAxis);
    auto twist = normalized({axial*twistAxis[0], axial*twistAxis[1], axial*twistAxis[2], relative[3]});
    const auto swing = multiply(relative, conjugate(twist));
    if (twist[3] < 0.F)
        for (auto& value : twist) value = -value;

    std::array<float,3> angles{
        2.F*std::atan2(project(swing, joint.axes[0]), swing[3]),
        2.F*std::atan2(project(swing, joint.axes[1]), swing[3]),
        2.F*std::atan2(project(twist, twistAxis), twist[3])
    };
    for (std::size_t axis = 0; axis < angles.size(); ++axis) {
        const auto& rates = joint.rates[axis];
        const auto muscle = angles[axis] / rates[angles[axis] >= 0.F ? 1 : 0] + joint.offset[axis] * weight;
        angles[axis] = muscle * rates[muscle >= 0.F ? 1 : 0];
        if (!std::isfinite(angles[axis]) || std::abs(angles[axis]) >= 3.14159265F)
            throw std::invalid_argument("Humanoid rotation exceeds the local angular range");
    }

    // The swing stores the sum of the two orthogonal half-angle tangents.
    const auto first = std::tan(.5F * angles[0]);
    const auto second = std::tan(.5F * angles[1]);
    const auto rotatedSwing = normalized({first*joint.axes[0][0] + second*joint.axes[1][0],
        first*joint.axes[0][1] + second*joint.axes[1][1], first*joint.axes[0][2] + second*joint.axes[1][2], 1.F});
    const auto halfTwist = .5F * angles[2];
    const auto sine = std::sin(halfTwist);
    const Rotation rotatedTwist{sine*twistAxis[0], sine*twistAxis[1], sine*twistAxis[2], std::cos(halfTwist)};
    return normalized(multiply(joint.zero, multiply(rotatedSwing, rotatedTwist)));
}

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
    for (const auto& m : json.value("morphs", nlohmann::json::array()))
        result.morphs.push_back({m.at("nodePath"), m.at("targetId"), m.at("weight"), m.value("additive", false)});
    for (const auto& joint : json.value("humanoidRotations", nlohmann::json::array()))
        result.humanoidRotations.push_back({joint.at("path"), joint.at("zero"), joint.at("axes"), joint.at("rates"), joint.at("offset")});
    validate(result);return result;
}
nlohmann::json expressionPoseToJson(const ExpressionPose& value){validate(value);nlohmann::json result={{"schemaVersion",value.schemaVersion},{"id",value.id},{"skeletonFingerprint",value.skeletonFingerprint},{"bones",nlohmann::json::array()},{"morphs",nlohmann::json::array()}};
    for(const auto& b:value.bones)result["bones"].push_back({{"path",b.path},{"translation",b.translation},{"rotation",b.rotation}});
    for (const auto& m : value.morphs) {
        nlohmann::json record{{"nodePath", m.nodePath}, {"targetId", m.targetId}, {"weight", m.weight}};
        if (m.additive) record["additive"] = true;
        result["morphs"].push_back(std::move(record));
    }
    if (!value.humanoidRotations.empty()) {
        result["humanoidRotations"] = nlohmann::json::array();
        for (const auto& joint : value.humanoidRotations)
            result["humanoidRotations"].push_back({{"path", joint.path}, {"zero", joint.zero}, {"axes", joint.axes},
                                                  {"rates", joint.rates}, {"offset", joint.offset}});
    }
    return result;
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
    for (const auto& joint : preset.humanoidRotations) {
        const auto found = lookup.find(joint.path);
        if (found == lookup.end()) throw std::invalid_argument("Missing humanoid rotation path: " + joint.path);
        const auto index = found->second;
        if (asset.nodes[index].usesMatrix) throw std::invalid_argument("Humanoid rotation requires a decomposed TRS target");
        if (weight > 0.F) result.local[index].rotation = applyHumanoidOffset(result.local[index].rotation, joint, weight);
    }
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
                if(matched)continue;auto& weights=result.morphWeights[found->second];weights.resize(range.targets.size(),0);weights[i]+=(m.additive ? m.weight : m.weight-weights[i])*weight;matched=true;
            }
        }if(!matched)throw std::invalid_argument("Missing expression morph target: "+m.targetId);
    }
    resolveAssetPose(asset,result);return result;
}
