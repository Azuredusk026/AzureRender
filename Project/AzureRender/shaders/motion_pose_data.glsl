#ifndef AZURE_MOTION_POSE_DATA_GLSL
#define AZURE_MOTION_POSE_DATA_GLSL
#include "motion_projection.glsl"
#include "outline_projection.glsl"
#ifndef AZURE_MOTION_POSE_BINDING
#define AZURE_MOTION_POSE_BINDING 4
#endif
#ifndef AZURE_MOTION_TARGET_BINDING
#define AZURE_MOTION_TARGET_BINDING 3
#endif
layout(std430,set=0,binding=AZURE_MOTION_POSE_BINDING) readonly buffer PreviousMotionWords {uint words[];} previousMotion;
layout(std430,set=0,binding=AZURE_MOTION_TARGET_BINDING) readonly buffer PreviousMotionTargets {uint words[];} motionTargets;
float azureMotionFloat(uint word){return uintBitsToFloat(previousMotion.words[word]);}
vec4 azureMotionVector(uint word){return vec4(azureMotionFloat(word),azureMotionFloat(word+1u),azureMotionFloat(word+2u),azureMotionFloat(word+3u));}
mat4 azureMotionMatrix(uint word){return mat4(azureMotionVector(word),azureMotionVector(word+4u),azureMotionVector(word+8u),azureMotionVector(word+12u));}
vec3 azureMotionTarget(uint word){return vec3(uintBitsToFloat(motionTargets.words[word]),uintBitsToFloat(motionTargets.words[word+1u]),uintBitsToFloat(motionTargets.words[word+2u]));}
// All offsets are uint words. Limits protect every dependent joint/morph load.
bool azureMotionPoseRecord(uint instanceIndex,uint vertexIndex,out uint record,out uvec4 ranges){
    record=0u;ranges=uvec4(0);uint capacity=uint(previousMotion.words.length());
    if(capacity<28u)return false;
    uint total=previousMotion.words[24],count=previousMotion.words[1];
    if(previousMotion.words[0]!=1u||previousMotion.words[2]!=28u||previousMotion.words[3]!=40u
        ||total<28u||total>capacity||count>(total-28u)/40u||instanceIndex>=count)return false;
    uint jointsBegin=previousMotion.words[25],weightsBegin=previousMotion.words[26];
    if(jointsBegin<28u+count*40u||weightsBegin<jointsBegin||weightsBegin>total)return false;
    record=28u+instanceIndex*40u;
    if(previousMotion.words[record+35u]!=1u||vertexIndex>=previousMotion.words[record+34u])return false;
    ranges=uvec4(previousMotion.words[record+32u],previousMotion.words[record+33u],
        previousMotion.words[record+38u],previousMotion.words[record+39u]);
    return ranges.x>=jointsBegin&&ranges.x<=weightsBegin&&ranges.z>0u
        &&(ranges.x-jointsBegin)%16u==0u&&ranges.z<=(weightsBegin-ranges.x)/16u
        &&ranges.y>=weightsBegin&&ranges.y<=total&&ranges.w<=total-ranges.y;
}
bool azureMotionMorphRange(uint vertexIndex,uint morphTableBase,uvec4 ranges,out uvec3 targets){
    targets=uvec3(0);if(morphTableBase==0u)return true;
    uint length=uint(motionTargets.words.length());
    if(morphTableBase>=length||length-morphTableBase<3u||vertexIndex>(length-morphTableBase-3u)/3u)return false;
    uint table=morphTableBase+vertexIndex*3u;
    targets=uvec3(motionTargets.words[table],motionTargets.words[table+1u],motionTargets.words[table+2u]);
    return targets.y<=ranges.w&&targets.z<=ranges.w-targets.y&&targets.x<=length&&targets.z<=(length-targets.x)/9u;
}
bool azureMotionSkin(uvec4 ranges,uvec4 jointIndices,vec4 jointWeights,out mat4 skin){
    skin=mat4(0);
    for(uint influence=0u;influence<4u;++influence){
        float weight=jointWeights[influence];if(weight==0)continue;
        uint joint=jointIndices[influence];if(joint>=ranges.z)return false;
        skin+=weight*azureMotionMatrix(ranges.x+joint*16u);
    }
    return true;
}
mat4 azurePreviousMotionGizmo(uint primitiveIndex){
    uint selected=previousMotion.words[27];
    return selected!=0xffffffffu&&selected==primitiveIndex?azureMotionMatrix(8u):mat4(1);
}
bool azurePreviousMotionLocal(uint instanceIndex,uint vertexIndex,uint morphTableBase,
    vec3 position,vec3 morph0,vec3 morph1,uvec4 jointIndices,vec4 jointWeights,
    out uint record,out vec4 skinned) {
    record=0u;skinned=vec4(0);uvec4 ranges;uvec3 targets;mat4 skin;
    if(!azureMotionPoseRecord(instanceIndex,vertexIndex,record,ranges)
        ||!azureMotionMorphRange(vertexIndex,morphTableBase,ranges,targets)
        ||!azureMotionSkin(ranges,jointIndices,jointWeights,skin))return false;
    vec3 morphed=position;
    if(morphTableBase==0u)morphed+=morph0*azureMotionFloat(record+36u)+morph1*azureMotionFloat(record+37u);
    else for(uint target=0u;target<targets.z;++target){
        float weight=azureMotionFloat(ranges.y+targets.y+target);
        if(weight!=0)morphed+=weight*azureMotionTarget(targets.x+target*9u);
    }
    skinned=skin*vec4(morphed,1);return azureFiniteClip(skinned);
}
bool azurePreviousMotionClip(uint instanceIndex,uint vertexIndex,uint morphTableBase,uint primitiveIndex,
    vec3 position,vec3 morph0,vec3 morph1,uvec4 jointIndices,vec4 jointWeights,
    bool offsetBrow,float browOffset,out vec4 previousClip) {
    previousClip=vec4(0);uint record;vec4 skinned;
    if(!azurePreviousMotionLocal(instanceIndex,vertexIndex,morphTableBase,position,morph0,morph1,jointIndices,jointWeights,record,skinned))return false;
    vec4 shaped=azurePreviousMotionGizmo(primitiveIndex)*skinned;
    if(offsetBrow){
        mat4 model=azureMotionMatrix(record);
        vec3 direction=azureMotionVector(4u).xyz-(model*shaped).xyz;
        float squared=dot(direction,direction);
        if(!(squared>0)||isinf(squared))return false;
        vec3 local=transpose(mat3(model))*(direction*inversesqrt(squared));squared=dot(local,local);
        if(!(squared>0)||isinf(squared))return false;
        shaped.xyz+=local*inversesqrt(squared)*browOffset;
    }
    vec4 clip=azureMotionMatrix(record+16u)*shaped;
    if(!azureFiniteClip(clip))return false;
    previousClip=clip;return true;
}
bool azureMotionNormalize(inout vec3 direction){
    float squared=dot(direction,direction);
    if(!(squared>0)||isinf(squared))return false;
    direction*=inversesqrt(squared);return true;
}
bool azurePreviousMotionDirections(uint instanceIndex,uint vertexIndex,uint morphTableBase,uint primitiveIndex,
    vec3 normal,vec4 tangent,uvec4 jointIndices,vec4 jointWeights,out vec3 worldNormal,out vec4 worldTangent){
    worldNormal=vec3(0);worldTangent=vec4(0);
    uint record;uvec4 ranges;uvec3 targets;mat4 skin;
    if(!azureMotionPoseRecord(instanceIndex,vertexIndex,record,ranges)
        ||!azureMotionMorphRange(vertexIndex,morphTableBase,ranges,targets)
        ||!azureMotionSkin(ranges,jointIndices,jointWeights,skin))return false;
    vec3 n=normal,t=tangent.xyz;
    if(morphTableBase!=0u)for(uint target=0u;target<targets.z;++target){
        float weight=azureMotionFloat(ranges.y+targets.y+target);
        if(weight!=0){n+=weight*azureMotionTarget(targets.x+target*9u+3u);t+=weight*azureMotionTarget(targets.x+target*9u+6u);}
    }
    n=mat3(skin)*n;t=mat3(skin)*t;
    if(!azureMotionNormalize(n)||!azureMotionNormalize(t))return false;
    mat3 model=mat3(azureMotionMatrix(record)),gizmo=mat3(azurePreviousMotionGizmo(primitiveIndex));
    float modelSign=determinant(model),gizmoSign=determinant(gizmo);
    if(abs(modelSign)<1e-12||abs(gizmoSign)<1e-12)return false;
    n=transpose(inverse(gizmo))*n;if(!azureMotionNormalize(n))return false;
    n=transpose(inverse(model))*n;t=model*gizmo*t;
    if(!azureMotionNormalize(n)||!azureMotionNormalize(t))return false;
    vec4 signedTangent=vec4(t,tangent.w*sign(modelSign*gizmoSign));
    if(!azureFiniteClip(signedTangent))return false;
    worldNormal=n;worldTangent=signedTangent;return true;
}
// Material parameters and UV masks are immutable during a compatible history.
// A changed resource generation invalidates the caller's object history.
bool azurePreviousMotionOutline(uint instanceIndex,uint vertexIndex,uint morphTableBase,uint primitiveIndex,
    vec3 position,vec3 morph0,vec3 morph1,vec3 normal,vec4 tangent,uvec4 joints,vec4 weights,
    vec3 tangentDirection,bool viewScaled,vec4 parameters,vec2 mask,vec2 extent,out vec4 previousClip){
    previousClip=vec4(0);uint record;vec4 skinned;vec3 worldNormal;vec4 worldTangent;
    if(any(lessThanEqual(extent,vec2(0)))
        ||!azurePreviousMotionLocal(instanceIndex,vertexIndex,morphTableBase,position,morph0,morph1,joints,weights,record,skinned)
        ||!azurePreviousMotionDirections(instanceIndex,vertexIndex,morphTableBase,primitiveIndex,normal,tangent,joints,weights,worldNormal,worldTangent))return false;
    mat4 model=azureMotionMatrix(record),mvp=azureMotionMatrix(record+16u);
    // Colour outlines deliberately discard the skin's homogeneous component.
    vec3 local=(azurePreviousMotionGizmo(primitiveIndex)*vec4(skinned.xyz,1)).xyz;
    vec4 clip=mvp*vec4(local,1);
    mat3 inverseModel=inverse(mat3(model));
    if(!viewScaled){
        vec3 localNormal=transpose(mat3(model))*worldNormal;
        if(!azureMotionNormalize(localNormal))return false;
        vec4 shifted=mvp*vec4(local+localNormal*.001,1);
        vec2 direction=(shifted.xy/max(shifted.w,.0001)-clip.xy/max(clip.w,.0001))*extent;
        float magnitude=length(direction);
        if(magnitude>.00001)clip.xy+=direction/magnitude*azureMotionFloat(7u)*2.0/extent*clip.w;
    }else{
        worldNormal=worldTangent.xyz*tangentDirection.x+cross(worldNormal,worldTangent.xyz)*worldTangent.w*tangentDirection.y+worldNormal*tangentDirection.z;
        vec3 rowX=vec3(mvp[0].x,mvp[1].x,mvp[2].x)*inverseModel;
        vec3 rowY=vec3(mvp[0].y,mvp[1].y,mvp[2].y)*inverseModel;
        vec3 rowZ=vec3(mvp[0].z,mvp[1].z,mvp[2].z)*inverseModel;
        vec3 rowW=vec3(mvp[0].w,mvp[1].w,mvp[2].w)*inverseModel;
        vec3 forward=dot(rowW,rowW)>1e-12?rowW:cross(rowX,rowY);
        if(!azureMotionNormalize(forward))return false;
        vec3 centeredX=rowX-forward*dot(rowX,forward),centeredY=rowY-forward*dot(rowY,forward);
        clip.xy+=outlineViewScaledOffset(clip.w,vec2(dot(centeredX,worldNormal),dot(centeredY,worldNormal)),
            extent,length(centeredY),parameters.x,parameters.y,mask.x);
        clip.z=outlineShiftedClipDepth(clip,vec2(dot(rowZ,forward),dot(rowW,forward)),parameters.z*mask.y);
    }
    if(!azureFiniteClip(clip))return false;
    previousClip=clip;return true;
}
#endif
