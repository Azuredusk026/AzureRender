#version 450
#extension GL_GOOGLE_include_directive : require
#include "spatial_antialiasing.glsl"
#include "color_grading_lut.glsl"
layout(binding=0) uniform sampler2D linearSource;
layout(location=0) in vec2 screenUv;
layout(location=0) out vec4 outputColor;
layout(push_constant) uniform ResolveParameters { vec4 options; } resolve;
void main(){
    vec3 value;
    if(resolve.options.x>1.5){
        vec2 pixel=1.0/vec2(textureSize(linearSource,0));value=vec3(0);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)
            value+=textureLod(linearSource,screenUv+(vec2(x,y)-.5)*pixel,0).rgb*.25;
    }else if(resolve.options.x>.5&&resolve.options.z<.5)value=spatialEdgeResolve(linearSource,screenUv);
    else value=textureLod(linearSource,screenUv,0).rgb;
    if(resolve.options.y>.5)value=colorGradingLinearToSrgb(value);
    outputColor=vec4(value,1);
}
