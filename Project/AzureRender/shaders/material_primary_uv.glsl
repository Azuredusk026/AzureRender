#ifndef AZURE_MATERIAL_PRIMARY_UV_GLSL
#define AZURE_MATERIAL_PRIMARY_UV_GLSL
#if defined(AZURE_REFERENCE_MATERIAL_UV)
#include "material_uv_gradients.glsl"
vec2 azurePrimaryUvDx,azurePrimaryUvDy;
void azurePreparePrimaryUv(vec2 uv){mat2 gradients=azureMirroredCoarseUvGradients(uv);azurePrimaryUvDx=gradients[0];azurePrimaryUvDy=gradients[1];}
vec4 azureSamplePrimaryUv(sampler2D image,vec2 uv){return textureGrad(image,uv,azurePrimaryUvDx,azurePrimaryUvDy);}
#define azurePrimaryDx(uv) azurePrimaryUvDx
#define azurePrimaryDy(uv) azurePrimaryUvDy
#else
#define azurePreparePrimaryUv(uv)
#define azureSamplePrimaryUv texture
#define azurePrimaryDx dFdx
#define azurePrimaryDy dFdy
#endif
#if defined(AZURE_REFERENCE_MATERIAL_UV) && defined(AZURE_INDEPENDENT_COVERAGE) && !defined(AZURE_GEOMETRY_SAMPLES)
#include "texture_footprint.glsl"
float azureSampleCoverageAlpha(sampler2D image,vec2 uv,uint features){
    if((features&0x10000000u)!=0u){
        vec2 size=vec2(textureSize(image,0));
        return textureLod(image,uv,azureEllipseMajorAxisLod(azurePrimaryUvDx*size,azurePrimaryUvDy*size)).a;
    }
    return azureSamplePrimaryUv(image,uv).a;
}
#else
#define azureSampleCoverageAlpha(image,uv,features) (azureSamplePrimaryUv(image,uv).a)
#endif
#endif
