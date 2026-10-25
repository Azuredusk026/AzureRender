#include "outline_projection.glsl"
#include "outline_projection_data.glsl"
layout(location=2) in vec4 tangent;
layout(location=3) in vec2 texcoord;
layout(location=1) out vec2 textureCoordinate;
layout(location=2) flat out vec4 shellColor;
layout(location=3) flat out vec4 shellDepthProjection;

void main() {
    opacityCoordinate=position;
    textureCoordinate=texcoord;
    uint base=4u+16u*morphWeights.materialIndex;
    vec4 parameters=outlineRecordVector(base);
    shellColor=parameters.w>.5?outlineRecordVector(base+12u):vec4(.018,.028,.042,1);
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
    vec4 clip=mvp*vec4(localPosition,1);
    shellDepthProjection=vec4(1,0,1,0);
    if(abs(determinant(mat3(model)))<1e-12||abs(determinant(mat3(morphWeights.gizmoTransform)))<1e-12) {
        gl_Position=clip;return;
    }
    vec3 localNormal=normalize(transpose(inverse(mat3(morphWeights.gizmoTransform)))*skinnedNormal);
    if(parameters.w<.5) {
        vec4 shifted=mvp*vec4(localPosition+localNormal*.001,1);
        vec2 direction=(shifted.xy/max(shifted.w,.0001)-clip.xy/max(clip.w,.0001))*camera.clusterDepth.zw;
        float magnitude=length(direction);
        if(magnitude>.00001)clip.xy+=direction/magnitude*camera.renderingParameters.x*2.0/camera.clusterDepth.zw*clip.w;
        gl_Position=clip;return;
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
}
