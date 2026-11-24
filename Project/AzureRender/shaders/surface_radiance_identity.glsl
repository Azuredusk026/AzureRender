#include "beauty_composition.glsl"
#ifndef AZURE_SURFACE_RADIANCE_IDENTITY_GLSL
#define AZURE_SURFACE_RADIANCE_IDENTITY_GLSL
uvec4 azureSurfaceRadianceIdentity(float depth,uvec2 source,uint triangle,uint materialIndex){
    if(isnan(depth)||isinf(depth)||source.y==0xffffffffu||triangle>(0xfffffffeu-source.y)/3u)return uvec4(0xffffffffu);
    return uvec4(floatBitsToUint(depth),source.x,source.y+triangle*3u,materialIndex);
}
bool azureSurfaceRadianceFinite(vec4 value){return !any(isnan(value))&&!any(isinf(value));}
vec3 azureComposeSurfaceRadiance(vec4 a,vec4 b,vec4 c,vec4 reflection){
    vec3 radiance=mix(c.rgb,reflection.rgb,reflection.a);
    vec3 blended=b.rgb*radiance;
    return azureBeautyReplace(a.rgb,blended,vec3(a.a,b.a,c.a));
}
#endif
