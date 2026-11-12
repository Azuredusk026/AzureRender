#include "outline_projection.glsl"
#include "outline_projection_data.glsl"
layout(location=2) in vec4 tangent;
layout(location=3) in vec2 texcoord;
#if defined(AZURE_REFERENCE_SCENE_OUTLINE_DEPTH)
layout(location=2) out vec2 textureCoordinate;
layout(location=3) out vec3 worldPosition;
vec4 shellDepthProjection;

vec4 outlineReferenceScenePosition(vec3 world,vec4 deviceBase,vec4 deviceFinal,float depthOffset){
    uint axes=camera.referenceControl.y;
    uint flags=camera.referenceControl.z;
    if((flags&1u)==0u)return deviceFinal;
    precise vec4 clip=azureReferenceClipProjection(
        azureReferenceRelativePosition(world,axes,camera.referenceOffset.xyz),camera.referenceColumns,camera.referenceJitter.xy);
    if((flags&2u)!=0u)clip.y=-clip.y;
    if(deviceBase.w!=0.0&&deviceFinal.w!=0.0&&any(notEqual(deviceBase.xyw,deviceFinal.xyw))){
        precise vec2 finalNdc=deviceFinal.xy/deviceFinal.w;
        precise vec2 baseNdc=deviceBase.xy/deviceBase.w;
        precise vec2 offset=(finalNdc-baseNdc)*clip.w;
        clip.xy=clip.xy+offset;
    }
    clip.y=-clip.y;
    if(depthOffset!=0.0){
        vec3 forward=azureReferenceRelativePosition(camera.cameraForward.xyz,axes,vec3(0));
        vec3 rowZ=vec3(camera.referenceColumns[0].z,camera.referenceColumns[1].z,camera.referenceColumns[2].z);
        vec3 rowW=vec3(camera.referenceColumns[0].w,camera.referenceColumns[1].w,camera.referenceColumns[2].w);
        clip.z=outlineShiftedClipDepth(clip,vec2(dot(rowZ,forward),dot(rowW,forward)),depthOffset);
    }
    return clip;
}
#else
layout(location=1) out vec2 textureCoordinate;
layout(location=2) flat out vec4 shellColor;
layout(location=3) flat out vec4 shellDepthProjection;
#endif

void main() {
    opacityCoordinate=position;
    textureCoordinate=texcoord;
    uint base=4u+16u*morphWeights.materialIndex;
    vec4 parameters=outlineRecordVector(base);
#if !defined(AZURE_REFERENCE_SCENE_OUTLINE_DEPTH)
    shellColor=parameters.w>.5?outlineRecordVector(base+12u):vec4(.018,.028,.042,1);
#endif
#if defined(AZURE_COMPUTE_SKINNING)
    vec3 skinnedPosition=posedVector(0u),skinnedNormal=normalize(posedVector(3u));
    vec3 skinnedTangent=normalize(posedVector(6u));
    float tangentSign=posedFloat(9u);
#else
    uint jointBase=instanceData.instances[gl_InstanceIndex].meta.x;
    vec3 p=position,n=normal,t=tangent.xyz;
    if(!applyAssetMorph(instanceData.instances[gl_InstanceIndex].meta.z,uint(gl_VertexIndex),
        instanceData.instances[gl_InstanceIndex].meta.w,p,n,t))
        p+=morph0*instanceData.instances[gl_InstanceIndex].morph.x+morph1*instanceData.instances[gl_InstanceIndex].morph.y;
    mat4 skin=jointWeights.x*jointData.matrices[jointBase+jointIndices.x]
        +jointWeights.y*jointData.matrices[jointBase+jointIndices.y]
        +jointWeights.z*jointData.matrices[jointBase+jointIndices.z]
        +jointWeights.w*jointData.matrices[jointBase+jointIndices.w];
    vec3 skinnedPosition=azureSkinPosition(skin,p).xyz;
    vec3 skinnedNormal=normalize(mat3(skin)*n),skinnedTangent=normalize(mat3(skin)*t);
    float tangentSign=tangent.w;
#endif
    mat4 model=instanceData.instances[gl_InstanceIndex].model;
    mat4 mvp=instanceData.instances[gl_InstanceIndex].modelViewProjection;
    vec3 localPosition=(morphWeights.gizmoTransform*vec4(skinnedPosition,1)).xyz;
#if defined(AZURE_REFERENCE_SCENE_OUTLINE_DEPTH)
    worldPosition=(model*vec4(localPosition,1)).xyz;
#endif
    vec4 clip=mvp*vec4(localPosition,1);
#if defined(AZURE_REFERENCE_CAMERA)
    vec4 referenceDeviceBase=clip;
#endif
    shellDepthProjection=vec4(1,0,1,0);
    if(abs(determinant(mat3(model)))<1e-12||abs(determinant(mat3(morphWeights.gizmoTransform)))<1e-12) {
        gl_Position=clip;
#if defined(AZURE_REFERENCE_SCENE_OUTLINE_DEPTH)
        gl_Position=outlineReferenceScenePosition(worldPosition,referenceDeviceBase,clip,0.0);
#elif defined(AZURE_REFERENCE_CAMERA)
        gl_Position=azureCameraRasterPosition((model*vec4(localPosition,1)).xyz,referenceDeviceBase,clip);
#endif
        return;
    }
    vec3 localNormal=normalize(transpose(inverse(mat3(morphWeights.gizmoTransform)))*skinnedNormal);
    if(parameters.w<.5) {
        vec4 shifted=mvp*vec4(localPosition+localNormal*.001,1);
        vec2 direction=(shifted.xy/max(shifted.w,.0001)-clip.xy/max(clip.w,.0001))*camera.clusterDepth.zw;
        float magnitude=length(direction);
        if(magnitude>.00001)clip.xy+=direction/magnitude*camera.renderingParameters.x*2.0/camera.clusterDepth.zw*clip.w;
        gl_Position=clip;
#if defined(AZURE_REFERENCE_SCENE_OUTLINE_DEPTH)
        gl_Position=outlineReferenceScenePosition(worldPosition,referenceDeviceBase,clip,0.0);
#elif defined(AZURE_REFERENCE_CAMERA)
        gl_Position=azureCameraRasterPosition((model*vec4(localPosition,1)).xyz,referenceDeviceBase,clip);
#endif
        return;
    }
    mat3 modelInverse=inverse(mat3(model));
    vec3 worldNormal=normalize(transpose(modelInverse)*localNormal);
    if(outlineData.words[base+4u]!=0u) {
        uint coordinate=outlineData.words[base+6u]+2u*uint(gl_VertexIndex);
        vec2 xy=vec2(uintBitsToFloat(outlineData.words[coordinate]),uintBitsToFloat(outlineData.words[coordinate+1u]));
        vec3 worldTangent=normalize(mat3(model)*mat3(morphWeights.gizmoTransform)*skinnedTangent);
        float handedness=tangentSign*sign(determinant(mat3(model))*determinant(mat3(morphWeights.gizmoTransform)));
        vec3 tangentDirection=outlineTangentDirection(xy,outlineData.words[base+4u]);
        worldNormal=worldTangent*tangentDirection.x+cross(worldNormal,worldTangent)*handedness*tangentDirection.y
            +worldNormal*tangentDirection.z;
    }
    vec3 forward=camera.cameraForward.xyz;
    vec3 rowX=vec3(mvp[0].x,mvp[1].x,mvp[2].x)*modelInverse;
    vec3 rowY=vec3(mvp[0].y,mvp[1].y,mvp[2].y)*modelInverse;
    vec3 centeredX=rowX-forward*dot(rowX,forward),centeredY=rowY-forward*dot(rowY,forward);
    vec4 transform=outlineRecordVector(base+8u);
    vec2 mask=textureLod(PROJECTION_MASK,texcoord*transform.xy+transform.zw,0).rg;
    clip.xy+=outlineViewScaledOffset(clip.w,vec2(dot(centeredX,worldNormal),dot(centeredY,worldNormal)),
        camera.clusterDepth.zw,length(centeredY),parameters.x,parameters.y*max(morphWeights.samplingScale,1.0),mask.r);
    vec3 rowZ=vec3(mvp[0].z,mvp[1].z,mvp[2].z)*modelInverse;
    vec3 rowW=vec3(mvp[0].w,mvp[1].w,mvp[2].w)*modelInverse;
    vec3 cameraInModel=modelInverse*(camera.cameraPosition.xyz-model[3].xyz);
    vec4 cameraClip=mvp*vec4(cameraInModel,1);
    shellDepthProjection=vec4(dot(rowZ,forward),cameraClip.z,dot(rowW,forward),cameraClip.w);
    clip.z=outlineShiftedClipDepth(clip,shellDepthProjection.xz,parameters.z*mask.g);
    gl_Position=clip;
#if defined(AZURE_REFERENCE_SCENE_OUTLINE_DEPTH)
    gl_Position=outlineReferenceScenePosition(worldPosition,referenceDeviceBase,clip,parameters.z*mask.g);
#elif defined(AZURE_REFERENCE_CAMERA)
    gl_Position=azureCameraRasterPosition((model*vec4(localPosition,1)).xyz,referenceDeviceBase,clip);
#endif
}
