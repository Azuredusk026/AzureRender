#ifndef AZURE_VIEW_FOG_GLSL
#define AZURE_VIEW_FOG_GLSL
#include "fog_transport.glsl"
#include "fog_view_projection.glsl"
layout(std430,binding=34) readonly buffer ViewFogFrame {vec4 viewFogRows[4];};
layout(binding=35) uniform sampler3D viewFogVolume;
AzureFogTransport azureSampleViewFog(vec2 fragmentPixel,float viewDepth){
    AzureFogTransport result;result.scattering=vec3(0);result.transmittance=vec3(1);
    if(viewFogRows[3].z<.5)return result;
    vec2 referenceExtent=viewFogRows[2].yz;
    uvec2 pixel=uvec2(clamp(floor(fragmentPixel*referenceExtent/viewFogRows[3].xy),vec2(0),referenceExtent-1));
    vec4 projection[3]=vec4[3](viewFogRows[0],viewFogRows[1],viewFogRows[2]);
    vec4 coordinate=azureFogViewCoordinates(projection,pixel,viewDepth);
    vec4 value=textureLod(viewFogVolume,coordinate.xyz,0);
    value.rgb*=coordinate.w;value.a=mix(1.0,value.a,coordinate.w);
    return azureFogVolumeTransport(value);
}
#endif
