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
    vec4 faceLight;
    vec4 morph;
};
layout(std430, binding = 13) readonly buffer InstanceData {
    InstanceTransforms instances[];
} instanceData;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 6) in vec3 morph0;
layout(location = 7) in vec3 morph1;

layout(push_constant) uniform MorphWeights {
    layout(offset = 128) vec2 weights;
} morphWeights;

#if defined(AZURE_COMPUTE_SKINNING)
layout(std430, binding = 17) readonly buffer InstanceSkinnedVertices { uint words[]; } computed;
float posedFloat(uint offset) { return uintBitsToFloat(computed.words[(instanceData.instances[gl_InstanceIndex].meta.y + uint(gl_VertexIndex))*27u+offset]); }
vec3 posedVector(uint offset) { return vec3(posedFloat(offset),posedFloat(offset+1u),posedFloat(offset+2u)); }
#endif
void main() {
#if defined(AZURE_COMPUTE_SKINNING)
    vec3 skinnedPosition = posedVector(0u);
    vec3 skinnedNormal = normalize(posedVector(3u));
#else
    const uint jointBase = instanceData.instances[gl_InstanceIndex].meta.x;
    vec3 morphedPosition = position
        + morph0 * instanceData.instances[gl_InstanceIndex].morph.x
        + morph1 * instanceData.instances[gl_InstanceIndex].morph.y;
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointBase + jointIndices.x]
        + jointWeights.y * jointData.matrices[jointBase + jointIndices.y]
        + jointWeights.z * jointData.matrices[jointBase + jointIndices.z]
        + jointWeights.w * jointData.matrices[jointBase + jointIndices.w];
    vec3 skinnedPosition =
        (skinMatrix * vec4(morphedPosition, 1.0)).xyz;
    vec3 skinnedNormal =
        normalize(mat3(skinMatrix) * normal);
#endif
    mat4 mvp=instanceData.instances[gl_InstanceIndex].modelViewProjection;
    vec4 clip=mvp*vec4(skinnedPosition,1);
    vec4 shifted=mvp*vec4(skinnedPosition+skinnedNormal*.001,1);
    vec2 projected=shifted.xy/max(shifted.w,.0001)-clip.xy/max(clip.w,.0001);
    vec2 pixelDirection=projected*camera.clusterDepth.zw;
    float magnitude=length(pixelDirection);
    if(magnitude>.00001)clip.xy+=pixelDirection/magnitude * camera.renderingParameters.x*2.0/camera.clusterDepth.zw*clip.w;
    gl_Position=clip;
}
