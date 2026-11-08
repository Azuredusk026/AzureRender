#if defined(AZURE_SURFACE_RADIANCE_REUSE)
#include "surface_radiance_identity.glsl"
layout(location=15,component=1) flat in uvec2 surfaceRadianceSource;
#if defined(AZURE_OPAQUE_SNAPSHOT)
layout(location=2) out vec4 outputRadianceA;
layout(location=3) out vec4 outputRadianceB;
layout(location=4) out vec4 outputRadianceC;
layout(location=5) out uvec4 outputRadianceIdentity;
#else
layout(binding=38) uniform sampler2D surfaceRadianceA;
layout(binding=39) uniform sampler2D surfaceRadianceB;
layout(binding=40) uniform sampler2D surfaceRadianceC;
layout(binding=41) uniform usampler2D surfaceRadianceIdentity;
#endif
bool azureSurfaceRadianceMode(){
#if defined(AZURE_EXTENDED_MATERIAL_LIGHTING) || defined(AZURE_INDEPENDENT_COVERAGE)
    return false;
#else
    return azureOrdinaryOpaque&&camera.clusterLighting.x>.5
        &&int(floor(camera.qaParameters.x+.5))==0&&int(floor(camera.qaParameters.y+.5))==0
        &&(camera.screenReflectionTrace.x<=.5||camera.screenReflectionTrace.x>1.5);
#endif
}
#if defined(AZURE_OPAQUE_SNAPSHOT)
void azureStoreSurfaceRadiance(vec3 beauty,vec3 response,vec3 radiance,vec3 ambientSpecular,vec4 blend){
    if(!azureSurfaceRadianceMode()||blend.a!=1.0)return;
    vec4 a=vec4(beauty,ambientSpecular.x),b=vec4(response,ambientSpecular.y),c=vec4(radiance,ambientSpecular.z);
    if(!azureSurfaceRadianceFinite(a)||!azureSurfaceRadianceFinite(b)||!azureSurfaceRadianceFinite(c))return;
    outputRadianceA=a;outputRadianceB=b;outputRadianceC=c;
    outputRadianceIdentity=azureSurfaceRadianceIdentity(gl_FragCoord.z,surfaceRadianceSource,uint(gl_PrimitiveID),material.materialPadding>>1u);
}
#else
bool azureWriteSurfaceRadiance(vec3 geometricNormal){
    if(!azureSurfaceRadianceMode())return false;
    ivec2 pixel=ivec2(gl_FragCoord.xy);
    uvec4 requested=azureSurfaceRadianceIdentity(gl_FragCoord.z,surfaceRadianceSource,uint(gl_PrimitiveID),material.materialPadding>>1u);
    ivec2 extent=textureSize(surfaceRadianceIdentity,0);uvec4 stored=uvec4(0xffffffffu);
    bool inside=all(greaterThanEqual(pixel,ivec2(0)))&&all(lessThan(pixel,extent));
    if(inside)stored=texelFetch(surfaceRadianceIdentity,pixel,0);
    bool matched=inside&&requested.z!=0xffffffffu&&all(equal(requested,stored));
    // Keep implicit sampling and receiver derivatives defined in fallback quads.
    if(!subgroupQuadAll(matched))return false;
    vec4 a=texelFetch(surfaceRadianceA,pixel,0),b=texelFetch(surfaceRadianceB,pixel,0),c=texelFetch(surfaceRadianceC,pixel,0);
    vec3 beauty=a.rgb;
    if(camera.screenReflectionTrace.x>1.5){
        vec4 reflection=sampleComputedScreenReflection(AZURE_FRAGMENT_COORD.xy/vec2(textureSize(opaqueSceneColor,0)));
        beauty=azureComposeSurfaceRadiance(a,b,c,reflection);
    }
    float radianceScale=azureMaterialRadianceScale();if(radianceScale!=1.0)beauty*=radianceScale;
    outputColor=vec4(beauty,1.0);
    if((material.materialPadding&AZURE_SELECTED_PADDING)!=0U)outputColor.rgb=mix(outputColor.rgb,vec3(.96,.62,.10),.42);
    outputNormal=vec4(geometricNormal*.5+.5,0);azureWriteDisplayMask(1.0);return true;
}
#endif
#endif
