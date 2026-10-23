#version 450
#extension GL_GOOGLE_include_directive : require
#include "outline_surface_fragment.glsl"
layout(location=1) out vec4 outputDepth;
void main() {
    if(!shellCovered())discard;
    float denominator=gl_FragCoord.z*shellDepthProjection.z-shellDepthProjection.x;
    float depth=abs(denominator)>1e-12?
        (shellDepthProjection.y-gl_FragCoord.z*shellDepthProjection.w)/denominator:100000.0;
    outputDepth=vec4(clamp(depth,0,100000),0,0,1);
}
