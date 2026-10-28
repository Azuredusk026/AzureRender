#ifndef AZURE_VIEW_FOG_GLSL
#define AZURE_VIEW_FOG_GLSL
#include "fog_transport.glsl"
#include "fog_view_projection.glsl"
#include "fog_analytic.glsl"
layout(std430,binding=34) readonly buffer ViewFogFrame {vec4 viewFogRows[21];};
layout(binding=35) uniform sampler3D viewFogVolume;
void azureFogWorldRay(vec2 fragmentPixel,float depth,out vec3 origin,out vec3 position){
    vec2 screen=vec2(2*fragmentPixel.x/viewFogRows[3].x-1,1-2*fragmentPixel.y/viewFogRows[3].y);
    vec3 offset=viewFogRows[6].xyz*(screen.x-viewFogRows[8].x)+viewFogRows[7].xyz*(screen.y+viewFogRows[8].y);
    bool ortho=viewFogRows[4].w>.5;origin=viewFogRows[4].xyz+(ortho?offset:vec3(0));
    position=origin+(viewFogRows[5].xyz+(ortho?vec3(0):offset))*depth;
}
AzureFogTransport azureViewAnalyticFog(vec2 pixel,float depth){
    vec3 origin,position;azureFogWorldRay(pixel,depth,origin,position);AzureFogTransport height=azureIdentityFog();
    if(viewFogRows[8].w>.5&&depth>viewFogRows[8].z){
        vec3 begin=mix(origin,position,viewFogRows[8].z/depth);
        for(int i=0;i<2;++i)if(float(i)<viewFogRows[8].w)
            height=azureComposeFogTransport(height,azureEvaluateHeightFog(viewFogRows[9+2*i].xyz,viewFogRows[10+2*i].xyz,begin,position));
        float distance=length(position-origin);vec4 bias=viewFogRows[14];
        float original=height.transmittance.x;
        float adjusted=clamp(max(original,viewFogRows[13].x)+clamp(bias.x+distance*bias.y,0,1)+clamp(bias.z+distance*bias.w,0,1),0,1);
        if(adjusted!=original){height.scattering*=original<1?(1-adjusted)/(1-original):0;height.transmittance=vec3(adjusted);}
    }
    AzureFogTransport air=azureIdentityFog();
    if(viewFogRows[13].y>.5){AzureAerialFog p;for(int i=0;i<6;++i)p.rows[i]=viewFogRows[15+i];air=azureEvaluateAerialFog(p,origin,position);}
    return azureComposeFogTransport(height,air);
}
AzureFogTransport azureSampleViewFog(vec2 fragmentPixel,float viewDepth){
    AzureFogTransport result;result.scattering=vec3(0);result.transmittance=vec3(1);
    if(viewFogRows[3].z>.5){
        vec2 referenceExtent=viewFogRows[2].yz;
        uvec2 pixel=uvec2(clamp(floor(fragmentPixel*referenceExtent/viewFogRows[3].xy),vec2(0),referenceExtent-1));
        vec4 projection[3]=vec4[3](viewFogRows[0],viewFogRows[1],viewFogRows[2]);
        vec4 coordinate=azureFogViewCoordinates(projection,pixel,viewDepth);
        vec4 value=textureLod(viewFogVolume,coordinate.xyz,0);
        value.rgb*=coordinate.w;value.a=mix(1.0,value.a,coordinate.w);result=azureFogVolumeTransport(value);
    }
    if(viewFogRows[3].w>.5)result=azureComposeFogTransport(result,azureViewAnalyticFog(fragmentPixel,viewDepth));
    return result;
}
#endif
