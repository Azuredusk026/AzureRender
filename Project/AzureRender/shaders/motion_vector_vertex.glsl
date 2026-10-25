#include "motion_vector_push.glsl"
#define AZURE_MORPH_DATA_BINDING 3
#define AZURE_MORPH_WEIGHT_BINDING 2
#include "morph_data.glsl"
#include "motion_pose_data.glsl"
#include "instance_data.glsl"
#include "material_sampling.glsl"
layout(std430,binding=0) readonly buffer CurrentInstances {InstanceTransforms values[];} current;
layout(std430,binding=1) readonly buffer CurrentJoints {mat4 values[];} joints;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 tangent;
layout(location=3) in vec2 texcoord;
layout(location=4) in uvec4 jointIndices;
layout(location=5) in vec4 jointWeights;
layout(location=6) in vec3 morph0;
layout(location=7) in vec3 morph1;
layout(location=8) in float browMask;
layout(location=0) out vec2 textureCoordinate;
layout(location=1) out vec3 opacityCoordinate;
layout(location=2) out vec4 currentClip;
layout(location=3) out vec4 previousClip;
layout(location=4) out float previousValid;
layout(location=5) out vec3 materialFrameNormal;
layout(location=6) out vec3 materialFrameTangent;
#if defined(AZURE_MOTION_OUTLINE)
layout(std430,binding=6) readonly buffer OutlineWords {uint words[];} shell;
layout(binding=7) uniform sampler2D widthMask;
vec4 shellVector(uint i){return vec4(uintBitsToFloat(shell.words[i]),uintBitsToFloat(shell.words[i+1u]),uintBitsToFloat(shell.words[i+2u]),uintBitsToFloat(shell.words[i+3u]));}
#endif
void main(){
    uint index=uint(gl_InstanceIndex),vertex=uint(gl_VertexIndex);
    InstanceTransforms instance=current.values[index];
    vec3 p=position,n=normal,t=tangent.xyz;
    if(!applyAssetMorph(instance.meta.z,vertex,instance.meta.w,p,n,t))p+=morph0*instance.morph.x+morph1*instance.morph.y;
    mat4 skin=mat4(0);
    for(uint i=0u;i<4u;++i)if(jointWeights[i]!=0)skin+=jointWeights[i]*joints.values[instance.meta.x+jointIndices[i]];
    vec4 shaped=azureSkinPosition(skin,p);
    vec4 local=motion.gizmo*shaped;
    materialFrameNormal=vec3(0);materialFrameTangent=vec3(0);
    if((motion.flags&512u)==0u&&(floatBitsToUint(motion.detail.y)&azurePixelIntegralFeature)!=0u){
        vec3 localNormal=normalize(transpose(inverse(mat3(motion.gizmo)))*normalize(mat3(skin)*n));
        materialFrameNormal=normalize(transpose(inverse(mat3(instance.model)))*localNormal);
        materialFrameTangent=normalize(mat3(instance.model)*mat3(motion.gizmo)*normalize(mat3(skin)*t));
    }
    vec4 raster;
    bool valid;
#if defined(AZURE_MOTION_OUTLINE)
    local=vec4((motion.gizmo*vec4(shaped.xyz,1)).xyz,1);
    raster=instance.modelViewProjection*local;
    vec3 localNormal=normalize(transpose(inverse(mat3(motion.gizmo)))*normalize(mat3(skin)*n));
    vec3 worldNormal=normalize(transpose(inverse(mat3(instance.model)))*localNormal);
    vec3 worldTangent=normalize(mat3(instance.model)*mat3(motion.gizmo)*normalize(mat3(skin)*t));
    uint base=4u+16u*motion.material;
    bool profile=(motion.flags&256u)!=0u&&uint(shell.words.length())>=base+16u;
    vec4 parameters=profile?shellVector(base):vec4(0);
    profile=profile&&parameters.w>.5;
    vec2 mask=vec2(1);vec3 smoothDirection=vec3(0,0,1);
    if(profile){
        if(shell.words[base+4u]!=0u){uint uv=shell.words[base+6u]+2u*vertex;
            smoothDirection=outlineTangentDirection(vec2(uintBitsToFloat(shell.words[uv]),uintBitsToFloat(shell.words[uv+1u])),shell.words[base+4u]);}
        float handed=tangent.w*sign(determinant(mat3(instance.model))*determinant(mat3(motion.gizmo)));
        worldNormal=worldTangent*smoothDirection.x+cross(worldNormal,worldTangent)*handed*smoothDirection.y+worldNormal*smoothDirection.z;
        mat3 modelInverse=inverse(mat3(instance.model));mat4 mvp=instance.modelViewProjection;
        vec3 rowX=vec3(mvp[0].x,mvp[1].x,mvp[2].x)*modelInverse;
        vec3 rowY=vec3(mvp[0].y,mvp[1].y,mvp[2].y)*modelInverse;
        vec3 rowZ=vec3(mvp[0].z,mvp[1].z,mvp[2].z)*modelInverse;
        vec3 rowW=vec3(mvp[0].w,mvp[1].w,mvp[2].w)*modelInverse;
        vec3 forward=normalize(dot(rowW,rowW)>1e-12?rowW:cross(rowX,rowY));
        vec3 cx=rowX-forward*dot(rowX,forward),cy=rowY-forward*dot(rowY,forward);
        vec4 transform=shellVector(base+8u);mask=textureLod(widthMask,texcoord*transform.xy+transform.zw,0).rg;
        raster.xy+=outlineViewScaledOffset(raster.w,vec2(dot(cx,worldNormal),dot(cy,worldNormal)),motion.outline.yz,length(cy),parameters.x,parameters.y,mask.x);
        raster.z=outlineShiftedClipDepth(raster,vec2(dot(rowZ,forward),dot(rowW,forward)),parameters.z*mask.y);
    }else{
        vec4 shifted=instance.modelViewProjection*vec4(local.xyz+localNormal*.001,1);
        vec2 direction=(shifted.xy/max(shifted.w,.0001)-raster.xy/max(raster.w,.0001))*motion.outline.yz;
        float magnitude=length(direction);if(magnitude>.00001)raster.xy+=direction/magnitude*motion.outline.x*2.0/motion.outline.yz*raster.w;
    }
    valid=azurePreviousMotionOutline(index,vertex,instance.meta.z,motion.primitive,position,morph0,morph1,normal,tangent,
        jointIndices,jointWeights,smoothDirection,profile,parameters,mask,motion.outline.yz,previousClip);
#else
    bool brow=(floatBitsToUint(motion.detail.y)&64u)!=0u&&browMask>.5;
    if(brow){vec3 world=(instance.model*local).xyz;
        local.xyz+=normalize(transpose(mat3(instance.model))*normalize(motion.camera.xyz-world))*motion.detail.x;}
    raster=instance.modelViewProjection*local;
    valid=azurePreviousMotionClip(index,vertex,instance.meta.z,motion.primitive,position,morph0,morph1,jointIndices,jointWeights,brow,motion.detail.x,previousClip);
#endif
    currentClip=raster;currentClip.xy-=motion.detail.zw*raster.w;
    previousValid=valid?1:0;gl_Position=raster;textureCoordinate=texcoord;opacityCoordinate=position;
}
