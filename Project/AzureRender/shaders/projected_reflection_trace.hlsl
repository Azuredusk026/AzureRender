// Public projected reflection program; uniform layout follows ProjectedReflectionFrame.
cbuffer Frame:register(b0){
    float4 worldNormalToView[3];
    float4 viewDirectionToClip[3];
    float4 clipToView[4];
    float4 relativeWorldToClip[4];
    float4 clipToWorld[4];
    float4 previousClipToWorld[4];
    float4 cameraPosition,sourceSize,depthDecode,traceSize,traceConfig,historyConfig,stepConfig,footprintConfig;
};
StructuredBuffer<uint> worklist:register(t0);
Texture2D<float4> previousColor:register(t1);
Texture2D<float4> previousDepth:register(t2);
Texture2D<float4> currentDepth:register(t3);
Texture2D<float4> motionInput:register(t4);
Texture2D<float4> surfaceMask:register(t5);
Texture2D<float4> normalRoughness:register(t6);
SamplerState pointSampler:register(s0);
[[vk::image_format("r11g11b10f")]] RWTexture2D<float4> outputColor:register(u0);
[[vk::image_format("rg16")]] RWTexture2D<unorm float4> outputFootprint:register(u1);
[[vk::image_format("r8")]] RWTexture2D<unorm float4> outputConfidence:register(u2);
float orderedDot(float2 a,float2 b){precise float v=a.x*b.x;return mad(a.y,b.y,v);}
float orderedDot(float3 a,float3 b){precise float v=a.x*b.x;return mad(a.z,b.z,mad(a.y,b.y,v));}
float orderedDot(float4 a,float4 b){precise float v=a.x*b.x;return mad(a.w,b.w,mad(a.z,b.z,mad(a.y,b.y,v)));}
float2 finiteMotion(float2 value){return all(isfinite(value))?value:float2(0,0);}
[numthreads(8,8,1)] void main(uint3 group:SV_GroupID,uint3 thread:SV_GroupThreadID){
uint4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13;
uint packedTile=worklist[group.x];uint2 outputPixel=uint2(packedTile>>16,packedTile&65535u)*8+thread.xy;
// Locate the scheduled 8x8 tile.
uint4 v0=uint4(worklist[(uint4(group,0).xxxx).x+(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)).x/4u],0u,0u,0u);r0.x=v0.x;
uint4 v1=(r0.xxxx)>>(uint4(0x00000010u,0x00000010u,0x00000010u,0x00000010u));r1.x=v1.x;
uint4 v2=(r0.xxxx)&(uint4(0x0000ffffu,0x0000ffffu,0x0000ffffu,0x0000ffffu));r1.y=v2.y;
uint4 v3=asuint((asint(r1.xyxx))*(asint(uint4(0x00000008u,0x00000008u,0x00000000u,0x00000000u)))+(asint(uint4(thread,0).xyxx)));r0.xy=v3.xy;
uint4 v4=uint4(traceSize.xyxx);r1.xy=v4.xy;
uint4 v5=uint4((r0.xyxx)>=(r1.xyxx))*0xffffffffu;r1.xy=v5.xy;
uint4 v6=(r1.yyyy)|(r1.xxxx);r1.x=v6.x;
if((r1.xxxx).x==0u){
// Read the initial reflection eligibility.
float4 v8=outputConfidence[(r0.xyyy).xy].xyzw;r1.x=asuint(v8).x;
float4 v9=float4(asint(r0.xxyx));r1.yz=asuint(v9).yz;
float4 v10=(asfloat(r1.yyzy))+(asfloat(uint4(0x00000000u,0x3f000000u,0x3f000000u,0x00000000u)));r1.yz=asuint(v10).yz;
float4 v11=(asfloat(r1.yzyy))*(traceSize.zwzz);r2.xy=asuint(v11).xy;
uint4 v12=uint4((asfloat(uint4(0x3a83126fu,0x3a83126fu,0x3a83126fu,0x3a83126fu)))<(asfloat(r1.xxxx)))*0xffffffffu;r1.w=v12.w;
if((r1.wwww).x!=0u){
// Select the per-surface trace budget.
float4 v14=float4(r0.xxxy);r2.zw=asuint(v14).zw;
float4 v15=(asfloat(r2.zzzw))+(asfloat(uint4(0x00000000u,0x00000000u,0x3f000000u,0x3f000000u)));r2.zw=asuint(v15).zw;
float4 v16=(asfloat(r2.zzzw))*(traceSize.zzzw);r2.zw=asuint(v16).zw;
float4 v17=surfaceMask.SampleLevel(pointSampler,(asfloat(r2.zwzz)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzwx;r1.w=asuint(v17).w;
uint4 v18=uint4((asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u)))<(asfloat(r1.wwww)))*0xffffffffu;r1.w=v18.w;
float4 v19=(asfloat(uint4(0x3e000000u,0x3e000000u,0x3e000000u,0x3e000000u)))*(stepConfig.xxxx);r2.z=asuint(v19).z;
float4 v20=ceil(asfloat(r2.zzzz));r2.z=asuint(v20).z;
float4 v21=max(asfloat(r2.zzzz),asfloat(uint4(0x40800000u,0x40800000u,0x40800000u,0x40800000u)));r2.z=asuint(v21).z;
uint4 v22=((r1.wwww)!=uint4(0,0,0,0)?asuint(stepConfig.xxxx):r2.zzzz);r1.w=v22.w;
// Reconstruct view position from device depth.
float4 v23=currentDepth.SampleLevel(pointSampler,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzxw;r3.z=asuint(v23).z;
float4 v24=mad(asfloat(r2.xyxx),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r3.xy=asuint(v24).xy;
float4 v25=(asfloat(r3.yyyy))*(clipToView[1].xyzw);r4.xyzw=asuint(v25).xyzw;
float4 v26=mad(clipToView[0].xyzw,asfloat(r3.xxxx),asfloat(r4.xyzw));r4.xyzw=asuint(v26).xyzw;
float4 v27=mad(clipToView[2].xyzw,asfloat(r3.zzzz),asfloat(r4.xyzw));r4.xyzw=asuint(v27).xyzw;
float4 v28=(asfloat(r4.xyzw))+(clipToView[3].xyzw);r4.xyzw=asuint(v28).xyzw;
float4 v29=(asfloat(r4.xyzx))/(asfloat(r4.wwww));r4.xyz=asuint(v29).xyz;
uint4 v30=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r0.zw=v30.zw;
// Decode the Y-pole octahedral normal and roughness.
float4 v31=normalRoughness.Load((asint(r0.xyzw)).xyz).xyzw;r5.xyz=asuint(v31).xyz;
float4 v32=mad(asfloat(r5.xxxy),asfloat(uint4(0x00000000u,0x00000000u,0x40000000u,0x40000000u)),asfloat(uint4(0x00000000u,0x00000000u,0xbf800000u,0xbf800000u)));r0.zw=asuint(v32).zw;
float4 v33=float4(orderedDot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy),orderedDot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy),orderedDot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy),orderedDot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy));r2.z=asuint(v33).z;
float4 v34=(-(asfloat(r2.zzzz)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r6.y=asuint(v34).y;
uint4 v35=uint4((asfloat(r6.yyyy))<(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r2.z=v35.z;
uint4 v36=uint4((asfloat(r0.zwzz))>=(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r5.xy=v36.xy;
float4 v37=(-(abs(asfloat(r0.wzww))))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u)));r7.xy=asuint(v37).xy;
uint4 v38=((r5.xyxx)!=uint4(0,0,0,0)?uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u):uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u));r5.xy=v38.xy;
float4 v39=(asfloat(r5.xyxx))*(asfloat(r7.xyxx));r5.xy=asuint(v39).xy;
uint4 v40=((r2.zzzz)!=uint4(0,0,0,0)?r5.xxyx:r0.zzwz);r6.xz=v40.xz;
float4 v41=float4(orderedDot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),orderedDot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),orderedDot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),orderedDot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz));r0.z=asuint(v41).z;
float4 v42=rsqrt(asfloat(r0.zzzz));r0.z=asuint(v42).z;
float4 v43=(asfloat(r0.zzzz))*(asfloat(r6.xyxz));r5.xyw=asuint(v43).xyw;
float4 v44=(asfloat(r5.zzzz))*(asfloat(r5.zzzz));r0.z=asuint(v44).z;
float4 v45=(asfloat(r0.zzzz))*(asfloat(r0.zzzz));r0.z=asuint(v45).z;
// Reflect the view direction and project the ray.
if(footprintConfig.w>.5){r4.w=r4.y;}else{uint4 v46=(r4.yyyy)^0x80000000u;r4.w=v46.w;}
float4 v47=float4(orderedDot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz),orderedDot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz),orderedDot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz),orderedDot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz));r0.w=asuint(v47).w;
float4 v48=rsqrt(asfloat(r0.wwww));r0.w=asuint(v48).w;
float4 v49=(asfloat(r0.wwww))*(asfloat(r4.xwzx));r4.xyz=asuint(v49).xyz;
float4 v50=(asfloat(r5.yyyy))*(worldNormalToView[1].xyzx);r6.xyz=asuint(v50).xyz;
float4 v51=mad(worldNormalToView[0].xyzx,asfloat(r5.xxxx),asfloat(r6.xyzx));r6.xyz=asuint(v51).xyz;
float4 v52=mad(worldNormalToView[2].xyzx,asfloat(r5.wwww),asfloat(r6.xyzx));r6.xyz=asuint(v52).xyz;
float4 v53=float4(orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz));r0.w=asuint(v53).w;
float4 v54=(asfloat(r0.wwww))+(asfloat(r0.wwww));r0.w=asuint(v54).w;
float4 v55=mad(asfloat(r6.xyzx),-(asfloat(r0.wwww)),asfloat(r4.xyzx));r4.xyz=asuint(v55).xyz;
float4 v56=(asfloat(r4.yyyy))*(viewDirectionToClip[1].xyzw);r6.xyzw=asuint(v56).xyzw;
float4 v57=mad(viewDirectionToClip[0].xyzw,asfloat(r4.xxxx),asfloat(r6.xyzw));r6.xyzw=asuint(v57).xyzw;
float4 v58=mad(viewDirectionToClip[2].xyzw,asfloat(r4.zzzz),asfloat(r6.xyzw));r4.xyzw=asuint(v58).xyzw;
uint4 v59=uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u);r3.w=v59.w;
if(footprintConfig.w>.5){float4 v60=mad(asfloat(r4.xyzw),float4(1,1,1,1),asfloat(r3.xyzw));r4.xyzw=asuint(v60).xyzw;}
else{float4 v60=mad(asfloat(r4.xyzw),asfloat(uint4(0x3f800000u,0xbf800000u,0x3f800000u,0x3f800000u)),asfloat(r3.xyzw));r4.xyzw=asuint(v60).xyzw;}
float4 v61=(asfloat(r4.xyzx))/(asfloat(r4.wwww));r4.xyz=asuint(v61).xyz;
float4 v62=(-(asfloat(r3.xyzx)))+(asfloat(r4.xyzx));r4.xyz=asuint(v62).xyz;
float4 v63=float4(orderedDot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy),orderedDot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy),orderedDot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy),orderedDot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy));r0.w=asuint(v63).w;
float4 v64=sqrt(asfloat(r0.wwww));r0.w=asuint(v64).w;
float4 v65=(asfloat(r0.wwww))*(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u)));r2.z=asuint(v65).z;
float4 v66=mad(asfloat(r3.xyxx),asfloat(r2.zzzz),asfloat(r4.xyxx));r6.xy=asuint(v66).xy;
float4 v67=mad(-(asfloat(r0.wwww)),asfloat(uint4(0x3f000000u,0x3f000000u,0x00000000u,0x00000000u)),abs(asfloat(r6.xyxx)));r6.xy=asuint(v67).xy;
float4 v68=max(asfloat(r6.xyxx),asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)));r6.xy=asuint(v68).xy;
float4 v69=(asfloat(r6.xyxx))/(abs(asfloat(r4.xyxx)));r6.xy=asuint(v69).xy;
float4 v70=(-(asfloat(r6.xyxx)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u)));r6.xy=asuint(v70).xy;
float4 v71=min(asfloat(r6.yyyy),asfloat(r6.xxxx));r0.w=asuint(v71).w;
float4 v72=(asfloat(r0.wwww))/(asfloat(r2.zzzz));r0.w=asuint(v72).w;
float4 v73=(asfloat(r0.wwww))*(asfloat(r4.xyzx));r4.xyz=asuint(v73).xyz;
float4 v74=(abs(asfloat(r4.zzzz)))/(asfloat(r1.wwww));r0.w=asuint(v74).w;
float4 v75=max(asfloat(r0.wwww),asfloat(uint4(0x38d1b717u,0x38d1b717u,0x38d1b717u,0x38d1b717u)));r0.w=asuint(v75).w;
float4 v76=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r1.wwww));r2.z=asuint(v76).z;
float4 v77=(asfloat(r4.xyzx))*(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f800000u,0x00000000u)));r4.xyz=asuint(v77).xyz;
float4 v78=(asfloat(r2.zzzz))*(asfloat(r4.xyzx));r4.xyz=asuint(v78).xyz;
float4 v79=(asfloat(r4.xxxy))*(traceSize.xxxy);r2.zw=asuint(v79).zw;
float4 v80=max(abs(asfloat(r2.wwww)),abs(asfloat(r2.zzzz)));r2.z=asuint(v80).z;
uint4 v81=uint4((asfloat(r2.zzzz))<(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u))))*0xffffffffu;r2.w=v81.w;
float4 v82=(asfloat(r2.zzzz))+(asfloat(uint4(0x3a83126fu,0x3a83126fu,0x3a83126fu,0x3a83126fu)));r2.z=asuint(v82).z;
float4 v83=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r2.zzzz));r2.z=asuint(v83).z;
// Generate the deterministic sample offset.
float4 v84=trunc(traceConfig.xxxx);r3.w=asuint(v84).w;
float4 v85=mad(asfloat(r3.wwww),asfloat(uint4(0x00000000u,0x40054fdfu,0x409bbe77u,0x00000000u)),asfloat(r1.yyzy));r1.yz=asuint(v85).yz;
float4 v86=float4(orderedDot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy));r1.y=asuint(v86).y;
float4 v87=frac(asfloat(r1.yyyy));r1.y=asuint(v87).y;
float4 v88=(asfloat(r1.yyyy))*(asfloat(uint4(0x4253ee82u,0x4253ee82u,0x4253ee82u,0x4253ee82u)));r1.y=asuint(v88).y;
float4 v89=frac(asfloat(r1.yyyy));r1.y=asuint(v89).y;
uint4 v90=((r2.wwww)!=uint4(0,0,0,0)?r2.zzzz:r1.yyyy);r1.y=v90.y;
float4 v91=mad(asfloat(r3.xyzx),asfloat(uint4(0x3f000000u,0x3f000000u,0x3f800000u,0x00000000u)),asfloat(uint4(0x3f000000u,0x3f000000u,0x00000000u,0x00000000u)));r6.xyz=asuint(v91).xyz;
float4 v92=mad(asfloat(r4.xyzx),asfloat(r1.yyyy),asfloat(r6.xyzx));r6.xyz=asuint(v92).xyz;
float4 v93=currentDepth.SampleLevel(pointSampler,(asfloat(r6.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yxzw;r1.y=asuint(v93).y;
float4 v94=(-(asfloat(r1.yyyy)))+(asfloat(r6.zzzz));r1.y=asuint(v94).y;
uint4 v95=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r7.xyz=v95.xyz;
uint4 v96=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r8.xyz=v96.xyz;
uint4 v97=r6.xyzx;r9.xyz=v97.xyz;
uint4 v98=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v98.z;
uint4 v99=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r2.z=v99.z;
// Search four consecutive device-depth positions per batch.
uint4 v100=r1.yyyy;r2.w=v100.w;
[loop]while(true){
uint4 v102=uint4((asfloat(r2.zzzz))<(asfloat(r1.wwww)))*0xffffffffu;r3.w=v102.w;
if((r3.wwww).x!=0u){
float4 v104=mad(asfloat(r4.xyxy),asfloat(uint4(0x3f800000u,0x3f800000u,0x40000000u,0x40000000u)),asfloat(r9.xyxy));r10.xyzw=asuint(v104).xyzw;
float4 v105=mad(asfloat(r4.xyxy),asfloat(uint4(0x40400000u,0x40400000u,0x40800000u,0x40800000u)),asfloat(r9.xyxy));r11.xyzw=asuint(v105).xyzw;
float4 v106=mad(asfloat(r4.zzzz),asfloat(uint4(0x3f800000u,0x40000000u,0x40400000u,0x40800000u)),asfloat(r9.zzzz));r12.xyzw=asuint(v106).xyzw;
float4 v107=currentDepth.SampleLevel(pointSampler,(asfloat(r10.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).xyzw;r13.x=asuint(v107).x;
float4 v108=currentDepth.SampleLevel(pointSampler,(asfloat(r10.zwzz)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yxzw;r13.y=asuint(v108).y;
float4 v109=currentDepth.SampleLevel(pointSampler,(asfloat(r11.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzxw;r13.z=asuint(v109).z;
float4 v110=currentDepth.SampleLevel(pointSampler,(asfloat(r11.zwzz)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzwx;r13.w=asuint(v110).w;
float4 v111=(asfloat(r12.xyzw))+(-(asfloat(r13.xyzw)));r10.xyzw=asuint(v111).xyzw;
float4 v112=(asfloat(r0.wwww))+(asfloat(r10.xyzw));r11.xyzw=asuint(v112).xyzw;
uint4 v113=uint4((abs(asfloat(r11.xyzw)))<(asfloat(r0.wwww)))*0xffffffffu;r11.xyzw=v113.xyzw;
uint4 v114=(r11.zwzz)|(r11.xyxx);r12.xy=v114.xy;
uint4 v115=(r12.yyyy)|(r12.xxxx);r3.w=v115.w;
if((r3.wwww).x!=0u){
uint4 v117=((r11.zzzz)!=uint4(0,0,0,0)?r10.zyzz:r10.wzww);r12.xy=v117.xy;
uint4 v118=((r11.yyyy)!=uint4(0,0,0,0)?r10.yyxy:r12.xxyx);r10.yz=v118.yz;
uint4 v119=((r11.xxxx)!=uint4(0,0,0,0)?r10.xxxx:r10.yyyy);r3.w=v119.w;
uint4 v120=((r11.xxxx)!=uint4(0,0,0,0)?r2.wwww:r10.zzzz);r4.w=v120.w;
uint4 v121=((r11.zzzz)!=uint4(0,0,0,0)?uint4(0x40000000u,0x40000000u,0x40000000u,0x40000000u):uint4(0x40400000u,0x40400000u,0x40400000u,0x40400000u));r5.z=v121.z;
uint4 v122=((r11.yyyy)!=uint4(0,0,0,0)?uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u):r5.zzzz);r5.z=v122.z;
uint4 v123=((r11.xxxx)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):r5.zzzz);r5.z=v123.z;
float4 v124=(-(asfloat(r3.wwww)))+(asfloat(r4.wwww));r3.w=asuint(v124).w;
float4 v125=saturate((asfloat(r4.wwww))/(asfloat(r3.wwww)));r3.w=asuint(v125).w;
float4 v126=(asfloat(r3.wwww))+(asfloat(r5.zzzz));r3.w=asuint(v126).w;
float4 v127=mad(asfloat(r4.xyzx),asfloat(r3.wwww),asfloat(r9.xyzx));r10.xyz=asuint(v127).xyz;
uint4 v128=uint4(0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu);r1.z=v128.z;
uint4 v129=r10.xyzx;r8.xyz=v129.xyz;
break;
}
float4 v132=mad(asfloat(r4.xyzx),asfloat(uint4(0x40800000u,0x40800000u,0x40800000u,0x00000000u)),asfloat(r9.xyzx));r9.xyz=asuint(v132).xyz;
float4 v133=(asfloat(r2.zzzz))+(asfloat(uint4(0x40800000u,0x40800000u,0x40800000u,0x40800000u)));r2.z=asuint(v133).z;
uint4 v134=r9.xyzx;r7.xyz=v134.xyz;
uint4 v135=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v135.z;
uint4 v136=r10.wwww;r2.w=v136.w;
continue;
}else{
uint4 v139=r7.xyzx;r8.xyz=v139.xyz;
uint4 v140=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v140.z;
break;
}
uint4 v143=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v143.z;
}
if((r1.zzzz).x!=0u){
float4 v146=max(asfloat(r8.yyyy),asfloat(r8.xxxx));r0.w=asuint(v146).w;
float4 v147=(-(asfloat(r0.wwww)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r0.w=asuint(v147).w;
// Resolve the hit confidence and screen boundary.
float4 v148=min(asfloat(r8.yyyy),asfloat(r8.xxxx));r1.y=asuint(v148).y;
float4 v149=min(asfloat(r0.wwww),asfloat(r1.yyyy));r0.w=asuint(v149).w;
uint4 v150=uint4((traceConfig.yyyy)<(asfloat(r0.wwww)))*0xffffffffu;r1.y=v150.y;
float4 v151=saturate((asfloat(r0.wwww))/(traceConfig.yyyy));r0.w=asuint(v151).w;
uint4 v152=((r1.yyyy)!=uint4(0,0,0,0)?uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u):r0.wwww);r0.w=v152.w;
float4 v153=(asfloat(r0.wwww))*(asfloat(r1.xxxx));r0.w=asuint(v153).w;
// Decode signed fourth-power motion at the hit.
float4 v154=motionInput.SampleLevel(pointSampler,(asfloat(r8.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).zxyw;r1.yz=asuint(v154).yz;
if(footprintConfig.z>.5){float2 delta=finiteMotion(asfloat(r1.yz));r1.yz=asuint(delta);r2.zw=asuint(abs(delta));}else{
float4 v155=mad(abs(asfloat(r1.yyyz)),asfloat(uint4(0x00000000u,0x00000000u,0x40000000u,0x40000000u)),asfloat(uint4(0x00000000u,0x00000000u,0xbf800000u,0xbf800000u)));r2.zw=asuint(v155).zw;
float4 v156=(asfloat(r2.zzzw))*(asfloat(r2.zzzw));r2.zw=asuint(v156).zw;
float4 v157=(asfloat(r2.zzzw))*(asfloat(r2.zzzw));r2.zw=asuint(v157).zw;
float4 v158=(asfloat(r1.yyzy))+(asfloat(uint4(0x00000000u,0xbf000000u,0xbf000000u,0x00000000u)));r1.yz=asuint(v158).yz;
uint4 v159=uint4((asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)))<(asfloat(r1.yzyy)))*0xffffffffu;r4.xy=v159.xy;
uint4 v160=uint4((asfloat(r1.yyzy))<(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r1.yz=v160.yz;
uint4 v161=asuint((-(asint(r4.xxyx)))+(asint(r1.yyzy)));r1.yz=v161.yz;
float4 v162=float4(asint(r1.yyzy));r1.yz=asuint(v162).yz;
float4 v163=(asfloat(r1.yyzy))*(asfloat(r2.zzwz));r1.yz=asuint(v163).yz;
}
float4 v164=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))+(-(historyConfig.zzzz));r1.w=asuint(v164).w;
float4 v165=(asfloat(r1.wwww))*(asfloat(r1.yyyz));r2.zw=asuint(v165).zw;
float4 v166=mad(-(asfloat(r1.yzyy)),asfloat(r1.wwww),asfloat(r8.xyxx));r2.xy=asuint(v166).xy;
float4 v167=previousDepth.SampleLevel(pointSampler,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yxzw;r1.y=asuint(v167).y;
uint4 v168=uint4((asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)))<(asfloat(r1.yyyy)))*0xffffffffu;r1.z=v168.z;
uint4 v169=uint4((historyConfig.zzzz)<(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u))))*0xffffffffu;r1.w=v169.w;
uint4 v170=(r1.wwww)&(r1.zzzz);r1.z=v170.z;
// Reject inconsistent previous-frame geometry.
float4 v171=mad(asfloat(r2.xyxx),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r4.xy=asuint(v171).xy;
if(footprintConfig.w>.5){float4 v172=asfloat(r4.yyyy)*previousClipToWorld[1].xyzw;r6.xyzw=asuint(v172);}
else{float4 v172=(-(asfloat(r4.yyyy)))*(previousClipToWorld[1].xyzw);r6.xyzw=asuint(v172).xyzw;}
float4 v173=mad(previousClipToWorld[0].xyzw,asfloat(r4.xxxx),asfloat(r6.xyzw));r4.xyzw=asuint(v173).xyzw;
float4 v174=mad(previousClipToWorld[2].xyzw,asfloat(r1.yyyy),asfloat(r4.xyzw));r4.xyzw=asuint(v174).xyzw;
float4 v175=(asfloat(r4.xyzw))+(previousClipToWorld[3].xyzw);r4.xyzw=asuint(v175).xyzw;
float4 v176=mad(asfloat(r8.xxxy),asfloat(uint4(0x00000000u,0x40000000u,0x00000000u,0x40000000u)),asfloat(uint4(0x00000000u,0xbf800000u,0x00000000u,0xbf800000u)));r1.yw=asuint(v176).yw;
if(footprintConfig.w>.5){float4 v177=asfloat(r1.wwww)*clipToWorld[1].xyzw;r6.xyzw=asuint(v177);}
else{float4 v177=(-(asfloat(r1.wwww)))*(clipToWorld[1].xyzw);r6.xyzw=asuint(v177).xyzw;}
float4 v178=mad(clipToWorld[0].xyzw,asfloat(r1.yyyy),asfloat(r6.xyzw));r6.xyzw=asuint(v178).xyzw;
float4 v179=mad(clipToWorld[2].xyzw,asfloat(r8.zzzz),asfloat(r6.xyzw));r6.xyzw=asuint(v179).xyzw;
float4 v180=(asfloat(r6.xyzw))+(clipToWorld[3].xyzw);r6.xyzw=asuint(v180).xyzw;
float4 v181=mad(depthDecode.zzzz,asfloat(r8.zzzz),depthDecode.wwww);r1.y=asuint(v181).y;
float4 v182=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r1.yyyy));r1.y=asuint(v182).y;
float4 v183=(asfloat(r4.xyzx))/(asfloat(r4.wwww));r4.xyz=asuint(v183).xyz;
float4 v184=(asfloat(r6.xyzx))/(asfloat(r6.wwww));r6.xyz=asuint(v184).xyz;
float4 v185=(asfloat(r4.xyzx))+(-(asfloat(r6.xyzx)));r4.xyz=asuint(v185).xyz;
float4 v186=float4(orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz),orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz),orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz),orderedDot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz));r1.w=asuint(v186).w;
float4 v187=sqrt(asfloat(r1.wwww));r1.w=asuint(v187).w;
float4 v188=(asfloat(r1.yyyy))*(asfloat(r1.yyyy));r1.y=asuint(v188).y;
float4 v189=(asfloat(r1.yyyy))*(asfloat(uint4(0x3c23d70au,0x3c23d70au,0x3c23d70au,0x3c23d70au)));r1.y=asuint(v189).y;
float4 v190=max(asfloat(r1.yyyy),asfloat(uint4(0x3e800000u,0x3e800000u,0x3e800000u,0x3e800000u)));r1.y=asuint(v190).y;
uint4 v191=uint4((asfloat(r1.yyyy))<(asfloat(r1.wwww)))*0xffffffffu;r1.y=v191.y;
uint4 v192=((r1.yyyy)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):r0.wwww);r1.y=v192.y;
uint4 v193=((r1.zzzz)!=uint4(0,0,0,0)?r1.yyyy:r0.wwww);r1.x=v193.x;
}else{
uint4 v195=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r2.zw=v195.zw;
uint4 v196=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.x=v196.x;
}
// Compute the roughness-dependent projected footprint.
float4 v198=motionInput.SampleLevel(pointSampler,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).zxyw;r1.yz=asuint(v198).yz;
if(footprintConfig.z>.5){float2 delta=finiteMotion(asfloat(r1.yz));r4.xy=asuint(sqrt(abs(delta)));}else{
float4 v199=mad(abs(asfloat(r1.yzyy)),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r4.xy=asuint(v199).xy;
float4 v200=(asfloat(r4.xyxx))*(asfloat(r4.xyxx));r4.xy=asuint(v200).xy;
}
float4 v201=(asfloat(uint4(0x40000000u,0x40000000u,0x40000000u,0x40000000u)))/(asfloat(r0.zzzz));r0.w=asuint(v201).w;
float4 v202=(asfloat(r0.wwww))+(asfloat(uint4(0xbf800000u,0xbf800000u,0xbf800000u,0xbf800000u)));r0.w=asuint(v202).w;
float4 v203=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r0.wwww));r0.w=asuint(v203).w;
float4 v204=(asfloat(r0.wwww))*(asfloat(uint4(0xc0023e35u,0xc0023e35u,0xc0023e35u,0xc0023e35u)));r0.w=asuint(v204).w;
float4 v205=exp2(asfloat(r0.wwww));r0.w=asuint(v205).w;
float4 v206=(-(asfloat(r0.wwww)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r1.w=asuint(v206).w;
float4 v207=sqrt(asfloat(r1.wwww));r1.w=asuint(v207).w;
float4 v208=mad(asfloat(r0.wwww),asfloat(uint4(0xbc996e30u,0xbc996e30u,0xbc996e30u,0xbc996e30u)),asfloat(uint4(0x3d981627u,0x3d981627u,0x3d981627u,0x3d981627u)));r3.w=asuint(v208).w;
float4 v209=mad(asfloat(r3.wwww),asfloat(r0.wwww),asfloat(uint4(0xbe593484u,0xbe593484u,0xbe593484u,0xbe593484u)));r3.w=asuint(v209).w;
float4 v210=mad(asfloat(r3.wwww),asfloat(r0.wwww),asfloat(uint4(0x3fc90da4u,0x3fc90da4u,0x3fc90da4u,0x3fc90da4u)));r0.w=asuint(v210).w;
float4 v211=(asfloat(r1.wwww))*(asfloat(r0.wwww));r0.w=asuint(v211).w;
// Reconstruct the current sample and its view ray.
float4 v212=(asfloat(r3.yyyy))*(clipToWorld[1].xyzw);r6.xyzw=asuint(v212).xyzw;
float4 v213=mad(clipToWorld[0].xyzw,asfloat(r3.xxxx),asfloat(r6.xyzw));r6.xyzw=asuint(v213).xyzw;
float4 v214=mad(clipToWorld[2].xyzw,asfloat(r3.zzzz),asfloat(r6.xyzw));r6.xyzw=asuint(v214).xyzw;
float4 v215=(asfloat(r6.xyzw))+(clipToWorld[3].xyzw);r6.xyzw=asuint(v215).xyzw;
float4 v216=(asfloat(r6.xyzw))/(asfloat(r6.wwww));r6.xyzw=asuint(v216).xyzw;
float4 v217=mad(asfloat(r8.xyxx),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r3.xy=asuint(v217).xy;
float4 v218=(asfloat(r3.yyyy))*(clipToWorld[1].xyzw);r7.xyzw=asuint(v218).xyzw;
float4 v219=mad(clipToWorld[0].xyzw,asfloat(r3.xxxx),asfloat(r7.xyzw));r7.xyzw=asuint(v219).xyzw;
float4 v220=mad(clipToWorld[2].xyzw,asfloat(r8.zzzz),asfloat(r7.xyzw));r7.xyzw=asuint(v220).xyzw;
float4 v221=(asfloat(r7.xyzw))+(clipToWorld[3].xyzw);r7.xyzw=asuint(v221).xyzw;
float4 v222=(-(asfloat(r6.xyxz)))+(cameraPosition.xyxz);r3.xyw=asuint(v222).xyw;
float4 v223=float4(orderedDot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz),orderedDot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz),orderedDot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz),orderedDot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz));r1.w=asuint(v223).w;
float4 v224=rsqrt(asfloat(r1.wwww));r4.z=asuint(v224).z;
float4 v225=(asfloat(r3.xywx))*(asfloat(r4.zzzz));r8.xyz=asuint(v225).xyz;
float4 v226=sqrt(asfloat(r1.wwww));r1.w=asuint(v226).w;
uint4 v227=uint4((abs(asfloat(r5.wwww)))<(asfloat(uint4(0x3f7fbe77u,0x3f7fbe77u,0x3f7fbe77u,0x3f7fbe77u))))*0xffffffffu;r4.w=v227.w;
uint4 v228=((r4.wwww)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u));r9.z=v228.z;
uint4 v229=(r4.wwww)&(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u));r9.y=v229.y;
uint4 v230=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r9.x=v230.x;
float4 v231=(asfloat(r5.xywx))*(asfloat(r9.xyzx));r10.xyz=asuint(v231).xyz;
float4 v232=mad(asfloat(r9.zxyz),asfloat(r5.ywxy),-(asfloat(r10.xyzx)));r9.xyz=asuint(v232).xyz;
float4 v233=float4(orderedDot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz),orderedDot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz),orderedDot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz),orderedDot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz));r4.w=asuint(v233).w;
float4 v234=rsqrt(asfloat(r4.wwww));r4.w=asuint(v234).w;
float4 v235=(asfloat(r4.wwww))*(asfloat(r9.xyzx));r9.xyz=asuint(v235).xyz;
float4 v236=(asfloat(r7.xyzw))/(asfloat(r7.wwww));r7.xyzw=asuint(v236).xyzw;
float4 v237=(-(asfloat(r6.xyzw)))+(asfloat(r7.xyzw));r6.xyzw=asuint(v237).xyzw;
float4 v238=float4(orderedDot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw),orderedDot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw),orderedDot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw),orderedDot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw));r4.w=asuint(v238).w;
float4 v239=sqrt(asfloat(r4.wwww));r4.w=asuint(v239).w;
float4 v240=(asfloat(r8.xyzx))*(asfloat(r9.zxyz));r6.xyz=asuint(v240).xyz;
float4 v241=mad(asfloat(r9.yzxy),asfloat(r8.yzxy),-(asfloat(r6.xyzx)));r6.xyz=asuint(v241).xyz;
// Rotate footprint directions around the reflected normal.
float sn242,cs242;sincos((asfloat(r0.wwww)).x,sn242,cs242);
r7.x=asuint(sn242);r10.x=asuint(cs242);
float4 v243=mad(-(asfloat(r8.xxyz)),asfloat(r1.wwww),cameraPosition.xxyz);r7.yzw=asuint(v243).yzw;
float4 v244=(asfloat(r5.xxyw))*(asfloat(r9.xxyz));r10.yzw=asuint(v244).yzw;
float4 v245=mad(asfloat(r5.wxyw),asfloat(r9.yzxy),-(asfloat(r10.yzwy)));r5.xyz=asuint(v245).xyz;
float4 v246=(asfloat(r8.xyzx))*(asfloat(r5.xyzx));r9.xyz=asuint(v246).xyz;
float4 v247=mad(asfloat(r5.zxyz),asfloat(r8.yzxy),-(asfloat(r9.xyzx)));r5.xyz=asuint(v247).xyz;
float4 v248=(asfloat(r1.wwww))+(asfloat(r4.wwww));r0.w=asuint(v248).w;
float4 v249=(asfloat(r0.wwww))*(asfloat(r8.xyzx));r9.xyz=asuint(v249).xyz;
float4 v250=(-(asfloat(r9.yyyy)))*(relativeWorldToClip[1].xxyw);r10.yzw=asuint(v250).yzw;
float4 v251=mad(relativeWorldToClip[0].xyxw,-(asfloat(r9.xxxx)),asfloat(r10.yzyw));r9.xyw=asuint(v251).xyw;
float4 v252=mad(relativeWorldToClip[2].xywx,-(asfloat(r9.zzzz)),asfloat(r9.xywx));r9.xyz=asuint(v252).xyz;
float4 v253=(asfloat(r9.xyzx))+(relativeWorldToClip[3].xywx);r9.xyz=asuint(v253).xyz;
float2 q254=mad(asfloat(r9.xy),rcp(asfloat(r9.z))*0.5,0.5);r9.xy=asuint(q254);
float4 v256=(asfloat(r9.xyxx))*(traceSize.xyxx);r9.xy=asuint(v256).xy;
float4 v257=float4(orderedDot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),orderedDot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),orderedDot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),orderedDot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz));r0.w=asuint(v257).w;
float4 v258=(asfloat(r0.wwww))*(asfloat(r6.yyzx));r10.yzw=asuint(v258).yzw;
float4 v259=mad(-(asfloat(r3.xywx)),asfloat(r4.zzzz),-(asfloat(r10.yzwy)));r11.xyz=asuint(v259).xyz;
float4 v260=mad(asfloat(r10.xxxx),asfloat(r11.xxyz),asfloat(r10.yyzw));r10.yzw=asuint(v260).yzw;
float4 v261=(-(asfloat(r8.yzxy)))*(asfloat(r6.xyzx));r11.xyz=asuint(v261).xyz;
float4 v262=mad(asfloat(r6.zxyz),-(asfloat(r8.zxyz)),-(asfloat(r11.xyzx)));r6.xyz=asuint(v262).xyz;
float4 v263=mad(asfloat(r6.xyzx),asfloat(r7.xxxx),asfloat(r10.yzwy));r6.xyz=asuint(v263).xyz;
float4 v264=mad(asfloat(r6.xyzx),asfloat(r4.wwww),asfloat(r7.yzwy));r6.xyz=asuint(v264).xyz;
float4 v265=(asfloat(r6.xyzx))+(-(cameraPosition.xyzx));r6.xyz=asuint(v265).xyz;
float4 v266=(asfloat(r6.yyyy))*(relativeWorldToClip[1].xxyw);r10.yzw=asuint(v266).yzw;
float4 v267=mad(relativeWorldToClip[0].xyxw,asfloat(r6.xxxx),asfloat(r10.yzyw));r6.xyw=asuint(v267).xyw;
float4 v268=mad(relativeWorldToClip[2].xywx,asfloat(r6.zzzz),asfloat(r6.xywx));r6.xyz=asuint(v268).xyz;
float4 v269=(asfloat(r6.xyzx))+(relativeWorldToClip[3].xywx);r6.xyz=asuint(v269).xyz;
float4 v270=float4(orderedDot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),orderedDot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),orderedDot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),orderedDot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz));r0.w=asuint(v270).w;
float4 v271=(asfloat(r0.wwww))*(asfloat(r5.yyzx));r10.yzw=asuint(v271).yzw;
float4 v272=mad(-(asfloat(r3.xyxw)),asfloat(r4.zzzz),-(asfloat(r10.yzyw)));r3.xyw=asuint(v272).xyw;
float4 v273=mad(asfloat(r10.xxxx),asfloat(r3.xyxw),asfloat(r10.yzyw));r3.xyw=asuint(v273).xyw;
float4 v274=(-(asfloat(r8.yzxy)))*(asfloat(r5.xyzx));r10.xyz=asuint(v274).xyz;
float4 v275=mad(asfloat(r5.zxyz),-(asfloat(r8.zxyz)),-(asfloat(r10.xyzx)));r5.xyz=asuint(v275).xyz;
float4 v276=mad(asfloat(r5.xyxz),asfloat(r7.xxxx),asfloat(r3.xyxw));r3.xyw=asuint(v276).xyw;
float4 v277=mad(asfloat(r3.xyxw),asfloat(r4.wwww),asfloat(r7.yzyw));r3.xyw=asuint(v277).xyw;
float4 v278=(asfloat(r3.xyxw))+(-(cameraPosition.xyxz));r3.xyw=asuint(v278).xyw;
float4 v279=(asfloat(r3.yyyy))*(relativeWorldToClip[1].xywx);r5.xyz=asuint(v279).xyz;
float4 v280=mad(relativeWorldToClip[0].xywx,asfloat(r3.xxxx),asfloat(r5.xyzx));r5.xyz=asuint(v280).xyz;
float4 v281=mad(relativeWorldToClip[2].xyxw,asfloat(r3.wwww),asfloat(r5.xyxz));r3.xyw=asuint(v281).xyw;
float4 v282=(asfloat(r3.xyxw))+(relativeWorldToClip[3].xyxw);r3.xyw=asuint(v282).xyw;
// Compute confidence attenuation from source motion.
float4 v283=(asfloat(r4.xyxx))*(asfloat(r4.xyxx));r4.xy=asuint(v283).xy;
float4 v284=(asfloat(r1.yyzy))+(asfloat(uint4(0x00000000u,0xbf000000u,0xbf000000u,0x00000000u)));r1.yz=asuint(v284).yz;
uint4 v285=uint4((asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)))<(asfloat(r1.yyyz)))*0xffffffffu;r4.zw=v285.zw;
uint4 v286=uint4((asfloat(r1.yyzy))<(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r1.yz=v286.yz;
uint4 v287=asuint((-(asint(r4.zzwz)))+(asint(r1.yyzy)));r1.yz=v287.yz;
float4 v288=float4(asint(r1.yyzy));r1.yz=asuint(v288).yz;
float4 v289=mad(asfloat(r4.xxyx),asfloat(r1.yyzy),asfloat(r2.zzwz));r1.yz=asuint(v289).yz;
float4 v290=(asfloat(r1.yyzy))*(sourceSize.xxyx);r1.yz=asuint(v290).yz;
float4 v291=float4(orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy));r0.w=asuint(v291).w;
float4 v292=sqrt(asfloat(r0.wwww));r0.w=asuint(v292).w;
float4 v293=mad(depthDecode.zzzz,asfloat(r3.zzzz),depthDecode.wwww);r1.y=asuint(v293).y;
float4 v294=(asfloat(uint4(0x3ca3d70au,0x3ca3d70au,0x3ca3d70au,0x3ca3d70au)))/(asfloat(r1.yyyy));r1.y=asuint(v294).y;
float4 v295=log2(asfloat(r1.yyyy));r1.y=asuint(v295).y;
float4 v296=saturate(mad(asfloat(r1.yyyy),asfloat(uint4(0x3f317218u,0x3f317218u,0x3f317218u,0x3f317218u)),asfloat(uint4(0x3c23d70au,0x3c23d70au,0x3c23d70au,0x3c23d70au))));r1.y=asuint(v296).y;
float4 v297=(-(asfloat(r0.wwww)))*(asfloat(r1.yyyy));r0.w=asuint(v297).w;
float4 v298=(asfloat(r0.wwww))*(asfloat(uint4(0x3fb8aa3bu,0x3fb8aa3bu,0x3fb8aa3bu,0x3fb8aa3bu)));r0.w=asuint(v298).w;
float4 v299=exp2(asfloat(r0.wwww));r4.x=asuint(v299).x;
uint4 v300=uint4((asfloat(uint4(0x38d1b717u,0x38d1b717u,0x38d1b717u,0x38d1b717u)))>=(asfloat(r0.zzzz)))*0xffffffffu;r0.z=v300.z;
// Measure the projected filter footprint.
float2 q301=mad(asfloat(r6.xy),rcp(asfloat(r6.z))*0.5,0.5);r1.yz=asuint(q301);
float4 v303=mad(-(asfloat(r1.yyzy)),traceSize.xxyx,asfloat(r9.xxyx));r1.yz=asuint(v303).yz;
float4 v304=float4(orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy));r0.w=asuint(v304).w;
float2 q305=mad(asfloat(r3.xy),rcp(asfloat(r3.w))*0.5,0.5);r1.yz=asuint(q305);
float4 v307=mad(-(asfloat(r1.yyzy)),traceSize.xxyx,asfloat(r9.xxyx));r1.yz=asuint(v307).yz;
float4 v308=float4(orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),orderedDot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy));r1.y=asuint(v308).y;
float4 v309=(asfloat(r0.wwww))+(asfloat(r1.yyyy));r0.w=asuint(v309).w;
float4 v310=sqrt(asfloat(r0.wwww));r0.w=asuint(v310).w;
float4 v311=(asfloat(r0.wwww))*(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u)));r0.w=asuint(v311).w;
float4 v312=log2(asfloat(r0.wwww));r0.w=asuint(v312).w;
float4 v313=max(asfloat(r0.wwww),asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)));r0.w=asuint(v313).w;
float4 v314=min(asfloat(r0.wwww),footprintConfig.yyyy);r0.w=asuint(v314).w;
float4 v315=(asfloat(r0.wwww))/(footprintConfig.yyyy);r0.w=asuint(v315).w;
uint4 v316=((r0.zzzz)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):r0.wwww);r4.y=v316.y;
}else{
uint4 v318=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r4.xy=v318.xy;
}
// Encode HDR color and publish the signal.
float4 v320=previousColor.SampleLevel(pointSampler,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).wxyz;r1.yzw=asuint(v320).yzw;
float4 v321=max(asfloat(r1.zzzz),asfloat(r1.yyyy));r0.z=asuint(v321).z;
float4 v322=max(asfloat(r1.wwww),asfloat(r0.zzzz));r0.z=asuint(v322).z;
float4 v323=(asfloat(r0.zzzz))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r0.z=asuint(v323).z;
float4 v324=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r0.zzzz));r0.z=asuint(v324).z;
float4 v325=(asfloat(r0.zzzz))*(asfloat(r1.yzwy));r2.xyzw=asuint(v325).xyzw;
outputColor[outputPixel]=asfloat(r2.xyzw);
outputFootprint[outputPixel]=asfloat(r4.xyxx);
outputConfidence[outputPixel]=asfloat(r1.xxxx);
}
return;
}
