#version 450
#extension GL_GOOGLE_include_directive : require
#include "reference_clip_projection.glsl"
#if defined(AZURE_REFERENCE_DEPTH_POSITION_FRAME)
#include "reference_position_frame.glsl"
#endif
layout(location=0) in vec4 position;
#if defined(AZURE_REFERENCE_DEPTH_MASKED)
layout(location=1) in vec2 uv;
layout(location=0) out vec2 textureCoordinate;
#endif
layout(push_constant) uniform Projection {vec4 columns[4];vec4 jitter;uvec4 control;
#if defined(AZURE_REFERENCE_DEPTH_POSITION_FRAME)
vec4 coverage;vec4 frameOffset;
#endif
} projection;
void main(){
#if defined(AZURE_REFERENCE_DEPTH_MASKED)
    textureCoordinate=uv;
#endif
#if defined(AZURE_REFERENCE_DEPTH_POSITION_FRAME)
    vec3 relative=azureReferenceRelativePosition(position.xyz,projection.control.z,projection.frameOffset.xyz);
    gl_Position=azureReferenceClipProjection(relative,projection.columns,projection.jitter.xy);
#else
    gl_Position=azureReferenceClipProjection(position.xyz,projection.columns,projection.jitter.xy);
#endif
    if(projection.control.x!=0u)gl_Position.y=-gl_Position.y;
}
