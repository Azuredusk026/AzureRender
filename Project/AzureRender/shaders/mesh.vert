#version 450

layout(binding = 0) uniform CameraData {
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

// Per-instance transforms, written by the host in the same order as the
// visible instance list; gl_InstanceIndex selects the slot.
struct InstanceTransforms {
    mat4 model;
    mat4 modelViewProjection;
    mat4 lightModelViewProjection;
    uvec4 meta;
};
layout(std430, binding = 13) readonly buffer InstanceData {
    InstanceTransforms instances[];
} instanceData;

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
// constants from RenderSettings). The vertex push range starts at byte 128
// (after MaterialPushConstants); weights occupies 128..135, the std140 mat4
// gizmoTransform starts at byte 144 (16-byte aligned).
layout(push_constant) uniform MorphWeights {
    layout(offset = 80) vec4 styleParameters;
    layout(offset = 96) vec4 featureParameters;
    layout(offset = 116) uint materialFeatures;
    layout(offset = 128) vec2 weights;
    layout(offset = 144) mat4 gizmoTransform;
} morphWeights;

void main() {
    bool browOverlay =
        (morphWeights.materialFeatures & 64U) != 0U;
    bool eyebrowVertex = browOverlay && (
        (jointWeights.x > 0.001
            && jointIndices.x >= 112U && jointIndices.x <= 154U)
        || (jointWeights.y > 0.001
            && jointIndices.y >= 112U && jointIndices.y <= 154U)
        || (jointWeights.z > 0.001
            && jointIndices.z >= 112U && jointIndices.z <= 154U)
        || (jointWeights.w > 0.001
            && jointIndices.w >= 112U && jointIndices.w <= 154U));
    const uint jointBase = instanceData.instances[gl_InstanceIndex].meta.x;
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointBase + jointIndices.x]
        + jointWeights.y * jointData.matrices[jointBase + jointIndices.y]
        + jointWeights.z * jointData.matrices[jointBase + jointIndices.z]
        + jointWeights.w * jointData.matrices[jointBase + jointIndices.w];
    vec3 morphedPosition = position + morph0 * morphWeights.weights.x
        + morph1 * morphWeights.weights.y;
    if (eyebrowVertex) {
        // This exported primitive combines four brow islands with thirty eye
        // and eyelash islands. Brow joints occupy 112..154, so compensate for
        // the sub-pixel card height without altering the eyelashes.
        float browThickness = min(morphWeights.styleParameters.x, 0.0012);
        float islandCenterY = position.y > 1.2617 ? 1.26704 : 1.25846;
        float islandCenterX = sign(position.x) * 0.0365;
        morphedPosition.y += sign(position.y - islandCenterY) * browThickness;
        morphedPosition.x += sign(position.x - islandCenterX)
            * browThickness * 0.45;
    }
    vec4 skinnedPosition = skinMatrix * vec4(morphedPosition, 1.0);
    vec4 gizmoPosition = morphWeights.gizmoTransform * skinnedPosition;
    if (browOverlay) {
        vec3 initialWorldPosition = (instanceData.instances[gl_InstanceIndex].model * gizmoPosition).xyz;
        vec3 worldViewDirection = normalize(
            camera.cameraPosition.xyz - initialWorldPosition);
        vec3 localViewDirection = normalize(
            transpose(mat3(instanceData.instances[gl_InstanceIndex].model)) * worldViewDirection);
        // M_Common_Brow Offset, converted from Unreal cm to glTF metres by
        // the asset profile.
        gizmoPosition.xyz += localViewDirection
            * morphWeights.featureParameters.x;
    }
    vec3 skinnedNormal = normalize(mat3(skinMatrix) * normal);
    vec3 gizmoNormal = normalize(mat3(morphWeights.gizmoTransform) * skinnedNormal);
    vec3 gizmoTangent = normalize(mat3(morphWeights.gizmoTransform) * tangent.xyz);
    gl_Position = instanceData.instances[gl_InstanceIndex].modelViewProjection * gizmoPosition;
    gl_Position.xy *= 1.6;
    worldNormal = normalize(mat3(instanceData.instances[gl_InstanceIndex].model) * gizmoNormal);
    worldTangent = vec4(
        normalize(mat3(instanceData.instances[gl_InstanceIndex].model) * gizmoTangent),
        tangent.w);
    textureCoordinate = texcoord;
    worldPosition = (instanceData.instances[gl_InstanceIndex].model * gizmoPosition).xyz;
    shadowPosition = instanceData.instances[gl_InstanceIndex].lightModelViewProjection * gizmoPosition;
}
