#include "assets/AssetDeformation.hpp"
#include <cmath>
#include "render/DeformedBounds.hpp"
#include <iostream>
int main(){
    LoadedAsset asset;asset.vertices.resize(1);auto& vertex=asset.vertices[0];vertex.position={1,2,3};vertex.morph0={2,0,0};vertex.morph1={0,4,0};vertex.weights={.25F,.75F,0,0};vertex.joints={0,1,0,0};
    AssetPose first,second;first.jointMatrices={azurerender::internal::translation(4,0,0),azurerender::internal::translation(0,8,0)};
    second.jointMatrices={azurerender::internal::translation(-4,0,0),azurerender::internal::translation(0,-8,0)};
    const auto a=azurerender::deformedVertexPosition(asset,0,&first,{.5F,.25F});
    const auto b=azurerender::deformedVertexPosition(asset,0,&second,{.5F,.25F});
    if(std::abs(a[0]-3)>1e-6F||std::abs(a[1]-9)>1e-6F||a[2]!=3||std::abs(b[0]-1)>1e-6F||std::abs(b[1]+3)>1e-6F){std::cerr<<"Picking positions must apply morph before each instance's current joint pose\n";return 1;}
    const auto unskinned=azurerender::deformedVertexPosition(asset,0,nullptr,{.5F,.25F});
    if(unskinned!=std::array<float,3>{2,3,3})return 2;
    MorphTargetRange range;range.node=0;range.firstVertex=0;range.vertexCount=1;range.targets.resize(3);
    range.targets[2].positions={{{0,0,8}}};asset.morphRanges={range};
    first.morphWeights={{0,0,.5F}};second.morphWeights={{0,0,-.25F}};
    const auto third=azurerender::deformedVertexPosition(asset,0,&first,{0,0});
    const auto independent=azurerender::deformedVertexPosition(asset,0,&second,{0,0});
    if(third[2]!=7||independent[2]!=1){std::cerr<<"Third morph must use instance pose weights before skinning\n";return 3;}
    asset.hasSkin=true;asset.jointMatrices=first.jointMatrices;asset.defaultMorphWeights={{0,0,.5F}};
    range.targets[2].normals={{{0,-2,2}}};range.targets[2].tangents={{{-2,0,2}}};asset.morphRanges={range};
    const auto normal=azurerender::deformedVertexDirection(asset,0,&first,1);
    const auto tangent=azurerender::deformedVertexDirection(asset,0,&first,2);
    if(normal[2]<.999F||tangent[2]<.999F)return 4;
    const auto envelopes=azurerender::JointBounds::build(asset);
    const auto positive=envelopes.evaluate(asset,{0,0},first.jointMatrices,&first);
    const auto negative=envelopes.evaluate(asset,{0,0},second.jointMatrices,&second);
    if(positive.maximum[2]<7||negative.minimum[2]>1)return 5;
    const auto saved=first.morphWeights;
    bool rejected=false;try{azurerender::applyMorphOverrides(asset,first,{1});}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected||first.morphWeights!=saved)return 6;
    azurerender::applyMorphOverrides(asset,first,{0,0,-1});
    if(first.morphWeights[0][2]!=-1||second.morphWeights[0][2]!=-.25F)return 7;
    asset.hasSkin=false;first.jointMatrices.clear();
    if(azurerender::deformedVertexPosition(asset,0,&first,{0,0})[2]!=-5)return 8;
    return 0;
}
