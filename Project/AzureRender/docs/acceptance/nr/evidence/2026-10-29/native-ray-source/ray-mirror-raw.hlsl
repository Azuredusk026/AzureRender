cbuffer C0:register(b0){float4 cb0[65];};
cbuffer C1:register(b1){float4 cb1[3];};
cbuffer C2:register(b2){float4 cb2[6];};
StructuredBuffer<uint> t0:register(t0);
Texture2D<float4> t1:register(t1);
Texture2D<float4> t2:register(t2);
Texture2D<float4> t3:register(t3);
Texture2D<float4> t4:register(t4);
Texture2D<float4> t5:register(t5);
Texture2D<float4> t6:register(t6);
SamplerState s0:register(s0);
RWTexture2D<float4> u0:register(u0);
RWTexture2D<unorm float4> u1:register(u1);
RWTexture2D<unorm float4> u2:register(u2);
[numthreads(8,8,1)] void main(uint3 group:SV_GroupID,uint3 thread:SV_GroupThreadID){
uint4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13;
uint packedTile=t0[group.x];uint2 outputPixel=uint2(packedTile>>16,packedTile&65535u)*8+thread.xy;
// 0: ld_structured_indexable(structured_buffer, stride=4)(mixed,mixed,mixed,mixed) r0.x, vThreadGroupID.x, l(0), t0.xxxx
uint4 v0=uint4(t0[(uint4(group,0).xxxx).x+(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)).x/4u],0u,0u,0u);r0.x=v0.x;
// 1: ushr r1.x, r0.x, l(16)
uint4 v1=(r0.xxxx)>>(uint4(0x00000010u,0x00000010u,0x00000010u,0x00000010u));r1.x=v1.x;
// 2: and r1.y, r0.x, l(0x0000ffff)
uint4 v2=(r0.xxxx)&(uint4(0x0000ffffu,0x0000ffffu,0x0000ffffu,0x0000ffffu));r1.y=v2.y;
// 3: imad r0.xy, r1.xyxx, l(8, 8, 0, 0), vThreadIDInGroup.xyxx
uint4 v3=asuint((asint(r1.xyxx))*(asint(uint4(0x00000008u,0x00000008u,0x00000000u,0x00000000u)))+(asint(uint4(thread,0).xyxx)));r0.xy=v3.xy;
// 4: ftou r1.xy, cb2[0].xyxx
uint4 v4=uint4(cb2[0].xyxx);r1.xy=v4.xy;
// 5: uge r1.xy, r0.xyxx, r1.xyxx
uint4 v5=uint4((r0.xyxx)>=(r1.xyxx))*0xffffffffu;r1.xy=v5.xy;
// 6: or r1.x, r1.y, r1.x
uint4 v6=(r1.yyyy)|(r1.xxxx);r1.x=v6.x;
// 7: if_z r1.x
if((r1.xxxx).x==0u){
// 8: ld_uav_typed(texture2d)(unorm,unorm,unorm,unorm) r1.x, r0.xyyy, u2.xyzw
float4 v8=u2[(r0.xyyy).xy].xyzw;r1.x=asuint(v8).x;
// 9: itof r1.yz, r0.xxyx
float4 v9=float4(asint(r0.xxyx));r1.yz=asuint(v9).yz;
// 10: add r1.yz, r1.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000)
float4 v10=(asfloat(r1.yyzy))+(asfloat(uint4(0x00000000u,0x3f000000u,0x3f000000u,0x00000000u)));r1.yz=asuint(v10).yz;
// 11: mul r2.xy, r1.yzyy, cb2[0].zwzz
float4 v11=(asfloat(r1.yzyy))*(cb2[0].zwzz);r2.xy=asuint(v11).xy;
// 12: lt r1.w, l(0.001000), r1.x
uint4 v12=uint4((asfloat(uint4(0x3a83126fu,0x3a83126fu,0x3a83126fu,0x3a83126fu)))<(asfloat(r1.xxxx)))*0xffffffffu;r1.w=v12.w;
// 13: if_nz r1.w
if((r1.wwww).x!=0u){
// 14: utof r2.zw, r0.xxxy
float4 v14=float4(r0.xxxy);r2.zw=asuint(v14).zw;
// 15: add r2.zw, r2.zzzw, l(0.000000, 0.000000, 0.500000, 0.500000)
float4 v15=(asfloat(r2.zzzw))+(asfloat(uint4(0x00000000u,0x00000000u,0x3f000000u,0x3f000000u)));r2.zw=asuint(v15).zw;
// 16: mul r2.zw, r2.zzzw, cb2[0].zzzw
float4 v16=(asfloat(r2.zzzw))*(cb2[0].zzzw);r2.zw=asuint(v16).zw;
// 17: sample_l(texture2d)(float,float,float,float) r1.w, r2.zwzz, t5.yzwx, s0, l(0)
float4 v17=t5.SampleLevel(s0,(asfloat(r2.zwzz)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzwx;r1.w=asuint(v17).w;
// 18: lt r1.w, l(0.500000), r1.w
uint4 v18=uint4((asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u)))<(asfloat(r1.wwww)))*0xffffffffu;r1.w=v18.w;
// 19: mul r2.z, l(0.125000), cb2[3].x
float4 v19=(asfloat(uint4(0x3e000000u,0x3e000000u,0x3e000000u,0x3e000000u)))*(cb2[3].xxxx);r2.z=asuint(v19).z;
// 20: round_pi r2.z, r2.z
float4 v20=ceil(asfloat(r2.zzzz));r2.z=asuint(v20).z;
// 21: max r2.z, r2.z, l(4.000000)
float4 v21=max(asfloat(r2.zzzz),asfloat(uint4(0x40800000u,0x40800000u,0x40800000u,0x40800000u)));r2.z=asuint(v21).z;
// 22: movc r1.w, r1.w, cb2[3].x, r2.z
uint4 v22=((r1.wwww)!=uint4(0,0,0,0)?asuint(cb2[3].xxxx):r2.zzzz);r1.w=v22.w;
// 23: sample_l(texture2d)(float,float,float,float) r3.z, r2.xyxx, t3.yzxw, s0, l(0)
float4 v23=t3.SampleLevel(s0,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzxw;r3.z=asuint(v23).z;
// 24: mad r3.xy, r2.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
float4 v24=mad(asfloat(r2.xyxx),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r3.xy=asuint(v24).xy;
// 25: mul r4.xyzw, r3.yyyy, cb0[13].xyzw
float4 v25=(asfloat(r3.yyyy))*(cb0[13].xyzw);r4.xyzw=asuint(v25).xyzw;
// 26: mad r4.xyzw, cb0[12].xyzw, r3.xxxx, r4.xyzw
float4 v26=mad(cb0[12].xyzw,asfloat(r3.xxxx),asfloat(r4.xyzw));r4.xyzw=asuint(v26).xyzw;
// 27: mad r4.xyzw, cb0[14].xyzw, r3.zzzz, r4.xyzw
float4 v27=mad(cb0[14].xyzw,asfloat(r3.zzzz),asfloat(r4.xyzw));r4.xyzw=asuint(v27).xyzw;
// 28: add r4.xyzw, r4.xyzw, cb0[15].xyzw
float4 v28=(asfloat(r4.xyzw))+(cb0[15].xyzw);r4.xyzw=asuint(v28).xyzw;
// 29: div r4.xyz, r4.xyzx, r4.wwww
float4 v29=(asfloat(r4.xyzx))/(asfloat(r4.wwww));r4.xyz=asuint(v29).xyz;
// 30: mov r0.zw, l(0, 0, 0, 0)
uint4 v30=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r0.zw=v30.zw;
// 31: ld_indexable(texture2d)(float,float,float,float) r5.xyz, r0.xyzw, t6.xyzw
float4 v31=t6.Load((asint(r0.xyzw)).xyz).xyzw;r5.xyz=asuint(v31).xyz;
// 32: mad r0.zw, r5.xxxy, l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
float4 v32=mad(asfloat(r5.xxxy),asfloat(uint4(0x00000000u,0x00000000u,0x40000000u,0x40000000u)),asfloat(uint4(0x00000000u,0x00000000u,0xbf800000u,0xbf800000u)));r0.zw=asuint(v32).zw;
// 33: dp2 r2.z, l(1.000000, 1.000000, 0.000000, 0.000000), abs(r0.zwzz)
float4 v33=float4(dot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy),dot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy),dot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy),dot((asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u))).xy,(abs(asfloat(r0.zwzz))).xy));r2.z=asuint(v33).z;
// 34: add r6.y, -r2.z, l(1.000000)
float4 v34=(-(asfloat(r2.zzzz)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r6.y=asuint(v34).y;
// 35: lt r2.z, r6.y, l(0)
uint4 v35=uint4((asfloat(r6.yyyy))<(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r2.z=v35.z;
// 36: ge r5.xy, r0.zwzz, l(0, 0, 0, 0)
uint4 v36=uint4((asfloat(r0.zwzz))>=(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r5.xy=v36.xy;
// 37: add r7.xy, -abs(r0.wzww), l(1.000000, 1.000000, 0.000000, 0.000000)
float4 v37=(-(abs(asfloat(r0.wzww))))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u)));r7.xy=asuint(v37).xy;
// 38: movc r5.xy, r5.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
uint4 v38=((r5.xyxx)!=uint4(0,0,0,0)?uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u):uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u));r5.xy=v38.xy;
// 39: mul r5.xy, r5.xyxx, r7.xyxx
float4 v39=(asfloat(r5.xyxx))*(asfloat(r7.xyxx));r5.xy=asuint(v39).xy;
// 40: movc r6.xz, r2.zzzz, r5.xxyx, r0.zzwz
uint4 v40=((r2.zzzz)!=uint4(0,0,0,0)?r5.xxyx:r0.zzwz);r6.xz=v40.xz;
// 41: dp3 r0.z, r6.xyzx, r6.xyzx
float4 v41=float4(dot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),dot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),dot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),dot((asfloat(r6.xyzx)).xyz,(asfloat(r6.xyzx)).xyz));r0.z=asuint(v41).z;
// 42: rsq r0.z, r0.z
float4 v42=rsqrt(asfloat(r0.zzzz));r0.z=asuint(v42).z;
// 43: mul r5.xyw, r0.zzzz, r6.xyxz
float4 v43=(asfloat(r0.zzzz))*(asfloat(r6.xyxz));r5.xyw=asuint(v43).xyw;
// 44: mul r0.z, r5.z, r5.z
float4 v44=(asfloat(r5.zzzz))*(asfloat(r5.zzzz));r0.z=asuint(v44).z;
// 45: mul r0.z, r0.z, r0.z
float4 v45=(asfloat(r0.zzzz))*(asfloat(r0.zzzz));r0.z=asuint(v45).z;
// 46: mov r4.w, -r4.y
uint4 v46=(r4.yyyy)^0x80000000u;r4.w=v46.w;
// 47: dp3 r0.w, r4.xzwx, r4.xzwx
float4 v47=float4(dot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz),dot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz),dot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz),dot((asfloat(r4.xzwx)).xyz,(asfloat(r4.xzwx)).xyz));r0.w=asuint(v47).w;
// 48: rsq r0.w, r0.w
float4 v48=rsqrt(asfloat(r0.wwww));r0.w=asuint(v48).w;
// 49: mul r4.xyz, r0.wwww, r4.xwzx
float4 v49=(asfloat(r0.wwww))*(asfloat(r4.xwzx));r4.xyz=asuint(v49).xyz;
// 50: mul r6.xyz, r5.yyyy, cb0[1].xyzx
float4 v50=(asfloat(r5.yyyy))*(cb0[1].xyzx);r6.xyz=asuint(v50).xyz;
// 51: mad r6.xyz, cb0[0].xyzx, r5.xxxx, r6.xyzx
float4 v51=mad(cb0[0].xyzx,asfloat(r5.xxxx),asfloat(r6.xyzx));r6.xyz=asuint(v51).xyz;
// 52: mad r6.xyz, cb0[2].xyzx, r5.wwww, r6.xyzx
float4 v52=mad(cb0[2].xyzx,asfloat(r5.wwww),asfloat(r6.xyzx));r6.xyz=asuint(v52).xyz;
// 53: dp3 r0.w, r4.xyzx, r6.xyzx
float4 v53=float4(dot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),dot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),dot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz),dot((asfloat(r4.xyzx)).xyz,(asfloat(r6.xyzx)).xyz));r0.w=asuint(v53).w;
// 54: add r0.w, r0.w, r0.w
float4 v54=(asfloat(r0.wwww))+(asfloat(r0.wwww));r0.w=asuint(v54).w;
// 55: mad r4.xyz, r6.xyzx, -r0.wwww, r4.xyzx
float4 v55=mad(asfloat(r6.xyzx),-(asfloat(r0.wwww)),asfloat(r4.xyzx));r4.xyz=asuint(v55).xyz;
// 56: mul r6.xyzw, r4.yyyy, cb0[9].xyzw
float4 v56=(asfloat(r4.yyyy))*(cb0[9].xyzw);r6.xyzw=asuint(v56).xyzw;
// 57: mad r6.xyzw, cb0[8].xyzw, r4.xxxx, r6.xyzw
float4 v57=mad(cb0[8].xyzw,asfloat(r4.xxxx),asfloat(r6.xyzw));r6.xyzw=asuint(v57).xyzw;
// 58: mad r4.xyzw, cb0[10].xyzw, r4.zzzz, r6.xyzw
float4 v58=mad(cb0[10].xyzw,asfloat(r4.zzzz),asfloat(r6.xyzw));r4.xyzw=asuint(v58).xyzw;
// 59: mov r3.w, l(1.000000)
uint4 v59=uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u);r3.w=v59.w;
// 60: mad r4.xyzw, r4.xyzw, l(1.000000, -1.000000, 1.000000, 1.000000), r3.xyzw
float4 v60=mad(asfloat(r4.xyzw),asfloat(uint4(0x3f800000u,0xbf800000u,0x3f800000u,0x3f800000u)),asfloat(r3.xyzw));r4.xyzw=asuint(v60).xyzw;
// 61: div r4.xyz, r4.xyzx, r4.wwww
float4 v61=(asfloat(r4.xyzx))/(asfloat(r4.wwww));r4.xyz=asuint(v61).xyz;
// 62: add r4.xyz, -r3.xyzx, r4.xyzx
float4 v62=(-(asfloat(r3.xyzx)))+(asfloat(r4.xyzx));r4.xyz=asuint(v62).xyz;
// 63: dp2 r0.w, r4.xyxx, r4.xyxx
float4 v63=float4(dot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy),dot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy),dot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy),dot((asfloat(r4.xyxx)).xy,(asfloat(r4.xyxx)).xy));r0.w=asuint(v63).w;
// 64: sqrt r0.w, r0.w
float4 v64=sqrt(asfloat(r0.wwww));r0.w=asuint(v64).w;
// 65: mul r2.z, r0.w, l(0.500000)
float4 v65=(asfloat(r0.wwww))*(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u)));r2.z=asuint(v65).z;
// 66: mad r6.xy, r3.xyxx, r2.zzzz, r4.xyxx
float4 v66=mad(asfloat(r3.xyxx),asfloat(r2.zzzz),asfloat(r4.xyxx));r6.xy=asuint(v66).xy;
// 67: mad r6.xy, -r0.wwww, l(0.500000, 0.500000, 0.000000, 0.000000), abs(r6.xyxx)
float4 v67=mad(-(asfloat(r0.wwww)),asfloat(uint4(0x3f000000u,0x3f000000u,0x00000000u,0x00000000u)),abs(asfloat(r6.xyxx)));r6.xy=asuint(v67).xy;
// 68: max r6.xy, r6.xyxx, l(0, 0, 0, 0)
float4 v68=max(asfloat(r6.xyxx),asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)));r6.xy=asuint(v68).xy;
// 69: div r6.xy, r6.xyxx, abs(r4.xyxx)
float4 v69=(asfloat(r6.xyxx))/(abs(asfloat(r4.xyxx)));r6.xy=asuint(v69).xy;
// 70: add r6.xy, -r6.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000)
float4 v70=(-(asfloat(r6.xyxx)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x00000000u,0x00000000u)));r6.xy=asuint(v70).xy;
// 71: min r0.w, r6.y, r6.x
float4 v71=min(asfloat(r6.yyyy),asfloat(r6.xxxx));r0.w=asuint(v71).w;
// 72: div r0.w, r0.w, r2.z
float4 v72=(asfloat(r0.wwww))/(asfloat(r2.zzzz));r0.w=asuint(v72).w;
// 73: mul r4.xyz, r0.wwww, r4.xyzx
float4 v73=(asfloat(r0.wwww))*(asfloat(r4.xyzx));r4.xyz=asuint(v73).xyz;
// 74: div r0.w, abs(r4.z), r1.w
float4 v74=(abs(asfloat(r4.zzzz)))/(asfloat(r1.wwww));r0.w=asuint(v74).w;
// 75: max r0.w, r0.w, l(0.000100)
float4 v75=max(asfloat(r0.wwww),asfloat(uint4(0x38d1b717u,0x38d1b717u,0x38d1b717u,0x38d1b717u)));r0.w=asuint(v75).w;
// 76: div r2.z, l(1.000000, 1.000000, 1.000000, 1.000000), r1.w
float4 v76=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r1.wwww));r2.z=asuint(v76).z;
// 77: mul r4.xyz, r4.xyzx, l(0.500000, 0.500000, 1.000000, 0.000000)
float4 v77=(asfloat(r4.xyzx))*(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f800000u,0x00000000u)));r4.xyz=asuint(v77).xyz;
// 78: mul r4.xyz, r2.zzzz, r4.xyzx
float4 v78=(asfloat(r2.zzzz))*(asfloat(r4.xyzx));r4.xyz=asuint(v78).xyz;
// 79: mul r2.zw, r4.xxxy, cb2[0].xxxy
float4 v79=(asfloat(r4.xxxy))*(cb2[0].xxxy);r2.zw=asuint(v79).zw;
// 80: max r2.z, abs(r2.w), abs(r2.z)
float4 v80=max(abs(asfloat(r2.wwww)),abs(asfloat(r2.zzzz)));r2.z=asuint(v80).z;
// 81: lt r2.w, r2.z, l(1.000000)
uint4 v81=uint4((asfloat(r2.zzzz))<(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u))))*0xffffffffu;r2.w=v81.w;
// 82: add r2.z, r2.z, l(0.001000)
float4 v82=(asfloat(r2.zzzz))+(asfloat(uint4(0x3a83126fu,0x3a83126fu,0x3a83126fu,0x3a83126fu)));r2.z=asuint(v82).z;
// 83: div r2.z, l(1.000000, 1.000000, 1.000000, 1.000000), r2.z
float4 v83=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r2.zzzz));r2.z=asuint(v83).z;
// 84: round_z r3.w, cb2[1].x
float4 v84=trunc(cb2[1].xxxx);r3.w=asuint(v84).w;
// 85: mad r1.yz, r3.wwww, l(0.000000, 2.083000, 4.867000, 0.000000), r1.yyzy
float4 v85=mad(asfloat(r3.wwww),asfloat(uint4(0x00000000u,0x40054fdfu,0x409bbe77u,0x00000000u)),asfloat(r1.yyzy));r1.yz=asuint(v85).yz;
// 86: dp2 r1.y, r1.yzyy, l(0.067111, 0.005837, 0.000000, 0.000000)
float4 v86=float4(dot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(uint4(0x3d897143u,0x3bbf4590u,0x00000000u,0x00000000u))).xy));r1.y=asuint(v86).y;
// 87: frc r1.y, r1.y
float4 v87=frac(asfloat(r1.yyyy));r1.y=asuint(v87).y;
// 88: mul r1.y, r1.y, l(52.982918)
float4 v88=(asfloat(r1.yyyy))*(asfloat(uint4(0x4253ee82u,0x4253ee82u,0x4253ee82u,0x4253ee82u)));r1.y=asuint(v88).y;
// 89: frc r1.y, r1.y
float4 v89=frac(asfloat(r1.yyyy));r1.y=asuint(v89).y;
// 90: movc r1.y, r2.w, r2.z, r1.y
uint4 v90=((r2.wwww)!=uint4(0,0,0,0)?r2.zzzz:r1.yyyy);r1.y=v90.y;
// 91: mad r6.xyz, r3.xyzx, l(0.500000, 0.500000, 1.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
float4 v91=mad(asfloat(r3.xyzx),asfloat(uint4(0x3f000000u,0x3f000000u,0x3f800000u,0x00000000u)),asfloat(uint4(0x3f000000u,0x3f000000u,0x00000000u,0x00000000u)));r6.xyz=asuint(v91).xyz;
// 92: mad r6.xyz, r4.xyzx, r1.yyyy, r6.xyzx
float4 v92=mad(asfloat(r4.xyzx),asfloat(r1.yyyy),asfloat(r6.xyzx));r6.xyz=asuint(v92).xyz;
// 93: sample_l(texture2d)(float,float,float,float) r1.y, r6.xyxx, t3.yxzw, s0, l(0)
float4 v93=t3.SampleLevel(s0,(asfloat(r6.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yxzw;r1.y=asuint(v93).y;
// 94: add r1.y, -r1.y, r6.z
float4 v94=(-(asfloat(r1.yyyy)))+(asfloat(r6.zzzz));r1.y=asuint(v94).y;
// 95: mov r7.xyz, l(0, 0, 0, 0)
uint4 v95=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r7.xyz=v95.xyz;
// 96: mov r8.xyz, l(0, 0, 0, 0)
uint4 v96=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r8.xyz=v96.xyz;
// 97: mov r9.xyz, r6.xyzx
uint4 v97=r6.xyzx;r9.xyz=v97.xyz;
// 98: mov r1.z, l(0)
uint4 v98=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v98.z;
// 99: mov r2.z, l(0)
uint4 v99=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r2.z=v99.z;
// 100: mov r2.w, r1.y
uint4 v100=r1.yyyy;r2.w=v100.w;
// 101: loop
[loop]while(true){
// 102: lt r3.w, r2.z, r1.w
uint4 v102=uint4((asfloat(r2.zzzz))<(asfloat(r1.wwww)))*0xffffffffu;r3.w=v102.w;
// 103: if_nz r3.w
if((r3.wwww).x!=0u){
// 104: mad r10.xyzw, r4.xyxy, l(1.000000, 1.000000, 2.000000, 2.000000), r9.xyxy
float4 v104=mad(asfloat(r4.xyxy),asfloat(uint4(0x3f800000u,0x3f800000u,0x40000000u,0x40000000u)),asfloat(r9.xyxy));r10.xyzw=asuint(v104).xyzw;
// 105: mad r11.xyzw, r4.xyxy, l(3.000000, 3.000000, 4.000000, 4.000000), r9.xyxy
float4 v105=mad(asfloat(r4.xyxy),asfloat(uint4(0x40400000u,0x40400000u,0x40800000u,0x40800000u)),asfloat(r9.xyxy));r11.xyzw=asuint(v105).xyzw;
// 106: mad r12.xyzw, r4.zzzz, l(1.000000, 2.000000, 3.000000, 4.000000), r9.zzzz
float4 v106=mad(asfloat(r4.zzzz),asfloat(uint4(0x3f800000u,0x40000000u,0x40400000u,0x40800000u)),asfloat(r9.zzzz));r12.xyzw=asuint(v106).xyzw;
// 107: sample_l(texture2d)(float,float,float,float) r13.x, r10.xyxx, t3.xyzw, s0, l(0)
float4 v107=t3.SampleLevel(s0,(asfloat(r10.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).xyzw;r13.x=asuint(v107).x;
// 108: sample_l(texture2d)(float,float,float,float) r13.y, r10.zwzz, t3.yxzw, s0, l(0)
float4 v108=t3.SampleLevel(s0,(asfloat(r10.zwzz)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yxzw;r13.y=asuint(v108).y;
// 109: sample_l(texture2d)(float,float,float,float) r13.z, r11.xyxx, t3.yzxw, s0, l(0)
float4 v109=t3.SampleLevel(s0,(asfloat(r11.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzxw;r13.z=asuint(v109).z;
// 110: sample_l(texture2d)(float,float,float,float) r13.w, r11.zwzz, t3.yzwx, s0, l(0)
float4 v110=t3.SampleLevel(s0,(asfloat(r11.zwzz)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yzwx;r13.w=asuint(v110).w;
// 111: add r10.xyzw, r12.xyzw, -r13.xyzw
float4 v111=(asfloat(r12.xyzw))+(-(asfloat(r13.xyzw)));r10.xyzw=asuint(v111).xyzw;
// 112: add r11.xyzw, r0.wwww, r10.xyzw
float4 v112=(asfloat(r0.wwww))+(asfloat(r10.xyzw));r11.xyzw=asuint(v112).xyzw;
// 113: lt r11.xyzw, abs(r11.xyzw), r0.wwww
uint4 v113=uint4((abs(asfloat(r11.xyzw)))<(asfloat(r0.wwww)))*0xffffffffu;r11.xyzw=v113.xyzw;
// 114: or r12.xy, r11.zwzz, r11.xyxx
uint4 v114=(r11.zwzz)|(r11.xyxx);r12.xy=v114.xy;
// 115: or r3.w, r12.y, r12.x
uint4 v115=(r12.yyyy)|(r12.xxxx);r3.w=v115.w;
// 116: if_nz r3.w
if((r3.wwww).x!=0u){
// 117: movc r12.xy, r11.zzzz, r10.zyzz, r10.wzww
uint4 v117=((r11.zzzz)!=uint4(0,0,0,0)?r10.zyzz:r10.wzww);r12.xy=v117.xy;
// 118: movc r10.yz, r11.yyyy, r10.yyxy, r12.xxyx
uint4 v118=((r11.yyyy)!=uint4(0,0,0,0)?r10.yyxy:r12.xxyx);r10.yz=v118.yz;
// 119: movc r3.w, r11.x, r10.x, r10.y
uint4 v119=((r11.xxxx)!=uint4(0,0,0,0)?r10.xxxx:r10.yyyy);r3.w=v119.w;
// 120: movc r4.w, r11.x, r2.w, r10.z
uint4 v120=((r11.xxxx)!=uint4(0,0,0,0)?r2.wwww:r10.zzzz);r4.w=v120.w;
// 121: movc r5.z, r11.z, l(2.000000), l(3.000000)
uint4 v121=((r11.zzzz)!=uint4(0,0,0,0)?uint4(0x40000000u,0x40000000u,0x40000000u,0x40000000u):uint4(0x40400000u,0x40400000u,0x40400000u,0x40400000u));r5.z=v121.z;
// 122: movc r5.z, r11.y, l(1.000000), r5.z
uint4 v122=((r11.yyyy)!=uint4(0,0,0,0)?uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u):r5.zzzz);r5.z=v122.z;
// 123: movc r5.z, r11.x, l(0), r5.z
uint4 v123=((r11.xxxx)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):r5.zzzz);r5.z=v123.z;
// 124: add r3.w, -r3.w, r4.w
float4 v124=(-(asfloat(r3.wwww)))+(asfloat(r4.wwww));r3.w=asuint(v124).w;
// 125: div_sat r3.w, r4.w, r3.w
float4 v125=saturate((asfloat(r4.wwww))/(asfloat(r3.wwww)));r3.w=asuint(v125).w;
// 126: add r3.w, r3.w, r5.z
float4 v126=(asfloat(r3.wwww))+(asfloat(r5.zzzz));r3.w=asuint(v126).w;
// 127: mad r10.xyz, r4.xyzx, r3.wwww, r9.xyzx
float4 v127=mad(asfloat(r4.xyzx),asfloat(r3.wwww),asfloat(r9.xyzx));r10.xyz=asuint(v127).xyz;
// 128: mov r1.z, l(-1)
uint4 v128=uint4(0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu);r1.z=v128.z;
// 129: mov r8.xyz, r10.xyzx
uint4 v129=r10.xyzx;r8.xyz=v129.xyz;
// 130: break
break;
// 131: endif
}
// 132: mad r9.xyz, r4.xyzx, l(4.000000, 4.000000, 4.000000, 0.000000), r9.xyzx
float4 v132=mad(asfloat(r4.xyzx),asfloat(uint4(0x40800000u,0x40800000u,0x40800000u,0x00000000u)),asfloat(r9.xyzx));r9.xyz=asuint(v132).xyz;
// 133: add r2.z, r2.z, l(4.000000)
float4 v133=(asfloat(r2.zzzz))+(asfloat(uint4(0x40800000u,0x40800000u,0x40800000u,0x40800000u)));r2.z=asuint(v133).z;
// 134: mov r7.xyz, r9.xyzx
uint4 v134=r9.xyzx;r7.xyz=v134.xyz;
// 135: mov r1.z, l(0)
uint4 v135=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v135.z;
// 136: mov r2.w, r10.w
uint4 v136=r10.wwww;r2.w=v136.w;
// 137: continue
continue;
// 138: else
}else{
// 139: mov r8.xyz, r7.xyzx
uint4 v139=r7.xyzx;r8.xyz=v139.xyz;
// 140: mov r1.z, l(0)
uint4 v140=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v140.z;
// 141: break
break;
// 142: endif
}
// 143: mov r1.z, l(0)
uint4 v143=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.z=v143.z;
// 144: endloop
}
// 145: if_nz r1.z
if((r1.zzzz).x!=0u){
// 146: max r0.w, r8.y, r8.x
float4 v146=max(asfloat(r8.yyyy),asfloat(r8.xxxx));r0.w=asuint(v146).w;
// 147: add r0.w, -r0.w, l(1.000000)
float4 v147=(-(asfloat(r0.wwww)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r0.w=asuint(v147).w;
// 148: min r1.y, r8.y, r8.x
float4 v148=min(asfloat(r8.yyyy),asfloat(r8.xxxx));r1.y=asuint(v148).y;
// 149: min r0.w, r0.w, r1.y
float4 v149=min(asfloat(r0.wwww),asfloat(r1.yyyy));r0.w=asuint(v149).w;
// 150: lt r1.y, cb2[1].y, r0.w
uint4 v150=uint4((cb2[1].yyyy)<(asfloat(r0.wwww)))*0xffffffffu;r1.y=v150.y;
// 151: div_sat r0.w, r0.w, cb2[1].y
float4 v151=saturate((asfloat(r0.wwww))/(cb2[1].yyyy));r0.w=asuint(v151).w;
// 152: movc r0.w, r1.y, l(1.000000), r0.w
uint4 v152=((r1.yyyy)!=uint4(0,0,0,0)?uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u):r0.wwww);r0.w=v152.w;
// 153: mul r0.w, r0.w, r1.x
float4 v153=(asfloat(r0.wwww))*(asfloat(r1.xxxx));r0.w=asuint(v153).w;
// 154: sample_l(texture2d)(float,float,float,float) r1.yz, r8.xyxx, t4.zxyw, s0, l(0)
float4 v154=t4.SampleLevel(s0,(asfloat(r8.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).zxyw;r1.yz=asuint(v154).yz;
// 155: mad r2.zw, abs(r1.yyyz), l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
float4 v155=mad(abs(asfloat(r1.yyyz)),asfloat(uint4(0x00000000u,0x00000000u,0x40000000u,0x40000000u)),asfloat(uint4(0x00000000u,0x00000000u,0xbf800000u,0xbf800000u)));r2.zw=asuint(v155).zw;
// 156: mul r2.zw, r2.zzzw, r2.zzzw
float4 v156=(asfloat(r2.zzzw))*(asfloat(r2.zzzw));r2.zw=asuint(v156).zw;
// 157: mul r2.zw, r2.zzzw, r2.zzzw
float4 v157=(asfloat(r2.zzzw))*(asfloat(r2.zzzw));r2.zw=asuint(v157).zw;
// 158: add r1.yz, r1.yyzy, l(0.000000, -0.500000, -0.500000, 0.000000)
float4 v158=(asfloat(r1.yyzy))+(asfloat(uint4(0x00000000u,0xbf000000u,0xbf000000u,0x00000000u)));r1.yz=asuint(v158).yz;
// 159: lt r4.xy, l(0, 0, 0, 0), r1.yzyy
uint4 v159=uint4((asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)))<(asfloat(r1.yzyy)))*0xffffffffu;r4.xy=v159.xy;
// 160: lt r1.yz, r1.yyzy, l(0, 0, 0, 0)
uint4 v160=uint4((asfloat(r1.yyzy))<(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r1.yz=v160.yz;
// 161: iadd r1.yz, -r4.xxyx, r1.yyzy
uint4 v161=asuint((-(asint(r4.xxyx)))+(asint(r1.yyzy)));r1.yz=v161.yz;
// 162: itof r1.yz, r1.yyzy
float4 v162=float4(asint(r1.yyzy));r1.yz=asuint(v162).yz;
// 163: mul r1.yz, r1.yyzy, r2.zzwz
float4 v163=(asfloat(r1.yyzy))*(asfloat(r2.zzwz));r1.yz=asuint(v163).yz;
// 164: add r1.w, l(1.000000), -cb2[2].z
float4 v164=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))+(-(cb2[2].zzzz));r1.w=asuint(v164).w;
// 165: mul r2.zw, r1.wwww, r1.yyyz
float4 v165=(asfloat(r1.wwww))*(asfloat(r1.yyyz));r2.zw=asuint(v165).zw;
// 166: mad r2.xy, -r1.yzyy, r1.wwww, r8.xyxx
float4 v166=mad(-(asfloat(r1.yzyy)),asfloat(r1.wwww),asfloat(r8.xyxx));r2.xy=asuint(v166).xy;
// 167: sample_l(texture2d)(float,float,float,float) r1.y, r2.xyxx, t2.yxzw, s0, l(0)
float4 v167=t2.SampleLevel(s0,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).yxzw;r1.y=asuint(v167).y;
// 168: lt r1.z, l(0), r1.y
uint4 v168=uint4((asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)))<(asfloat(r1.yyyy)))*0xffffffffu;r1.z=v168.z;
// 169: lt r1.w, cb2[2].z, l(0.500000)
uint4 v169=uint4((cb2[2].zzzz)<(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u))))*0xffffffffu;r1.w=v169.w;
// 170: and r1.z, r1.w, r1.z
uint4 v170=(r1.wwww)&(r1.zzzz);r1.z=v170.z;
// 171: mad r4.xy, r2.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
float4 v171=mad(asfloat(r2.xyxx),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r4.xy=asuint(v171).xy;
// 172: mul r6.xyzw, -r4.yyyy, cb0[62].xyzw
float4 v172=(-(asfloat(r4.yyyy)))*(cb0[62].xyzw);r6.xyzw=asuint(v172).xyzw;
// 173: mad r4.xyzw, cb0[61].xyzw, r4.xxxx, r6.xyzw
float4 v173=mad(cb0[61].xyzw,asfloat(r4.xxxx),asfloat(r6.xyzw));r4.xyzw=asuint(v173).xyzw;
// 174: mad r4.xyzw, cb0[63].xyzw, r1.yyyy, r4.xyzw
float4 v174=mad(cb0[63].xyzw,asfloat(r1.yyyy),asfloat(r4.xyzw));r4.xyzw=asuint(v174).xyzw;
// 175: add r4.xyzw, r4.xyzw, cb0[64].xyzw
float4 v175=(asfloat(r4.xyzw))+(cb0[64].xyzw);r4.xyzw=asuint(v175).xyzw;
// 176: mad r1.yw, r8.xxxy, l(0.000000, 2.000000, 0.000000, 2.000000), l(0.000000, -1.000000, 0.000000, -1.000000)
float4 v176=mad(asfloat(r8.xxxy),asfloat(uint4(0x00000000u,0x40000000u,0x00000000u,0x40000000u)),asfloat(uint4(0x00000000u,0xbf800000u,0x00000000u,0xbf800000u)));r1.yw=asuint(v176).yw;
// 177: mul r6.xyzw, -r1.wwww, cb0[25].xyzw
float4 v177=(-(asfloat(r1.wwww)))*(cb0[25].xyzw);r6.xyzw=asuint(v177).xyzw;
// 178: mad r6.xyzw, cb0[24].xyzw, r1.yyyy, r6.xyzw
float4 v178=mad(cb0[24].xyzw,asfloat(r1.yyyy),asfloat(r6.xyzw));r6.xyzw=asuint(v178).xyzw;
// 179: mad r6.xyzw, cb0[26].xyzw, r8.zzzz, r6.xyzw
float4 v179=mad(cb0[26].xyzw,asfloat(r8.zzzz),asfloat(r6.xyzw));r6.xyzw=asuint(v179).xyzw;
// 180: add r6.xyzw, r6.xyzw, cb0[27].xyzw
float4 v180=(asfloat(r6.xyzw))+(cb0[27].xyzw);r6.xyzw=asuint(v180).xyzw;
// 181: mad r1.y, cb1[2].z, r8.z, cb1[2].w
float4 v181=mad(cb1[2].zzzz,asfloat(r8.zzzz),cb1[2].wwww);r1.y=asuint(v181).y;
// 182: div r1.y, l(1.000000, 1.000000, 1.000000, 1.000000), r1.y
float4 v182=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r1.yyyy));r1.y=asuint(v182).y;
// 183: div r4.xyz, r4.xyzx, r4.wwww
float4 v183=(asfloat(r4.xyzx))/(asfloat(r4.wwww));r4.xyz=asuint(v183).xyz;
// 184: div r6.xyz, r6.xyzx, r6.wwww
float4 v184=(asfloat(r6.xyzx))/(asfloat(r6.wwww));r6.xyz=asuint(v184).xyz;
// 185: add r4.xyz, r4.xyzx, -r6.xyzx
float4 v185=(asfloat(r4.xyzx))+(-(asfloat(r6.xyzx)));r4.xyz=asuint(v185).xyz;
// 186: dp3 r1.w, r4.xyzx, r4.xyzx
float4 v186=float4(dot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz),dot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz),dot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz),dot((asfloat(r4.xyzx)).xyz,(asfloat(r4.xyzx)).xyz));r1.w=asuint(v186).w;
// 187: sqrt r1.w, r1.w
float4 v187=sqrt(asfloat(r1.wwww));r1.w=asuint(v187).w;
// 188: mul r1.y, r1.y, r1.y
float4 v188=(asfloat(r1.yyyy))*(asfloat(r1.yyyy));r1.y=asuint(v188).y;
// 189: mul r1.y, r1.y, l(0.010000)
float4 v189=(asfloat(r1.yyyy))*(asfloat(uint4(0x3c23d70au,0x3c23d70au,0x3c23d70au,0x3c23d70au)));r1.y=asuint(v189).y;
// 190: max r1.y, r1.y, l(0.250000)
float4 v190=max(asfloat(r1.yyyy),asfloat(uint4(0x3e800000u,0x3e800000u,0x3e800000u,0x3e800000u)));r1.y=asuint(v190).y;
// 191: lt r1.y, r1.y, r1.w
uint4 v191=uint4((asfloat(r1.yyyy))<(asfloat(r1.wwww)))*0xffffffffu;r1.y=v191.y;
// 192: movc r1.y, r1.y, l(0), r0.w
uint4 v192=((r1.yyyy)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):r0.wwww);r1.y=v192.y;
// 193: movc r1.x, r1.z, r1.y, r0.w
uint4 v193=((r1.zzzz)!=uint4(0,0,0,0)?r1.yyyy:r0.wwww);r1.x=v193.x;
// 194: else
}else{
// 195: mov r2.zw, l(0, 0, 0, 0)
uint4 v195=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r2.zw=v195.zw;
// 196: mov r1.x, l(0)
uint4 v196=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r1.x=v196.x;
// 197: endif
}
// 198: sample_l(texture2d)(float,float,float,float) r1.yz, r2.xyxx, t4.zxyw, s0, l(0)
float4 v198=t4.SampleLevel(s0,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).zxyw;r1.yz=asuint(v198).yz;
// 199: mad r4.xy, abs(r1.yzyy), l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
float4 v199=mad(abs(asfloat(r1.yzyy)),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r4.xy=asuint(v199).xy;
// 200: mul r4.xy, r4.xyxx, r4.xyxx
float4 v200=(asfloat(r4.xyxx))*(asfloat(r4.xyxx));r4.xy=asuint(v200).xy;
// 201: div r0.w, l(2.000000), r0.z
float4 v201=(asfloat(uint4(0x40000000u,0x40000000u,0x40000000u,0x40000000u)))/(asfloat(r0.zzzz));r0.w=asuint(v201).w;
// 202: add r0.w, r0.w, l(-1.000000)
float4 v202=(asfloat(r0.wwww))+(asfloat(uint4(0xbf800000u,0xbf800000u,0xbf800000u,0xbf800000u)));r0.w=asuint(v202).w;
// 203: div r0.w, l(1.000000, 1.000000, 1.000000, 1.000000), r0.w
float4 v203=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r0.wwww));r0.w=asuint(v203).w;
// 204: mul r0.w, r0.w, l(-2.035047)
float4 v204=(asfloat(r0.wwww))*(asfloat(uint4(0xc0023e35u,0xc0023e35u,0xc0023e35u,0xc0023e35u)));r0.w=asuint(v204).w;
// 205: exp r0.w, r0.w
float4 v205=exp2(asfloat(r0.wwww));r0.w=asuint(v205).w;
// 206: add r1.w, -r0.w, l(1.000000)
float4 v206=(-(asfloat(r0.wwww)))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r1.w=asuint(v206).w;
// 207: sqrt r1.w, r1.w
float4 v207=sqrt(asfloat(r1.wwww));r1.w=asuint(v207).w;
// 208: mad r3.w, r0.w, l(-0.018729), l(0.074261)
float4 v208=mad(asfloat(r0.wwww),asfloat(uint4(0xbc996e30u,0xbc996e30u,0xbc996e30u,0xbc996e30u)),asfloat(uint4(0x3d981627u,0x3d981627u,0x3d981627u,0x3d981627u)));r3.w=asuint(v208).w;
// 209: mad r3.w, r3.w, r0.w, l(-0.212114)
float4 v209=mad(asfloat(r3.wwww),asfloat(r0.wwww),asfloat(uint4(0xbe593484u,0xbe593484u,0xbe593484u,0xbe593484u)));r3.w=asuint(v209).w;
// 210: mad r0.w, r3.w, r0.w, l(1.570729)
float4 v210=mad(asfloat(r3.wwww),asfloat(r0.wwww),asfloat(uint4(0x3fc90da4u,0x3fc90da4u,0x3fc90da4u,0x3fc90da4u)));r0.w=asuint(v210).w;
// 211: mul r0.w, r1.w, r0.w
float4 v211=(asfloat(r1.wwww))*(asfloat(r0.wwww));r0.w=asuint(v211).w;
// 212: mul r6.xyzw, r3.yyyy, cb0[25].xyzw
float4 v212=(asfloat(r3.yyyy))*(cb0[25].xyzw);r6.xyzw=asuint(v212).xyzw;
// 213: mad r6.xyzw, cb0[24].xyzw, r3.xxxx, r6.xyzw
float4 v213=mad(cb0[24].xyzw,asfloat(r3.xxxx),asfloat(r6.xyzw));r6.xyzw=asuint(v213).xyzw;
// 214: mad r6.xyzw, cb0[26].xyzw, r3.zzzz, r6.xyzw
float4 v214=mad(cb0[26].xyzw,asfloat(r3.zzzz),asfloat(r6.xyzw));r6.xyzw=asuint(v214).xyzw;
// 215: add r6.xyzw, r6.xyzw, cb0[27].xyzw
float4 v215=(asfloat(r6.xyzw))+(cb0[27].xyzw);r6.xyzw=asuint(v215).xyzw;
// 216: div r6.xyzw, r6.xyzw, r6.wwww
float4 v216=(asfloat(r6.xyzw))/(asfloat(r6.wwww));r6.xyzw=asuint(v216).xyzw;
// 217: mad r3.xy, r8.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
float4 v217=mad(asfloat(r8.xyxx),asfloat(uint4(0x40000000u,0x40000000u,0x00000000u,0x00000000u)),asfloat(uint4(0xbf800000u,0xbf800000u,0x00000000u,0x00000000u)));r3.xy=asuint(v217).xy;
// 218: mul r7.xyzw, r3.yyyy, cb0[25].xyzw
float4 v218=(asfloat(r3.yyyy))*(cb0[25].xyzw);r7.xyzw=asuint(v218).xyzw;
// 219: mad r7.xyzw, cb0[24].xyzw, r3.xxxx, r7.xyzw
float4 v219=mad(cb0[24].xyzw,asfloat(r3.xxxx),asfloat(r7.xyzw));r7.xyzw=asuint(v219).xyzw;
// 220: mad r7.xyzw, cb0[26].xyzw, r8.zzzz, r7.xyzw
float4 v220=mad(cb0[26].xyzw,asfloat(r8.zzzz),asfloat(r7.xyzw));r7.xyzw=asuint(v220).xyzw;
// 221: add r7.xyzw, r7.xyzw, cb0[27].xyzw
float4 v221=(asfloat(r7.xyzw))+(cb0[27].xyzw);r7.xyzw=asuint(v221).xyzw;
// 222: add r3.xyw, -r6.xyxz, cb0[44].xyxz
float4 v222=(-(asfloat(r6.xyxz)))+(cb0[44].xyxz);r3.xyw=asuint(v222).xyw;
// 223: dp3 r1.w, r3.xywx, r3.xywx
float4 v223=float4(dot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz),dot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz),dot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz),dot((asfloat(r3.xywx)).xyz,(asfloat(r3.xywx)).xyz));r1.w=asuint(v223).w;
// 224: rsq r4.z, r1.w
float4 v224=rsqrt(asfloat(r1.wwww));r4.z=asuint(v224).z;
// 225: mul r8.xyz, r3.xywx, r4.zzzz
float4 v225=(asfloat(r3.xywx))*(asfloat(r4.zzzz));r8.xyz=asuint(v225).xyz;
// 226: sqrt r1.w, r1.w
float4 v226=sqrt(asfloat(r1.wwww));r1.w=asuint(v226).w;
// 227: lt r4.w, abs(r5.w), l(0.999000)
uint4 v227=uint4((abs(asfloat(r5.wwww)))<(asfloat(uint4(0x3f7fbe77u,0x3f7fbe77u,0x3f7fbe77u,0x3f7fbe77u))))*0xffffffffu;r4.w=v227.w;
// 228: movc r9.z, r4.w, l(0), l(1.000000)
uint4 v228=((r4.wwww)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u));r9.z=v228.z;
// 229: and r9.y, r4.w, l(1.000000)
uint4 v229=(r4.wwww)&(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u));r9.y=v229.y;
// 230: mov r9.x, l(0)
uint4 v230=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r9.x=v230.x;
// 231: mul r10.xyz, r5.xywx, r9.xyzx
float4 v231=(asfloat(r5.xywx))*(asfloat(r9.xyzx));r10.xyz=asuint(v231).xyz;
// 232: mad r9.xyz, r9.zxyz, r5.ywxy, -r10.xyzx
float4 v232=mad(asfloat(r9.zxyz),asfloat(r5.ywxy),-(asfloat(r10.xyzx)));r9.xyz=asuint(v232).xyz;
// 233: dp3 r4.w, r9.xyzx, r9.xyzx
float4 v233=float4(dot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz),dot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz),dot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz),dot((asfloat(r9.xyzx)).xyz,(asfloat(r9.xyzx)).xyz));r4.w=asuint(v233).w;
// 234: rsq r4.w, r4.w
float4 v234=rsqrt(asfloat(r4.wwww));r4.w=asuint(v234).w;
// 235: mul r9.xyz, r4.wwww, r9.xyzx
float4 v235=(asfloat(r4.wwww))*(asfloat(r9.xyzx));r9.xyz=asuint(v235).xyz;
// 236: div r7.xyzw, r7.xyzw, r7.wwww
float4 v236=(asfloat(r7.xyzw))/(asfloat(r7.wwww));r7.xyzw=asuint(v236).xyzw;
// 237: add r6.xyzw, -r6.xyzw, r7.xyzw
float4 v237=(-(asfloat(r6.xyzw)))+(asfloat(r7.xyzw));r6.xyzw=asuint(v237).xyzw;
// 238: dp4 r4.w, r6.xyzw, r6.xyzw
float4 v238=float4(dot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw),dot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw),dot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw),dot((asfloat(r6.xyzw)).xyzw,(asfloat(r6.xyzw)).xyzw));r4.w=asuint(v238).w;
// 239: sqrt r4.w, r4.w
float4 v239=sqrt(asfloat(r4.wwww));r4.w=asuint(v239).w;
// 240: mul r6.xyz, r8.xyzx, r9.zxyz
float4 v240=(asfloat(r8.xyzx))*(asfloat(r9.zxyz));r6.xyz=asuint(v240).xyz;
// 241: mad r6.xyz, r9.yzxy, r8.yzxy, -r6.xyzx
float4 v241=mad(asfloat(r9.yzxy),asfloat(r8.yzxy),-(asfloat(r6.xyzx)));r6.xyz=asuint(v241).xyz;
// 242: sincos r7.x, r10.x, r0.w
float sn242,cs242;sincos((asfloat(r0.wwww)).x,sn242,cs242);
r7.x=asuint(sn242);r10.x=asuint(cs242);
// 243: mad r7.yzw, -r8.xxyz, r1.wwww, cb0[44].xxyz
float4 v243=mad(-(asfloat(r8.xxyz)),asfloat(r1.wwww),cb0[44].xxyz);r7.yzw=asuint(v243).yzw;
// 244: mul r10.yzw, r5.xxyw, r9.xxyz
float4 v244=(asfloat(r5.xxyw))*(asfloat(r9.xxyz));r10.yzw=asuint(v244).yzw;
// 245: mad r5.xyz, r5.wxyw, r9.yzxy, -r10.yzwy
float4 v245=mad(asfloat(r5.wxyw),asfloat(r9.yzxy),-(asfloat(r10.yzwy)));r5.xyz=asuint(v245).xyz;
// 246: mul r9.xyz, r8.xyzx, r5.xyzx
float4 v246=(asfloat(r8.xyzx))*(asfloat(r5.xyzx));r9.xyz=asuint(v246).xyz;
// 247: mad r5.xyz, r5.zxyz, r8.yzxy, -r9.xyzx
float4 v247=mad(asfloat(r5.zxyz),asfloat(r8.yzxy),-(asfloat(r9.xyzx)));r5.xyz=asuint(v247).xyz;
// 248: add r0.w, r1.w, r4.w
float4 v248=(asfloat(r1.wwww))+(asfloat(r4.wwww));r0.w=asuint(v248).w;
// 249: mul r9.xyz, r0.wwww, r8.xyzx
float4 v249=(asfloat(r0.wwww))*(asfloat(r8.xyzx));r9.xyz=asuint(v249).xyz;
// 250: mul r10.yzw, -r9.yyyy, cb0[21].xxyw
float4 v250=(-(asfloat(r9.yyyy)))*(cb0[21].xxyw);r10.yzw=asuint(v250).yzw;
// 251: mad r9.xyw, cb0[20].xyxw, -r9.xxxx, r10.yzyw
float4 v251=mad(cb0[20].xyxw,-(asfloat(r9.xxxx)),asfloat(r10.yzyw));r9.xyw=asuint(v251).xyw;
// 252: mad r9.xyz, cb0[22].xywx, -r9.zzzz, r9.xywx
float4 v252=mad(cb0[22].xywx,-(asfloat(r9.zzzz)),asfloat(r9.xywx));r9.xyz=asuint(v252).xyz;
// 253: add r9.xyz, r9.xyzx, cb0[23].xywx
float4 v253=(asfloat(r9.xyzx))+(cb0[23].xywx);r9.xyz=asuint(v253).xyz;
// 254: div r9.xy, r9.xyxx, r9.zzzz
float4 v254=(asfloat(r9.xyxx))/(asfloat(r9.zzzz));r9.xy=asuint(v254).xy;
// 255: mad r9.xy, r9.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
float4 v255=mad(asfloat(r9.xyxx),asfloat(uint4(0x3f000000u,0x3f000000u,0x00000000u,0x00000000u)),asfloat(uint4(0x3f000000u,0x3f000000u,0x00000000u,0x00000000u)));r9.xy=asuint(v255).xy;
// 256: mul r9.xy, r9.xyxx, cb2[0].xyxx
float4 v256=(asfloat(r9.xyxx))*(cb2[0].xyxx);r9.xy=asuint(v256).xy;
// 257: dp3 r0.w, r6.yzxy, -r8.xyzx
float4 v257=float4(dot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),dot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),dot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),dot((asfloat(r6.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz));r0.w=asuint(v257).w;
// 258: mul r10.yzw, r0.wwww, r6.yyzx
float4 v258=(asfloat(r0.wwww))*(asfloat(r6.yyzx));r10.yzw=asuint(v258).yzw;
// 259: mad r11.xyz, -r3.xywx, r4.zzzz, -r10.yzwy
float4 v259=mad(-(asfloat(r3.xywx)),asfloat(r4.zzzz),-(asfloat(r10.yzwy)));r11.xyz=asuint(v259).xyz;
// 260: mad r10.yzw, r10.xxxx, r11.xxyz, r10.yyzw
float4 v260=mad(asfloat(r10.xxxx),asfloat(r11.xxyz),asfloat(r10.yyzw));r10.yzw=asuint(v260).yzw;
// 261: mul r11.xyz, -r8.yzxy, r6.xyzx
float4 v261=(-(asfloat(r8.yzxy)))*(asfloat(r6.xyzx));r11.xyz=asuint(v261).xyz;
// 262: mad r6.xyz, r6.zxyz, -r8.zxyz, -r11.xyzx
float4 v262=mad(asfloat(r6.zxyz),-(asfloat(r8.zxyz)),-(asfloat(r11.xyzx)));r6.xyz=asuint(v262).xyz;
// 263: mad r6.xyz, r6.xyzx, r7.xxxx, r10.yzwy
float4 v263=mad(asfloat(r6.xyzx),asfloat(r7.xxxx),asfloat(r10.yzwy));r6.xyz=asuint(v263).xyz;
// 264: mad r6.xyz, r6.xyzx, r4.wwww, r7.yzwy
float4 v264=mad(asfloat(r6.xyzx),asfloat(r4.wwww),asfloat(r7.yzwy));r6.xyz=asuint(v264).xyz;
// 265: add r6.xyz, r6.xyzx, -cb0[44].xyzx
float4 v265=(asfloat(r6.xyzx))+(-(cb0[44].xyzx));r6.xyz=asuint(v265).xyz;
// 266: mul r10.yzw, r6.yyyy, cb0[21].xxyw
float4 v266=(asfloat(r6.yyyy))*(cb0[21].xxyw);r10.yzw=asuint(v266).yzw;
// 267: mad r6.xyw, cb0[20].xyxw, r6.xxxx, r10.yzyw
float4 v267=mad(cb0[20].xyxw,asfloat(r6.xxxx),asfloat(r10.yzyw));r6.xyw=asuint(v267).xyw;
// 268: mad r6.xyz, cb0[22].xywx, r6.zzzz, r6.xywx
float4 v268=mad(cb0[22].xywx,asfloat(r6.zzzz),asfloat(r6.xywx));r6.xyz=asuint(v268).xyz;
// 269: add r6.xyz, r6.xyzx, cb0[23].xywx
float4 v269=(asfloat(r6.xyzx))+(cb0[23].xywx);r6.xyz=asuint(v269).xyz;
// 270: dp3 r0.w, r5.yzxy, -r8.xyzx
float4 v270=float4(dot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),dot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),dot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz),dot((asfloat(r5.yzxy)).xyz,(-(asfloat(r8.xyzx))).xyz));r0.w=asuint(v270).w;
// 271: mul r10.yzw, r0.wwww, r5.yyzx
float4 v271=(asfloat(r0.wwww))*(asfloat(r5.yyzx));r10.yzw=asuint(v271).yzw;
// 272: mad r3.xyw, -r3.xyxw, r4.zzzz, -r10.yzyw
float4 v272=mad(-(asfloat(r3.xyxw)),asfloat(r4.zzzz),-(asfloat(r10.yzyw)));r3.xyw=asuint(v272).xyw;
// 273: mad r3.xyw, r10.xxxx, r3.xyxw, r10.yzyw
float4 v273=mad(asfloat(r10.xxxx),asfloat(r3.xyxw),asfloat(r10.yzyw));r3.xyw=asuint(v273).xyw;
// 274: mul r10.xyz, -r8.yzxy, r5.xyzx
float4 v274=(-(asfloat(r8.yzxy)))*(asfloat(r5.xyzx));r10.xyz=asuint(v274).xyz;
// 275: mad r5.xyz, r5.zxyz, -r8.zxyz, -r10.xyzx
float4 v275=mad(asfloat(r5.zxyz),-(asfloat(r8.zxyz)),-(asfloat(r10.xyzx)));r5.xyz=asuint(v275).xyz;
// 276: mad r3.xyw, r5.xyxz, r7.xxxx, r3.xyxw
float4 v276=mad(asfloat(r5.xyxz),asfloat(r7.xxxx),asfloat(r3.xyxw));r3.xyw=asuint(v276).xyw;
// 277: mad r3.xyw, r3.xyxw, r4.wwww, r7.yzyw
float4 v277=mad(asfloat(r3.xyxw),asfloat(r4.wwww),asfloat(r7.yzyw));r3.xyw=asuint(v277).xyw;
// 278: add r3.xyw, r3.xyxw, -cb0[44].xyxz
float4 v278=(asfloat(r3.xyxw))+(-(cb0[44].xyxz));r3.xyw=asuint(v278).xyw;
// 279: mul r5.xyz, r3.yyyy, cb0[21].xywx
float4 v279=(asfloat(r3.yyyy))*(cb0[21].xywx);r5.xyz=asuint(v279).xyz;
// 280: mad r5.xyz, cb0[20].xywx, r3.xxxx, r5.xyzx
float4 v280=mad(cb0[20].xywx,asfloat(r3.xxxx),asfloat(r5.xyzx));r5.xyz=asuint(v280).xyz;
// 281: mad r3.xyw, cb0[22].xyxw, r3.wwww, r5.xyxz
float4 v281=mad(cb0[22].xyxw,asfloat(r3.wwww),asfloat(r5.xyxz));r3.xyw=asuint(v281).xyw;
// 282: add r3.xyw, r3.xyxw, cb0[23].xyxw
float4 v282=(asfloat(r3.xyxw))+(cb0[23].xyxw);r3.xyw=asuint(v282).xyw;
// 283: mul r4.xy, r4.xyxx, r4.xyxx
float4 v283=(asfloat(r4.xyxx))*(asfloat(r4.xyxx));r4.xy=asuint(v283).xy;
// 284: add r1.yz, r1.yyzy, l(0.000000, -0.500000, -0.500000, 0.000000)
float4 v284=(asfloat(r1.yyzy))+(asfloat(uint4(0x00000000u,0xbf000000u,0xbf000000u,0x00000000u)));r1.yz=asuint(v284).yz;
// 285: lt r4.zw, l(0, 0, 0, 0), r1.yyyz
uint4 v285=uint4((asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)))<(asfloat(r1.yyyz)))*0xffffffffu;r4.zw=v285.zw;
// 286: lt r1.yz, r1.yyzy, l(0, 0, 0, 0)
uint4 v286=uint4((asfloat(r1.yyzy))<(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))))*0xffffffffu;r1.yz=v286.yz;
// 287: iadd r1.yz, -r4.zzwz, r1.yyzy
uint4 v287=asuint((-(asint(r4.zzwz)))+(asint(r1.yyzy)));r1.yz=v287.yz;
// 288: itof r1.yz, r1.yyzy
float4 v288=float4(asint(r1.yyzy));r1.yz=asuint(v288).yz;
// 289: mad r1.yz, r4.xxyx, r1.yyzy, r2.zzwz
float4 v289=mad(asfloat(r4.xxyx),asfloat(r1.yyzy),asfloat(r2.zzwz));r1.yz=asuint(v289).yz;
// 290: mul r1.yz, r1.yyzy, cb1[0].xxyx
float4 v290=(asfloat(r1.yyzy))*(cb1[0].xxyx);r1.yz=asuint(v290).yz;
// 291: dp2 r0.w, r1.yzyy, r1.yzyy
float4 v291=float4(dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy));r0.w=asuint(v291).w;
// 292: sqrt r0.w, r0.w
float4 v292=sqrt(asfloat(r0.wwww));r0.w=asuint(v292).w;
// 293: mad r1.y, cb1[2].z, r3.z, cb1[2].w
float4 v293=mad(cb1[2].zzzz,asfloat(r3.zzzz),cb1[2].wwww);r1.y=asuint(v293).y;
// 294: div r1.y, l(0.020000), r1.y
float4 v294=(asfloat(uint4(0x3ca3d70au,0x3ca3d70au,0x3ca3d70au,0x3ca3d70au)))/(asfloat(r1.yyyy));r1.y=asuint(v294).y;
// 295: log r1.y, r1.y
float4 v295=log2(asfloat(r1.yyyy));r1.y=asuint(v295).y;
// 296: mad_sat r1.y, r1.y, l(0.693147), l(0.010000)
float4 v296=saturate(mad(asfloat(r1.yyyy),asfloat(uint4(0x3f317218u,0x3f317218u,0x3f317218u,0x3f317218u)),asfloat(uint4(0x3c23d70au,0x3c23d70au,0x3c23d70au,0x3c23d70au))));r1.y=asuint(v296).y;
// 297: mul r0.w, -r0.w, r1.y
float4 v297=(-(asfloat(r0.wwww)))*(asfloat(r1.yyyy));r0.w=asuint(v297).w;
// 298: mul r0.w, r0.w, l(1.442695)
float4 v298=(asfloat(r0.wwww))*(asfloat(uint4(0x3fb8aa3bu,0x3fb8aa3bu,0x3fb8aa3bu,0x3fb8aa3bu)));r0.w=asuint(v298).w;
// 299: exp r4.x, r0.w
float4 v299=exp2(asfloat(r0.wwww));r4.x=asuint(v299).x;
// 300: ge r0.z, l(0.000100), r0.z
uint4 v300=uint4((asfloat(uint4(0x38d1b717u,0x38d1b717u,0x38d1b717u,0x38d1b717u)))>=(asfloat(r0.zzzz)))*0xffffffffu;r0.z=v300.z;
// 301: div r1.yz, r6.xxyx, r6.zzzz
float4 v301=(asfloat(r6.xxyx))/(asfloat(r6.zzzz));r1.yz=asuint(v301).yz;
// 302: mad r1.yz, r1.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 0.500000, 0.500000, 0.000000)
float4 v302=mad(asfloat(r1.yyzy),asfloat(uint4(0x00000000u,0x3f000000u,0x3f000000u,0x00000000u)),asfloat(uint4(0x00000000u,0x3f000000u,0x3f000000u,0x00000000u)));r1.yz=asuint(v302).yz;
// 303: mad r1.yz, -r1.yyzy, cb2[0].xxyx, r9.xxyx
float4 v303=mad(-(asfloat(r1.yyzy)),cb2[0].xxyx,asfloat(r9.xxyx));r1.yz=asuint(v303).yz;
// 304: dp2 r0.w, r1.yzyy, r1.yzyy
float4 v304=float4(dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy));r0.w=asuint(v304).w;
// 305: div r1.yz, r3.xxyx, r3.wwww
float4 v305=(asfloat(r3.xxyx))/(asfloat(r3.wwww));r1.yz=asuint(v305).yz;
// 306: mad r1.yz, r1.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 0.500000, 0.500000, 0.000000)
float4 v306=mad(asfloat(r1.yyzy),asfloat(uint4(0x00000000u,0x3f000000u,0x3f000000u,0x00000000u)),asfloat(uint4(0x00000000u,0x3f000000u,0x3f000000u,0x00000000u)));r1.yz=asuint(v306).yz;
// 307: mad r1.yz, -r1.yyzy, cb2[0].xxyx, r9.xxyx
float4 v307=mad(-(asfloat(r1.yyzy)),cb2[0].xxyx,asfloat(r9.xxyx));r1.yz=asuint(v307).yz;
// 308: dp2 r1.y, r1.yzyy, r1.yzyy
float4 v308=float4(dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy),dot((asfloat(r1.yzyy)).xy,(asfloat(r1.yzyy)).xy));r1.y=asuint(v308).y;
// 309: add r0.w, r0.w, r1.y
float4 v309=(asfloat(r0.wwww))+(asfloat(r1.yyyy));r0.w=asuint(v309).w;
// 310: sqrt r0.w, r0.w
float4 v310=sqrt(asfloat(r0.wwww));r0.w=asuint(v310).w;
// 311: mul r0.w, r0.w, l(0.500000)
float4 v311=(asfloat(r0.wwww))*(asfloat(uint4(0x3f000000u,0x3f000000u,0x3f000000u,0x3f000000u)));r0.w=asuint(v311).w;
// 312: log r0.w, r0.w
float4 v312=log2(asfloat(r0.wwww));r0.w=asuint(v312).w;
// 313: max r0.w, r0.w, l(0)
float4 v313=max(asfloat(r0.wwww),asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u)));r0.w=asuint(v313).w;
// 314: min r0.w, r0.w, cb2[5].y
float4 v314=min(asfloat(r0.wwww),cb2[5].yyyy);r0.w=asuint(v314).w;
// 315: div r0.w, r0.w, cb2[5].y
float4 v315=(asfloat(r0.wwww))/(cb2[5].yyyy);r0.w=asuint(v315).w;
// 316: movc r4.y, r0.z, l(0), r0.w
uint4 v316=((r0.zzzz)!=uint4(0,0,0,0)?uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u):r0.wwww);r4.y=v316.y;
// 317: else
}else{
// 318: mov r4.xy, l(0, 0, 0, 0)
uint4 v318=uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u);r4.xy=v318.xy;
// 319: endif
}
// 320: sample_l(texture2d)(float,float,float,float) r1.yzw, r2.xyxx, t1.wxyz, s0, l(0)
float4 v320=t1.SampleLevel(s0,(asfloat(r2.xyxx)).xy,(asfloat(uint4(0x00000000u,0x00000000u,0x00000000u,0x00000000u))).x).wxyz;r1.yzw=asuint(v320).yzw;
// 321: max r0.z, r1.z, r1.y
float4 v321=max(asfloat(r1.zzzz),asfloat(r1.yyyy));r0.z=asuint(v321).z;
// 322: max r0.z, r1.w, r0.z
float4 v322=max(asfloat(r1.wwww),asfloat(r0.zzzz));r0.z=asuint(v322).z;
// 323: add r0.z, r0.z, l(1.000000)
float4 v323=(asfloat(r0.zzzz))+(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)));r0.z=asuint(v323).z;
// 324: div r0.z, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
float4 v324=(asfloat(uint4(0x3f800000u,0x3f800000u,0x3f800000u,0x3f800000u)))/(asfloat(r0.zzzz));r0.z=asuint(v324).z;
// 325: mul r2.xyzw, r0.zzzz, r1.yzwy
float4 v325=(asfloat(r0.zzzz))*(asfloat(r1.yzwy));r2.xyzw=asuint(v325).xyzw;
// 326: store_uav_typed u0.xyzw, r0.xyyy, r2.xyzw
u0[outputPixel]=asfloat(r2.xyzw);
// 327: store_uav_typed u1.xyzw, r0.xyyy, r4.xyxx
u1[outputPixel]=asfloat(r4.xyxx);
// 328: store_uav_typed u2.xyzw, r0.xyyy, r1.xxxx
u2[outputPixel]=asfloat(r1.xxxx);
// 329: endif
}
// 330: ret
return;
}
