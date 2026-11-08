#version 450
#extension GL_GOOGLE_include_directive : require
#if defined(AZURE_SURFACE_RADIANCE_CONSUMER)
#extension GL_KHR_shader_subgroup_vote : require
#extension GL_EXT_shader_quad_control : require
#extension GL_EXT_maximal_reconvergence : require
layout(quad_derivatives) in;
layout(full_quads) in;
#endif
#include "surface_radiance_identity.glsl"
#include "surface_radiance_statistics.glsl"
layout(push_constant) uniform Probe {float depth;float value;uint firstIndex;uint material;float coverage;float mismatchFromX;} probe;
layout(location=0) flat in uvec2 source;
layout(location=0) out vec4 outputA;
layout(location=1) out vec4 outputB;
layout(location=2) out vec4 outputC;
layout(location=3) out uvec4 identity;
#if defined(AZURE_SURFACE_RADIANCE_CONSUMER)
layout(binding=38) uniform sampler2D inputA;
layout(binding=39) uniform sampler2D inputB;
layout(binding=40) uniform sampler2D inputC;
layout(binding=41) uniform usampler2D inputIdentity;
#endif
void main()
#if defined(AZURE_SURFACE_RADIANCE_CONSUMER)
[[maximally_reconverges]]
#endif
{
    identity=azureSurfaceRadianceIdentity(gl_FragCoord.z,source,uint(gl_PrimitiveID),probe.material);
    outputA=vec4(probe.value,10.015625,1.0009765625,1);
    outputB=vec4(.25,.5,.75,1);outputC=vec4(4,2,1,.75);
#if defined(AZURE_SURFACE_RADIANCE_CONSUMER)
    ivec2 pixel=ivec2(gl_FragCoord.xy);uvec4 stored=texelFetch(inputIdentity,pixel,0);
#if defined(AZURE_SURFACE_RADIANCE_DIAGNOSTICS)
    azureRadianceCount(1u);azureRadianceCount(2u);
    bool matched=identity.z!=0xffffffffu&&all(equal(identity,stored));bool accepted=subgroupQuadAll(matched);
    if(matched)azureRadianceCount(3u);if(accepted)azureRadianceCount(4u);
    if(accepted){
#else
    if(subgroupQuadAll(identity.z!=0xffffffffu&&all(equal(identity,stored)))){
#endif
        vec4 a=texelFetch(inputA,pixel,0),b=texelFetch(inputB,pixel,0),c=texelFetch(inputC,pixel,0);
        if(azureSurfaceRadianceFinite(a)&&azureSurfaceRadianceFinite(b)&&azureSurfaceRadianceFinite(c))
            outputA=vec4(azureComposeSurfaceRadiance(a,b,c,vec4(8,6,5,.5)),1);
#if defined(AZURE_SURFACE_RADIANCE_FAST_PROBE)
        else discard;
#endif
    }
#if defined(AZURE_SURFACE_RADIANCE_FAST_PROBE)
    else discard;
#endif
#else
    if(probe.mismatchFromX>=0&&gl_FragCoord.x>=probe.mismatchFromX)++identity.w;
    if(probe.value==-2.0&&((int(gl_FragCoord.x)&1)==1))++identity.w;
    if(!azureSurfaceRadianceFinite(outputA))identity=uvec4(0xffffffffu);
#if defined(AZURE_SURFACE_RADIANCE_DIAGNOSTICS)
    if(identity.z!=0xffffffffu)azureRadianceCount(0u);
#endif
#endif
}
