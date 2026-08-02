#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 modelViewProjection;
    mat4 lightModelViewProjection;
    vec4 cameraPosition;
    vec4 renderingParameters;
    vec4 showcaseParameters;
} camera;

layout(std430, binding = 10) readonly buffer JointData {
    mat4 matrices[];
} jointData;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texcoord;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec4 worldTangent;
layout(location = 2) out vec2 textureCoordinate;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec4 shadowPosition;

void main() {
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointIndices.x]
        + jointWeights.y * jointData.matrices[jointIndices.y]
        + jointWeights.z * jointData.matrices[jointIndices.z]
        + jointWeights.w * jointData.matrices[jointIndices.w];
    vec4 skinnedPosition = skinMatrix * vec4(position, 1.0);
    vec3 skinnedNormal = normalize(mat3(skinMatrix) * normal);
    vec3 skinnedTangent = normalize(mat3(skinMatrix) * tangent.xyz);
    gl_Position = camera.modelViewProjection * skinnedPosition;
    gl_Position.xy *= 1.6;
    worldNormal = normalize(mat3(camera.model) * skinnedNormal);
    worldTangent = vec4(
        normalize(mat3(camera.model) * skinnedTangent),
        tangent.w);
    textureCoordinate = texcoord;
    worldPosition = (camera.model * skinnedPosition).xyz;
    shadowPosition = camera.lightModelViewProjection * skinnedPosition;
}
