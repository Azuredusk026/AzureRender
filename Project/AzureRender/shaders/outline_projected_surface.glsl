#include "outline_projection.glsl"
#include "outline_projection_data.glsl"

vec4 projectMaterialSurface(vec4 clip,vec3 normal,vec4 tangentFrame) {
    uint base=4u+16u*(morphWeights.packedMaterial>>1u);
    vec4 parameters=outlineRecordVector(base);
    if(parameters.w<1.5)return clip;
    mat4 model=instanceData.instances[gl_InstanceIndex].model;
    mat4 mvp=instanceData.instances[gl_InstanceIndex].modelViewProjection;
    if(abs(determinant(mat3(model)))<1e-12||abs(determinant(mat3(morphWeights.gizmoTransform)))<1e-12)return clip;
    mat3 modelInverse=inverse(mat3(model));
    if(outlineData.words[base+4u]!=0u) {
        uint coordinate=outlineData.words[base+6u]+2u*uint(gl_VertexIndex);
        vec2 xy=vec2(uintBitsToFloat(outlineData.words[coordinate]),uintBitsToFloat(outlineData.words[coordinate+1u]));
        vec3 direction=outlineTangentDirection(xy,outlineData.words[base+4u]);
        float handedness=tangentFrame.w;
#if defined(AZURE_COMPUTE_SKINNING)
        handedness=posedFloat(9u)*sign(determinant(mat3(model))*determinant(mat3(morphWeights.gizmoTransform)));
#endif
        normal=tangentFrame.xyz*direction.x+cross(normal,tangentFrame.xyz)*handedness*direction.y+normal*direction.z;
    }
    vec3 forward=camera.cameraForward.xyz;
    vec3 rowX=vec3(mvp[0].x,mvp[1].x,mvp[2].x)*modelInverse;
    vec3 rowY=vec3(mvp[0].y,mvp[1].y,mvp[2].y)*modelInverse;
    vec3 centeredX=rowX-forward*dot(rowX,forward),centeredY=rowY-forward*dot(rowY,forward);
    vec4 transform=outlineRecordVector(base+8u);
    vec2 mask=textureLod(PROJECTION_MASK,texcoord*transform.xy+transform.zw,0).rg;
    clip.xy+=outlineViewScaledOffset(clip.w,vec2(dot(centeredX,normal),dot(centeredY,normal)),
        camera.clusterDepth.zw,length(centeredY),parameters.x,parameters.y*max(morphWeights.samplingScale,1.0),mask.r);
    vec3 rowZ=vec3(mvp[0].z,mvp[1].z,mvp[2].z)*modelInverse;
    vec3 rowW=vec3(mvp[0].w,mvp[1].w,mvp[2].w)*modelInverse;
    vec3 cameraInModel=modelInverse*(camera.cameraPosition.xyz-model[3].xyz);
    vec4 cameraClip=mvp*vec4(cameraInModel,1);
    clip.z=outlineShiftedClipDepth(clip,vec2(dot(rowZ,forward),dot(rowW,forward)),parameters.z*mask.g);
    return clip;
}
