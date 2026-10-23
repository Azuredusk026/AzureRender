#ifndef AZURE_INSTANCE_DATA
#define AZURE_INSTANCE_DATA 1
#define AZURE_INSTANCE_DATA_LAYOUT_VERSION 1
struct InstanceTransforms {
    mat4 model;
    mat4 modelViewProjection;
    mat4 cascadeLightModelViewProjection[4];
    uvec4 meta;
    vec4 faceLight;
    vec4 morph;
    mat4 faceFrame;
};
#endif
