#ifndef AZURE_REFERENCE_CLIP_PROJECTION_GLSL
#define AZURE_REFERENCE_CLIP_PROJECTION_GLSL
#extension GL_EXT_spirv_intrinsics : require
// Columns consume a camera-relative position. Normalized UV jitter follows
// upper-left UV: subtract twice X and add twice Y in homogeneous clip space.
vec4 azureReferenceClipProjection(vec3 position,vec4 columns[4],vec2 jitter){
    precise vec4 value=position.y*columns[1];
    spirv_decorate(42) const vec4 first=fma(columns[0],vec4(position.x),value);value=first;
    spirv_decorate(42) const vec4 second=fma(columns[2],vec4(position.z),value);value=second;
    value=value+columns[3];
    precise vec2 offset=value.w*jitter;
    spirv_decorate(42) const vec2 shifted=fma(-offset,vec2(2,-2),value.xy);value.xy=shifted;
    return value;
}
#endif
