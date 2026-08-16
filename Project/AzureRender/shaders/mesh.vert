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
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texcoord;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 6) in vec3 morph0;
layout(location = 7) in vec3 morph1;
layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec4 worldTangent;
layout(location = 2) out vec2 textureCoordinate;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec4 shadowPosition;

// Morph blend weights + per-primitive gizmo transform (driven by push
// constants from RenderSettings). weights occupies bytes 0..7; the gizmo
// transform is a std140 mat4 starting at byte 16.
layout(push_constant) uniform MorphWeights {
    vec2 weights;
    mat4 gizmoTransform;
} morphWeights;

void main() {
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointIndices.x]
        + jointWeights.y * jointData.matrices[jointIndices.y]
        + jointWeights.z * jointData.matrices[jointIndices.z]
        + jointWeights.w * jointData.matrices[jointIndices.w];
    vec3 morphedPosition = position + morph0 * morphWeights.weights.x
        + morph1 * morphWeights.weights.y;
    vec4 skinnedPosition = skinMatrix * vec4(morphedPosition, 1.0);
    vec4 gizmoPosition = morphWeights.gizmoTransform * skinnedPosition;
    vec3 skinnedNormal = normalize(mat3(skinMatrix) * normal);
    vec3 gizmoNormal = normalize(mat3(morphWeights.gizmoTransform) * skinnedNormal);
    vec3 gizmoTangent = normalize(mat3(morphWeights.gizmoTransform) * tangent.xyz);
    gl_Position = camera.modelViewProjection * gizmoPosition;
    gl_Position.xy *= 1.6;
    worldNormal = normalize(mat3(camera.model) * gizmoNormal);
    worldTangent = vec4(
        normalize(mat3(camera.model) * gizmoTangent),
        tangent.w);
    textureCoordinate = texcoord;
    worldPosition = (camera.model * gizmoPosition).xyz;
    shadowPosition = camera.lightModelViewProjection * gizmoPosition;
}
