#version 450
#extension GL_GOOGLE_include_directive : require
#include "sky.glsl"
layout(binding=0) uniform Sky{vec4 right,up,forward,color,parameters;} camera;
layout(binding=1) uniform sampler2D environmentTexture;
layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

vec2 directionToEquirectangular(vec3 direction) {
    const float pi = 3.14159265358979323846;
    vec3 normalizedDirection = normalize(direction);
    float u = atan(normalizedDirection.z, normalizedDirection.x) * 0.5 / pi + 0.5;
    float v = acos(clamp(normalizedDirection.y, -1.0, 1.0)) / pi;
    return vec2(u, v);
}

// Infinite sky uses orientation and projection, never camera translation.
void main() {
    vec3 direction=normalize(camera.forward.xyz
        +(screenUv.x*2.0-1.0)*camera.right.xyz
        +(1.0-screenUv.y*2.0)*camera.up.xyz);
    float angle=camera.parameters.z;
    direction.xz=mat2(cos(angle),sin(angle),-sin(angle),cos(angle))*direction.xz;
    vec3 color;
    if(camera.parameters.x>1.5){
        color=camera.color.rgb;
    }else if(camera.parameters.x>.5){
        color=texture(environmentTexture,directionToEquirectangular(direction)).rgb;
    }else{
        color=azureProceduralSky(direction);
    }
    outputColor=vec4(color*camera.parameters.y,1.0);
    outputNormal=vec4(.5,.5,1.0,0.0);
}
