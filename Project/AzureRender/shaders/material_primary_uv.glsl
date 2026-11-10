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
#endif
