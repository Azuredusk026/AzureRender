#include "assets/AssetDeformation.hpp"
#include <cmath>
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
    return 0;
}
