#ifndef AZURE_INSTANCE_DATA
#define AZURE_INSTANCE_DATA 1
#define AZURE_INSTANCE_DATA_LAYOUT_VERSION 2
const uint AZURE_INSTANCE_MATERIAL_RIM_ORIGIN = 1U;
struct InstanceTransforms {
    mat4 model;
    mat4 modelViewProjection;
    mat4 cascadeLightModelViewProjection[4];
    uvec4 meta;
    vec4 faceLight;
    vec2 morph;
    uint objectShadowIndex;
    uint instanceFlags;
    mat4 faceFrame;
};
#endif
