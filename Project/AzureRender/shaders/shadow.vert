#version 450
#extension GL_GOOGLE_include_directive : require
#include "morph_data.glsl"
#include "skin_position.glsl"
#include "reference_position_frame.glsl"
#include "reference_clip_projection.glsl"

layout(constant_id=8) const bool azureReferenceObjectShadowRaster=false;

struct ReferenceObjectShadowRaster {
    vec4 columns[4];
    vec4 jitter;
    uvec4 control;
    vec4 offset;
    vec4 absoluteOrigin;
};

layout(std140,binding = 0) uniform CameraData {
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
    // The extension starts after the 800-byte scene camera block.
    layout(offset=800) ReferenceObjectShadowRaster objectShadows[8];
} camera;

layout(std430, binding = 10) readonly buffer JointData {
    mat4 matrices[];
} jointData;

#include "instance_data.glsl"
layout(std430, binding = 13) readonly buffer InstanceData {
    InstanceTransforms instances[];
} instanceData;

layout(push_constant) uniform ShadowParameters {
    layout(offset = 128) vec2 morphWeights;
    layout(offset = 144) mat4 gizmoTransform;
    layout(offset = 208) uint cascadeIndex;
} shadowParameters;

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;
layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 6) in vec3 morph0;
layout(location = 7) in vec3 morph1;
layout(location = 0) out vec2 textureCoordinate;
layout(location = 1) out vec3 opacityCoordinate;

#if defined(AZURE_COMPUTE_SKINNING)
#include "skinned_vertex_layout.glsl"
layout(std430, binding = 17) readonly buffer InstanceSkinnedVertices { uint words[]; } computed;
float posedFloat(uint offset) { return uintBitsToFloat(computed.words[(instanceData.instances[gl_InstanceIndex].meta.y + uint(gl_VertexIndex))*azureSkinnedVertexWordCount+offset]); }
vec3 posedVector(uint offset) { return vec3(posedFloat(offset),posedFloat(offset+1u),posedFloat(offset+2u)); }
#endif
void main() {
    opacityCoordinate=position;
#if defined(AZURE_COMPUTE_SKINNING)
    vec4 skinnedPosition = vec4(posedVector(0u), 1.0);
#else
    const uint jointBase = instanceData.instances[gl_InstanceIndex].meta.x;
    vec3 morphedPosition=position,unusedNormal=vec3(0),unusedTangent=vec3(0);
    if(!applyAssetMorph(instanceData.instances[gl_InstanceIndex].meta.z,uint(gl_VertexIndex),
        instanceData.instances[gl_InstanceIndex].meta.w,morphedPosition,unusedNormal,unusedTangent))
        morphedPosition+=morph0*instanceData.instances[gl_InstanceIndex].morph.x+morph1*instanceData.instances[gl_InstanceIndex].morph.y;
    mat4 skinMatrix =
        jointWeights.x * jointData.matrices[jointBase + jointIndices.x]
        + jointWeights.y * jointData.matrices[jointBase + jointIndices.y]
        + jointWeights.z * jointData.matrices[jointBase + jointIndices.z]
        + jointWeights.w * jointData.matrices[jointBase + jointIndices.w];
    vec4 skinnedPosition = azureSkinPosition(skinMatrix,morphedPosition);
#endif
    if(azureReferenceObjectShadowRaster&&shadowParameters.cascadeIndex>=5u){
        ReferenceObjectShadowRaster raster=camera.objectShadows[shadowParameters.cascadeIndex-5u];
        vec3 world=(shadowParameters.gizmoTransform*skinnedPosition).xyz;
        vec3 relative=azureReferenceRelativePosition(world,raster.control.y,raster.offset.xyz);
        precise vec3 absolute=relative+raster.absoluteOrigin.xyz;
        precise vec4 clip=azureReferenceClipProjection(absolute,raster.columns,raster.jitter.xy);
        gl_Position=vec4(clip.x,-clip.y,clip.w-clip.z,clip.w);
    }else{
        gl_Position=shadowParameters.cascadeIndex==4u
            ?shadowParameters.gizmoTransform*skinnedPosition
            :instanceData.instances[gl_InstanceIndex].cascadeLightModelViewProjection[shadowParameters.cascadeIndex]
                *shadowParameters.gizmoTransform*skinnedPosition;
    }
    textureCoordinate = texcoord;
}
