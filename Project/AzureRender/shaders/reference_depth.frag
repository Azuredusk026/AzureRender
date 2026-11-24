#version 450
#if defined(AZURE_REFERENCE_DEPTH_ELLIPSE)
#extension GL_GOOGLE_include_directive : require
#include "material_uv_gradients.glsl"
#include "texture_footprint.glsl"
#endif
#if defined(AZURE_REFERENCE_DEPTH_MASKED)
layout(location=0) in vec2 textureCoordinate;
layout(binding=0) uniform sampler2D alphaMask;
layout(push_constant) uniform Projection {vec4 columns[4];vec4 jitter;uvec4 control;vec4 coverage;} projection;
#endif
layout(location=0) out float surfaceDepth;
void main(){
#if defined(AZURE_REFERENCE_DEPTH_MASKED)
#if defined(AZURE_REFERENCE_DEPTH_ELLIPSE)
    mat2 gradients=azureMirroredCoarseUvGradients(textureCoordinate);
    vec2 size=vec2(textureSize(alphaMask,0));
    float lod=azureEllipseMajorAxisLod(gradients[0]*size,gradients[1]*size)+projection.coverage.y;
    if(textureLod(alphaMask,textureCoordinate,lod)[projection.control.y]<projection.coverage.x)discard;
#else
    if(texture(alphaMask,textureCoordinate,projection.coverage.y)[projection.control.y]<projection.coverage.x)discard;
#endif
#endif
    surfaceDepth=gl_FragCoord.z;
}
