#ifndef AZURE_VIEW_FOG_MEDIUM_GLSL
#define AZURE_VIEW_FOG_MEDIUM_GLSL
#include "fog_analytic.glsl"
#ifndef AZURE_VIEW_FOG_PARAMETER_BINDING
#define AZURE_VIEW_FOG_PARAMETER_BINDING 34
#endif
layout(std430,binding=AZURE_VIEW_FOG_PARAMETER_BINDING) readonly buffer ViewFogFrame {
    vec4 viewFogRows[21];
};

void azureFogWorldRay(vec2 unitPixel,float depth,out vec3 origin,out vec3 position){
    vec2 screen=vec2(2*unitPixel.x-1,1-2*unitPixel.y);
    vec3 offset=viewFogRows[6].xyz*(screen.x-viewFogRows[8].x)+viewFogRows[7].xyz*(screen.y+viewFogRows[8].y);
    bool ortho=viewFogRows[4].w>.5;
    origin=viewFogRows[4].xyz+(ortho?offset:vec3(0));
    position=origin+(viewFogRows[5].xyz+(ortho?vec3(0):offset))*depth;
}

AzureFogTransport azureViewHeightFog(vec3 origin,vec3 position,float depth){
    AzureFogTransport height=azureIdentityFog();
    if(viewFogRows[8].w<=.5||depth<=viewFogRows[8].z)return height;
    vec3 begin=mix(origin,position,viewFogRows[8].z/depth);
    for(int i=0;i<2;++i)if(float(i)<viewFogRows[8].w)
        height=azureComposeFogTransport(height,azureEvaluateHeightFog(viewFogRows[9+2*i].xyz,viewFogRows[10+2*i].xyz,begin,position));
    float distance=length(position-origin);vec4 bias=viewFogRows[14];
    float original=height.transmittance.x;
    float adjusted=clamp(max(original,viewFogRows[13].x)+clamp(bias.x+distance*bias.y,0,1)+clamp(bias.z+distance*bias.w,0,1),0,1);
    if(adjusted!=original){height.scattering*=original<1?(1-adjusted)/(1-original):0;height.transmittance=vec3(adjusted);}
    return height;
}

float azureHeightFogLogDepthRange(){
    return log2(1+(viewFogRows[13].w-viewFogRows[8].z));
}
#endif
