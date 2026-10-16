#include "animation/ExpressionPose.hpp"
#include "assets/AssetDeformation.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <functional>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    if((argc==5||argc==6)&&std::string(argv[1])=="--evaluate"){
        const auto asset=loadGltfAsset(argv[2]);std::ifstream input(argv[3]);nlohmann::json json;input>>json;
        const float time=argc==6?std::stof(argv[5]):0;
        const auto preset=expressionPoseFromJson(json);const auto pose=applyExpressionPose(asset,sampleAnimationPose(asset,0,time,false),preset,1);
        auto positions=nlohmann::json::array();for(std::size_t i=0;i<asset.vertices.size();++i)positions.push_back(azurerender::deformedVertexPosition(asset,i,&pose,{0,0}));
        auto origins=nlohmann::json::object();for(std::size_t i=0;i<asset.nodes.size();++i){const auto& m=pose.nodeWorldMatrices[i];origins[asset.nodes[i].name]={m[12],m[13],m[14]};}
        std::ofstream out(argv[4]);out<<nlohmann::json{{"positions",positions},{"boneOrigins",origins},{"sourceKind","authored"},{"preset",preset.id}}.dump();if(!out)throw std::runtime_error("Cannot write evaluated expression");return 0;
    }
    if(argc==4&&std::string(argv[1])=="--inspect"){
        const auto asset=loadGltfAsset(argv[2]);nlohmann::json report={{"skeletonFingerprint",expressionSkeletonFingerprint(asset)},{"nodes",nlohmann::json::array()},{"vertexCount",asset.vertices.size()}};
        std::function<std::string(std::size_t)> path=[&](std::size_t i){const auto& n=asset.nodes.at(i);return (n.parent>=0?path(n.parent)+"/":"")+(n.name.empty()?"@node:"+std::to_string(i):n.name);};
        for(std::size_t i=0;i<asset.nodes.size();++i){const auto& node=asset.nodes[i];report["nodes"].push_back({{"index",i},{"path",path(i)},{"name",node.name},{"translation",node.translation},{"rotation",node.rotation},{"scale",node.scale}});}
        report["preparedPositions"]=nlohmann::json::array();for(const auto& vertex:asset.vertices)report["preparedPositions"].push_back(vertex.position);
        std::ofstream out(argv[3]);out<<report.dump(2);if(!out)throw std::runtime_error("Cannot write asset inspection");return 0;
    }
    LoadedAsset asset;asset.nodes.resize(3);asset.nodes[0].name="body";
    asset.nodes[1].name="lid";asset.nodes[1].parent=0;asset.nodes[2].name="jaw";asset.nodes[2].parent=0;
    asset.defaultMorphWeights={{0,0,0},{},{}};
    MorphTargetRange range;range.node=0;range.targets.resize(3);range.targets[2].name="opening";asset.morphRanges={range};
    auto base=bindAnimationPose(asset);base.local[0].translation[0]=2;
    ExpressionPose blink;blink.id="fixture-blink";blink.skeletonFingerprint=expressionSkeletonFingerprint(asset);
    blink.bones.push_back({"body/lid",{0,-.1F,0},{0,0,0,1}});
    auto result=applyExpressionPose(asset,base,blink,1);
    check(result.local[0].translation[0]==2,"Body animation must survive face layer");
    check(result.local[1].translation[1]==-.1F,"Closed lid delta must apply");
    check(base.local[1].translation[1]==0,"Shared base pose remains immutable");
    ExpressionPose jaw=blink;jaw.bones={{"body/jaw",{0,-.2F,0},{0,0,0,1}}};
    jaw.morphs.push_back({"body","opening",.8F});
    auto mixed=applyExpressionPose(asset,result,jaw,.5F);
    check(mixed.local[1].translation[1]==-.1F&&mixed.local[2].translation[1]==-.1F,"Expression layers compose");
    check(std::abs(mixed.morphWeights[0][2]-.4F)<1e-6,"Third mechanical target must apply");
    const auto encoded=expressionPoseToJson(jaw);auto decoded=expressionPoseFromJson(encoded);
    check(decoded.morphs[0].targetId=="opening","Expression serialization retains stable target identity");
    bool rejected=false;try{jaw.skeletonFingerprint="wrong";(void)applyExpressionPose(asset,base,jaw,1);}catch(const std::exception&){rejected=true;}
    check(rejected,"Different skeleton must be rejected");
    rejected=false;try{blink.bones[0].path="missing";(void)applyExpressionPose(asset,base,blink,1);}catch(const std::exception&){rejected=true;}
    check(rejected,"Missing path must be rejected");
    std::cout<<"Expression body coexistence, layers, mechanical target and rejection passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
