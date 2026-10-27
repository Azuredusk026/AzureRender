#version 450
#extension GL_GOOGLE_include_directive : require
#include "morph_data.glsl"
#include "skin_position.glsl"

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
    vec4 cascadeDepthRanges;
} camera;

layout(std430, binding = 10) readonly buffer JointData {
    mat4 matrices[];
} jointData;

// Per-instance transforms, written by the host in the same order as the
// visible instance list; gl_InstanceIndex selects the slot.
#include "instance_data.glsl"
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
#if !defined(AZURE_OUTLINE_MASK) && !defined(AZURE_SNAPSHOT_DEPTH)
layout(location=9) in vec4 vertexColor;
layout(location=8,component=1) out vec3 vertexColorRgb;
layout(location=0,component=3) out float vertexColorAlpha;
#endif
#if defined(AZURE_OUTLINE_MASK) || defined(AZURE_SNAPSHOT_DEPTH)
#define AZURE_VERTEX_OUTPUT(locationIndex)
#else
#define AZURE_VERTEX_OUTPUT(locationIndex) layout(location = locationIndex) out
#endif
AZURE_VERTEX_OUTPUT(0) vec3 worldNormal;
AZURE_VERTEX_OUTPUT(1) vec4 worldTangent;
layout(location = 2) out vec2 textureCoordinate;
#if defined(AZURE_SNAPSHOT_DEPTH)
layout(location=3) out vec3 worldPosition;
#else
AZURE_VERTEX_OUTPUT(3) vec3 worldPosition;
#endif
AZURE_VERTEX_OUTPUT(4) vec4 shadowPositions[4];
AZURE_VERTEX_OUTPUT(8) float eyebrowRegion;
#if defined(AZURE_OUTLINE_MASK) || defined(AZURE_SNAPSHOT_DEPTH)
vec4 instanceFaceDirection;
mat4 instanceFaceFrame;
#else
layout(location = 9) flat out vec4 instanceFaceDirection;
layout(location = 10) flat out mat4 instanceFaceFrame;
#endif
#if defined(AZURE_SNAPSHOT_DEPTH)
layout(location=14) out vec3 opacityCoordinate;
#else
AZURE_VERTEX_OUTPUT(14) vec3 opacityCoordinate;
#endif
#if defined(AZURE_OBJECT_SHADOW)
layout(location=15) flat out uint instanceObjectShadow;
#endif

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
#if !defined(AZURE_OUTLINE_MASK) && !defined(AZURE_SNAPSHOT_DEPTH)
    vertexColorRgb=vertexColor.rgb;vertexColorAlpha=vertexColor.a;
#endif
#if defined(AZURE_OBJECT_SHADOW)
    instanceObjectShadow=instanceData.instances[gl_InstanceIndex].objectShadowIndex;
#endif
    opacityCoordinate=position;
    eyebrowRegion = browMask;
    instanceFaceDirection = instanceData.instances[gl_InstanceIndex].faceLight;
    instanceFaceFrame=instanceData.instances[gl_InstanceIndex].faceFrame;
    if((morphWeights.materialFeatures & 65536U)!=0U && (morphWeights.materialFeatures & 1048576U)==0U) {
        // The projected hair frame follows the source object and gizmo.
        instanceFaceFrame=instanceData.instances[gl_InstanceIndex].model*morphWeights.gizmoTransform;
    } else if((morphWeights.materialFeatures & (2048U|1048576U))!=0U
        ||(instanceData.instances[gl_InstanceIndex].instanceFlags&AZURE_INSTANCE_MATERIAL_RIM_ORIGIN)!=0U) {
        mat4 model=instanceData.instances[gl_InstanceIndex].model;
        instanceFaceFrame=model*morphWeights.gizmoTransform*inverse(model)*instanceFaceFrame;
        for(int axis=0;axis<3;++axis)instanceFaceFrame[axis].xyz=normalize(instanceFaceFrame[axis].xyz);
    }
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
    vec3 morphedPosition=position,morphedNormal=normal,morphedTangent=tangent.xyz;
    if(!applyAssetMorph(instanceData.instances[gl_InstanceIndex].meta.z,uint(gl_VertexIndex),
        instanceData.instances[gl_InstanceIndex].meta.w,morphedPosition,morphedNormal,morphedTangent))
        morphedPosition+=morph0*instanceData.instances[gl_InstanceIndex].morph.x+morph1*instanceData.instances[gl_InstanceIndex].morph.y;

    vec4 skinnedPosition = azureSkinPosition(skinMatrix,morphedPosition);
    vec3 skinnedNormal = normalize(mat3(skinMatrix) * morphedNormal);
    vec3 skinnedTangent = normalize(mat3(skinMatrix) * morphedTangent);
#endif
    vec4 gizmoPosition = morphWeights.gizmoTransform * skinnedPosition;
    if (browOverlay && browMask > 0.5) {
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
