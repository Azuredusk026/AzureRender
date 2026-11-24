#version 450
#extension GL_GOOGLE_include_directive : require
#include "surface_radiance_identity.glsl"
layout(early_fragment_tests) in;
#if defined(AZURE_SURFACE_RADIANCE_PROBE)
layout(location=0) flat in uvec2 source;
layout(push_constant) uniform Probe {float depth;float value;uint firstIndex;uint material;} probe;
#else
layout(location=15,component=1) flat in uvec2 source;
layout(location=15,component=3) flat in uint drawSlot;
layout(push_constant) uniform Material {layout(offset=124) uint packedMaterial;} material;
#endif
layout(binding=41) uniform usampler2D identities;
layout(std430,binding=43) buffer Predicate {uint words[];} predicate;
void main(){
    if(gl_HelperInvocation)return;
    ivec2 pixel=ivec2(gl_FragCoord.xy),extent=textureSize(identities,0);
#if defined(AZURE_SURFACE_RADIANCE_PROBE)
    uint materialIndex=probe.material;
#else
    uint materialIndex=material.packedMaterial>>1u;
#endif
    uvec4 requested=azureSurfaceRadianceIdentity(gl_FragCoord.z,source,uint(gl_PrimitiveID),materialIndex);
    bool inside=all(greaterThanEqual(pixel,ivec2(0)))&&all(lessThan(pixel,extent));
    uvec4 stored=uvec4(0xffffffffu);if(inside)stored=texelFetch(identities,pixel,0);
    bool mismatch=!inside||requested.z==0xffffffffu||any(notEqual(requested,stored));
#if defined(AZURE_SURFACE_RADIANCE_PROBE)
    if(mismatch)atomicOr(predicate.words[0],1u);
#else
    if(drawSlot>=predicate.words[1]){atomicOr(predicate.words[0],17u);return;}
    bool witness=all(equal(pixel&ivec2(31),ivec2(0)));
    if(mismatch)atomicOr(predicate.words[6+drawSlot],3u);
    else if(witness)atomicOr(predicate.words[6+drawSlot],2u);
    if(witness)atomicAdd(predicate.words[6+drawSlot],4u);
#endif
}
