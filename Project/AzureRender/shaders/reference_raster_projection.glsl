#ifndef AZURE_REFERENCE_RASTER_PROJECTION_GLSL
#define AZURE_REFERENCE_RASTER_PROJECTION_GLSL
#include "reference_position_frame.glsl"
#include "reference_clip_projection.glsl"
// Finite inputs and validated upload parameters are supplied by the caller.
vec4 azureComposeReferenceRasterPosition(vec4 referenceClip,vec4 deviceBase,vec4 deviceFinal,bool enabled){
    if(!enabled||referenceClip.w==0||deviceBase.w==0||deviceFinal.w==0)return deviceFinal;
    precise vec4 result=referenceClip;
    precise float depth=deviceFinal.z/deviceFinal.w;result.z=depth*result.w;
    if(any(notEqual(deviceBase.xyw,deviceFinal.xyw))){
        precise vec2 finalNdc=deviceFinal.xy/deviceFinal.w;
        precise vec2 baseNdc=deviceBase.xy/deviceBase.w;
        precise vec2 delta=finalNdc-baseNdc;
        precise vec2 offset=delta*result.w;result.xy=result.xy+offset;
    }
    if(any(isnan(result))||any(isinf(result)))return deviceFinal;
    return result;
}
vec4 azureReferenceRasterPosition(vec3 world,vec4 deviceBase,vec4 deviceFinal,vec4 columns[4],vec2 jitter,uint packedAxes,vec3 frameOffset,bool enabled,bool flipY){
    if(!enabled)return deviceFinal;
    vec4 referenceClip=azureReferenceClipProjection(azureReferenceRelativePosition(world,packedAxes,frameOffset),columns,jitter);
    if(flipY)referenceClip.y=-referenceClip.y;
    return azureComposeReferenceRasterPosition(referenceClip,deviceBase,deviceFinal,true);
}
#endif
