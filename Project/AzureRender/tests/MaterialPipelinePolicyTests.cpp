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
    if(sceneFragmentVariant("mesh.frag.spv",false,false)!="mesh.frag.spv")return 10;
    if(sceneFragmentVariant("mesh.frag.spv",true,false)!="mesh_samples.frag.spv")return 11;
    if(sceneFragmentVariant("mesh.frag.spv",false,true)!="mesh_clipmap.frag.spv")return 12;
    if(sceneFragmentVariant("/character_snapshot_object_bindless.frag.spv",true,true)
        !="/character_snapshot_object_bindless_samples_clipmap.frag.spv")return 13;
    bool invalid=false;try{(void)sceneFragmentVariant("mesh.vert.spv",false,true);}catch(const std::invalid_argument&){invalid=true;}
    if(!invalid)return 14;
    if(sceneFragmentVariant("mesh.frag.spv",false,false,true)!="mesh_field.frag.spv")return 15;
    if(sceneFragmentVariant("mesh.frag.spv",true,true,true)!="mesh_samples_clipmap_field.frag.spv")return 16;

    if(surfaceFacingIndex(false,false)!=0||surfaceFacingIndex(false,true)!=1
        ||surfaceFacingIndex(true,false)!=2||surfaceFacingIndex(true,true)!=3)return 17;

    const auto empty=fixedMaterialFeatures(0xffffffffU,0);
    if(empty.fixedMask||empty.fixedValue||!empty.matches(0xffffffffU))return 18;
    const auto shared=fixedMaterialFeatures(32,32);
    if(shared.fixedMask!=0x3fffffffU||shared.fixedValue!=32
        ||!shared.matches(32|0xc0000000U)||shared.matches(0)||shared.matches(33))return 19;
    const auto varied=fixedMaterialFeatures(32,40);
    if(varied.fixedMask!=0x3ffffff7U||varied.fixedValue!=32
        ||!varied.matches(32)||!varied.matches(40|0xc0000000U)
        ||varied.matches(0)||varied.matches(33))return 20;
    const auto viewOwned=fixedMaterialFeatures(32|0xc0000000U,40|0xc0000000U);
    if(viewOwned.fixedMask!=varied.fixedMask||viewOwned.fixedValue!=varied.fixedValue)return 21;
    const auto dynamic=fixedMaterialFeatures(0,0xffffffffU);
    if(dynamic.fixedMask||dynamic.fixedValue||!dynamic.matches(0xffffffffU))return 22;
    std::cout<<"Material feature policy: common bits, varying bits, view ownership and changed-material fallback passed\n";

}
