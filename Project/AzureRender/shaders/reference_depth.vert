#version 450
#extension GL_GOOGLE_include_directive : require
#include "reference_clip_projection.glsl"
layout(location=0) in vec4 position;
layout(push_constant) uniform Projection {vec4 columns[4];vec4 jitter;uvec4 control;} projection;
void main(){
    gl_Position=azureReferenceClipProjection(position.xyz,projection.columns,projection.jitter.xy);
    if(projection.control.x!=0u)gl_Position.y=-gl_Position.y;
}
