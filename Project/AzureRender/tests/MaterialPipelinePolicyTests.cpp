#include "render/MaterialPipelinePolicy.hpp"
#include <iostream>
using namespace azurerender;
int main(){
    AssetMaterial material;
    if(!ordinaryOpaqueMaterial(material))return 1;
    material.materialFeatures=0xffffffffU;
    if(!ordinaryOpaqueMaterial(material))return 2;
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
