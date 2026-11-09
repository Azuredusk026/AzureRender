#include "render/MaterialPipelinePolicy.hpp"
#include <iostream>
using namespace azurerender;
int main(){
    if(materialFallbackFragment("mesh_object_bindless_early_shadow_history.frag.spv",true,false)
        !="mesh_object_bindless_early_lighting.frag.spv")return 1;
    if(materialFallbackFragment("mesh_early_shadow_history.frag.spv",false,true)
        !="mesh_early_coverage.frag.spv")return 2;
    if(materialFallbackFragment("mesh_early_radiance_stats.frag.spv",true,true)
        !="mesh_early_lighting_coverage.frag.spv")return 3;
    if(materialFallbackFragment("/shadow_history/mesh_early.frag.spv",true,false)
        !="/shadow_history/mesh_early_lighting.frag.spv")return 4;
    bool invalid=false;
    try{materialFallbackFragment("mesh.vert.spv",true,false);}catch(const std::invalid_argument&){invalid=true;}
    if(!invalid)return 5;
    std::cout<<"Shadow history material fallbacks passed\n";
}
