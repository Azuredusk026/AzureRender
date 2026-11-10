#ifndef AZURE_MATERIAL_UV_GRADIENTS_GLSL
#define AZURE_MATERIAL_UV_GRADIENTS_GLSL
#extension GL_KHR_shader_subgroup_quad : require
// Compute in uniform control flow before any fragment discard.
// Columns contain horizontal bottom-row and vertical left-column footprints.
mat2 azureMirroredCoarseUvGradients(vec2 uv){
    vec2 dx=dFdxFine(uv),dy=dFdyFine(uv);
    return mat2(subgroupQuadBroadcast(dx,2u),subgroupQuadBroadcast(dy,0u));
}
#endif
