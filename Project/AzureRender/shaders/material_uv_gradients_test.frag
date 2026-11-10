#version 450
#extension GL_GOOGLE_include_directive : require
#include "material_uv_gradients.glsl"
layout(location=0) out vec4 outputColor;
layout(push_constant) uniform Parameters {uint discardPattern;} parameters;
void main(){
    float product=gl_FragCoord.x*gl_FragCoord.y;
    vec2 uv=vec2(product,-2.0*product)*(1.0/1024.0);
    mat2 gradients=azureMirroredCoarseUvGradients(uv);
    if(parameters.discardPattern!=0u&&((uint(gl_FragCoord.x)+uint(gl_FragCoord.y))&3u)==0u)discard;
    outputColor=vec4(gradients[0],gradients[1]);
}
