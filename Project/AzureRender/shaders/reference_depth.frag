#version 450
#if defined(AZURE_REFERENCE_DEPTH_MASKED)
layout(location=0) in vec2 textureCoordinate;
layout(binding=0) uniform sampler2D alphaMask;
layout(push_constant) uniform Projection {vec4 columns[4];vec4 jitter;uvec4 control;vec4 coverage;} projection;
#endif
layout(location=0) out float surfaceDepth;
void main(){
#if defined(AZURE_REFERENCE_DEPTH_MASKED)
    if(texture(alphaMask,textureCoordinate,projection.coverage.y)[projection.control.y]<projection.coverage.x)discard;
#endif
    surfaceDepth=gl_FragCoord.z;
}
