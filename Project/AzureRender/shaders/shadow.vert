#version 450

layout(binding = 0) uniform CameraData {
    vec4 cameraPosition;
    vec4 cameraForward;
    vec4 clusterGrid;
    vec4 clusterDepth;
    vec4 clusterLighting;
    vec4 cascadeSplits;
    vec4 renderingParameters;
    vec4 showcaseParameters;
    vec4 qaParameters;
    vec4 faceLightDirection;
    vec4 faceSdfParameters;
    vec4 faceSdfShadowColor;
} camera;

layout(std430, binding = 10) readonly buffer JointData {
    mat4 matrices[];
} jointData;

struct InstanceTransforms {
    mat4 model;
    mat4 modelViewProjection;
    mat4 cascadeLightModelViewProjection[4];
    uvec4 meta;
};
layout(std430, binding = 13) readonly buffer InstanceData {
    InstanceTransforms instances[];
} instanceData;

layout(push_constant) uniform ShadowParameters {
    layout(offset = 128) vec2 morphWeights;
    layout(offset = 208) uint cascadeIndex;
} shadowParameters;

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 6) in vec3 morph0;
layout(location = 7) in vec3 morph1;
layout(location = 0) out vec2 textureCoordinate;

void main() {
#if defined(AZURE_COMPUTE_SKINNING)
    vec4 skinnedPosition = vec4(position, 1.0);
#else
    const uint jointBase = instanceData.instances[gl_InstanceIndex].meta.x;
    vec3 morphedPosition = position
        + morph0 * shadowParameters.morphWeights.x
        + morph1 * shadowParameters.morphWeights.y;
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointBase + jointIndices.x]
        + jointWeights.y * jointData.matrices[jointBase + jointIndices.y]
        + jointWeights.z * jointData.matrices[jointBase + jointIndices.z]
        + jointWeights.w * jointData.matrices[jointBase + jointIndices.w];
    vec4 skinnedPosition = skinMatrix * vec4(morphedPosition, 1.0);
#endif
    gl_Position =
        instanceData.instances[gl_InstanceIndex]
            .cascadeLightModelViewProjection[shadowParameters.cascadeIndex]
        * skinnedPosition;
    textureCoordinate = texcoord;
}
