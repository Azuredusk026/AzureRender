#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    mat4 lightModelViewProjection;
    vec4 cameraPosition;
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

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;

void main() {
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointIndices.x]
        + jointWeights.y * jointData.matrices[jointIndices.y]
        + jointWeights.z * jointData.matrices[jointIndices.z]
        + jointWeights.w * jointData.matrices[jointIndices.w];
    vec3 skinnedPosition =
        (skinMatrix * vec4(position, 1.0)).xyz;
    vec3 skinnedNormal =
        normalize(mat3(skinMatrix) * normal);
    vec3 expandedPosition =
        skinnedPosition
        + skinnedNormal * camera.renderingParameters.x;
    gl_Position =
        camera.modelViewProjection * vec4(expandedPosition, 1.0);
    gl_Position.xy *= 1.6;
}
