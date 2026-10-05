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
    vec4 mainLightDirection;
} camera;

layout(std430, binding = 10) readonly buffer JointData {
    mat4 matrices[];
} jointData;

// Per-instance transforms, written by the host in the same order as the
// visible instance list; gl_InstanceIndex selects the slot.
struct InstanceTransforms {
    mat4 model;
    mat4 modelViewProjection;
    mat4 cascadeLightModelViewProjection[4];
    uvec4 meta;
    vec4 faceLight;
    vec4 morph;
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
layout(location = 8) in float browMask;
layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec4 worldTangent;
layout(location = 2) out vec2 textureCoordinate;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec4 shadowPositions[4];
layout(location = 8) out float eyebrowRegion;
layout(location = 9) flat out vec4 instanceFaceDirection;

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

#if defined(AZURE_COMPUTE_SKINNING)
layout(std430, binding = 17) readonly buffer InstanceSkinnedVertices { uint words[]; } computed;
float posedFloat(uint offset) { return uintBitsToFloat(computed.words[(instanceData.instances[gl_InstanceIndex].meta.y + uint(gl_VertexIndex))*27u+offset]); }
vec3 posedVector(uint offset) { return vec3(posedFloat(offset),posedFloat(offset+1u),posedFloat(offset+2u)); }
#endif
void main() {
    eyebrowRegion = browMask;
    instanceFaceDirection = instanceData.instances[gl_InstanceIndex].faceLight;
    bool browOverlay =
        (morphWeights.materialFeatures & 64U) != 0U;
#if defined(AZURE_COMPUTE_SKINNING)
    vec4 skinnedPosition = vec4(posedVector(0u), 1.0);
    vec3 skinnedNormal = normalize(posedVector(3u));
    vec3 skinnedTangent = normalize(posedVector(6u));

#else
    const uint jointBase = instanceData.instances[gl_InstanceIndex].meta.x;
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointBase + jointIndices.x]
        + jointWeights.y * jointData.matrices[jointBase + jointIndices.y]
        + jointWeights.z * jointData.matrices[jointBase + jointIndices.z]
        + jointWeights.w * jointData.matrices[jointBase + jointIndices.w];
    vec3 morphedPosition = position + morph0 * instanceData.instances[gl_InstanceIndex].morph.x
        + morph1 * instanceData.instances[gl_InstanceIndex].morph.y;

    vec4 skinnedPosition = skinMatrix * vec4(morphedPosition, 1.0);
    vec3 skinnedNormal = normalize(mat3(skinMatrix) * normal);
    vec3 skinnedTangent = normalize(mat3(skinMatrix) * tangent.xyz);
#endif
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
    vec3 gizmoNormal = normalize(transpose(inverse(mat3(morphWeights.gizmoTransform))) * skinnedNormal);
    vec3 gizmoTangent = normalize(mat3(morphWeights.gizmoTransform) * skinnedTangent);
    gl_Position = instanceData.instances[gl_InstanceIndex].modelViewProjection * gizmoPosition;
    mat3 modelLinear = mat3(instanceData.instances[gl_InstanceIndex].model);
    worldNormal = normalize(transpose(inverse(modelLinear)) * gizmoNormal);
    worldTangent = vec4(
        normalize(mat3(instanceData.instances[gl_InstanceIndex].model) * gizmoTangent),
        tangent.w * sign(determinant(modelLinear) * determinant(mat3(morphWeights.gizmoTransform))));
    textureCoordinate = texcoord;
    worldPosition = (instanceData.instances[gl_InstanceIndex].model * gizmoPosition).xyz;
    for (int cascadeIndex = 0; cascadeIndex < 4; ++cascadeIndex) {
        shadowPositions[cascadeIndex] =
            instanceData.instances[gl_InstanceIndex]
                .cascadeLightModelViewProjection[cascadeIndex] * gizmoPosition;
    }
}
