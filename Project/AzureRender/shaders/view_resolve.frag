#version 450
#extension GL_GOOGLE_include_directive : require
#include "color_grading_lut.glsl"
layout(binding=0) uniform sampler2D hdrSource;
layout(binding=1) uniform sampler2D lutSource;
layout(location=0) in vec2 screenUv;
layout(location=0) out vec4 outputColor;
layout(push_constant) uniform OutputTransfer { vec4 grade; vec4 tintTransfer; vec4 lut; } outputTransfer;
vec3 linearToSrgb(vec3 value) {
    return mix(value*12.92,1.055*pow(value,vec3(1.0/2.4))-0.055,greaterThan(value,vec3(0.0031308)));
}
void main() {
    vec3 value=texture(hdrSource,screenUv).rgb;
    value*=exp2(outputTransfer.grade.x);
    if(outputTransfer.lut.x>1.5){
        value=sampleColorGradingLut(lutSource,value,outputTransfer.lut.x,outputTransfer.lut.y,outputTransfer.lut.z);
        if(outputTransfer.tintTransfer.w>.5)value=colorGradingLinearToSrgb(value);
        outputColor=vec4(value,1);return;
    }
    value=max(value,vec3(0));
    value=outputTransfer.grade.w>0.5?clamp((value*(2.51*value+0.03))/(value*(2.43*value+0.59)+0.14),0.0,1.0):clamp(value,0.0,1.0);
    value*=outputTransfer.tintTransfer.rgb;
    value=mix(vec3(dot(value,vec3(0.2126,0.7152,0.0722))),value,outputTransfer.grade.y);
    value=clamp((value-vec3(0.5))*outputTransfer.grade.z+vec3(0.5),0.0,1.0);
    if(outputTransfer.tintTransfer.w>0.5)value=linearToSrgb(value);
    outputColor=vec4(value,1.0);
}
