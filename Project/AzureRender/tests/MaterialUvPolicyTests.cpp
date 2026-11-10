#include "assets/MaterialUvPolicy.hpp"
#include "render/MaterialUvGradients.hpp"
#include <iostream>
using namespace azurerender;
int main()try{
    AssetMaterial m;m.materialProfileVersion=2;m.materialClass=AssetMaterialClass::Generic;m.alphaMode=AssetAlphaMode::Opaque;
    validateMaterialUvPolicy(m);m.materialFeatures|=MaterialFeatureMirroredCoarseUv;validateMaterialUvPolicy(m);
    m.materialClass=AssetMaterialClass::Hair;validateMaterialUvPolicy(m);m.alphaMode=AssetAlphaMode::Mask;validateMaterialUvPolicy(m);
    unsigned refused=0;const auto reject=[&](AssetMaterial v){try{validateMaterialUvPolicy(v);}catch(const std::invalid_argument&){++refused;}};
    auto v=m;v.materialProfileVersion=1;reject(v);v=m;v.alphaMode=AssetAlphaMode::Blend;reject(v);
    for(auto f:{MaterialFeatureHairPixelIntegral,MaterialFeatureUnlitSurface,MaterialFeatureEyeRadialSurface,MaterialFeatureUnlitTextureLayers,MaterialFeatureSceneRefraction}){v=m;v.materialFeatures|=f;reject(v);}
    for(unsigned kind:{10U,31U,32U,0xffffffffU}){v=m;v.materialClass=static_cast<AssetMaterialClass>(kind);reject(v);}
    v=m;v.alphaMode=static_cast<AssetAlphaMode>(99);reject(v);
    if(refused!=12)throw std::runtime_error("UV policy rejects failed: "+std::to_string(refused)+"/12");
    if(materialUvGradientProgram("/shaders/mesh_bindless_field_lighting_coverage.frag.spv")!="/shaders/mesh_bindless_field_coverage_uvquad.frag.spv")throw std::runtime_error("UV program family canonicalization failed");
    if(materialUvGradientProgram("/shaders/mesh_early.frag.spv"))throw std::runtime_error("Early-depth path must remain a separate family");
    if(materialUvGradientProgram("/shaders/shadow_bindless.frag.spv")!="/shaders/shadow_bindless_uvquad.frag.spv")throw std::runtime_error("Shadow UV family missing");
    std::cout<<"Material UV ownership and program selection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
