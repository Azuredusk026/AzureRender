#ifndef AZURE_SKIN_POSITION_GLSL
#define AZURE_SKIN_POSITION_GLSL
// Joint matrices are affine. Weight rounding affects XYZ, not point identity.
// This matches the position stored by compute skinning and outline expansion.
vec4 azureSkinPosition(mat4 skin,vec3 position) {
    return vec4((skin*vec4(position,1.0)).xyz,1.0);
}
#endif
