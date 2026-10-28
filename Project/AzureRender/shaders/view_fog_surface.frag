#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_fog.glsl"
layout(binding=18) uniform sampler2D sceneLinearDepth;
layout(constant_id=0) const bool scatterPass=false;
layout(location=0) in vec2 screenUv;
layout(location=0) out vec4 outColor;
void main(){
    ivec2 size=textureSize(sceneLinearDepth,0);
    float depth=texelFetch(sceneLinearDepth,clamp(ivec2(screenUv*vec2(size)),ivec2(0),size-1),0).r;
    if(depth>=100000.0)discard;
    AzureFogTransport fog=azureSampleViewFog(gl_FragCoord.xy,depth);
    outColor=vec4(scatterPass?fog.scattering:fog.transmittance,1);
}
