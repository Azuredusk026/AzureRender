#include "render/MaterialPipelinePolicy.hpp"
#include <iostream>
using namespace azurerender;
int main(){
    AssetMaterial material;
    if(!ordinaryOpaqueMaterial(material))return 1;
    material.materialFeatures=0xffffffffU;
    if(ordinaryOpaqueMaterial(material))return 2;
    material.materialFeatures=0;
    if(!ordinaryOpaqueMaterial(material))return 5;
    material.materialFeatures=MaterialFeatureEmissiveMask;
    if(!ordinaryOpaqueMaterial(material))return 7;
    material.materialFeatures=MaterialFeatureNeutralFallback|MaterialFeatureEmissiveMask;
    if(!ordinaryOpaqueMaterial(material))return 8;
    for(unsigned bit=0;bit<32;++bit){
        material.materialFeatures=MaterialFeatureNeutralFallback|(1U<<bit);
        if(ordinaryOpaqueMaterial(material)!=(bit==3||bit==5))return 6;
    }
    material.materialFeatures=MaterialFeatureNeutralFallback;
    for(auto mode:{AssetAlphaMode::Mask,AssetAlphaMode::Blend}){
        material.alphaMode=mode;if(ordinaryOpaqueMaterial(material))return 3;
    }
    material.alphaMode=AssetAlphaMode::Opaque;
    for(unsigned category=1;category<=10;++category){
        material.materialClass=static_cast<AssetMaterialClass>(category);
        if(ordinaryOpaqueMaterial(material))return 4;
    }
    std::cout<<"Ordinary opaque policy: final class/alpha eligibility passed\n";
}
