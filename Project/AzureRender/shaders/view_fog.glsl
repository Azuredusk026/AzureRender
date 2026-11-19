#ifndef AZURE_VIEW_FOG_GLSL
#define AZURE_VIEW_FOG_GLSL
#include "fog_transport.glsl"
#include "fog_view_projection.glsl"
#include "view_fog_medium.glsl"
layout(binding=35) uniform sampler3D viewFogVolume;
AzureFogTransport azureViewAnalyticFog(vec2 pixel,float depth){
    vec2 unitPixel=pixel/viewFogRows[3].xy;
    vec3 origin,position;
    azureFogWorldRay(unitPixel,depth,origin,position);
    AzureFogTransport height;
    if(viewFogRows[13].z>.5&&depth>viewFogRows[8].z&&depth<=viewFogRows[13].w){
        float slices=float(textureSize(viewFogVolume,0).z);
        float fraction=log2(1+(depth-viewFogRows[8].z))/azureHeightFogLogDepthRange();
        float slice=(clamp(fraction,0,1)*(slices-1)+.5)/slices;
        height=azureFogVolumeTransport(textureLod(viewFogVolume,vec3(unitPixel,slice),0));
    }else{
        height=azureViewHeightFog(origin,position,depth);
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
