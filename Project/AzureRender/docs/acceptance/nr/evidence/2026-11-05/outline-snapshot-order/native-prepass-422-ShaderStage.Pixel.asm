Shader hash d2a926b5-5c54d3ec-6511b35b-32edbf30

ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[27], immediateIndexed
      dcl_constantbuffer cb1[4086], dynamicIndexed
      dcl_constantbuffer cb2[13], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_input_ps linear v1.xy
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyzw
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps nointerpolation v7.x
      dcl_input_ps_sgv nointerpolation v8.x, isfrontface
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyzw
      dcl_temps 3
   0: sample_b(texture2d)(float,float,float,float) r0.x, v1.xyxx, t2.xyzw, s0, cb0[26].x
   1: add r0.x, r0.x, -cb2[12].x
   2: lt r0.x, r0.x, l(0)
   3: discard_nz r0.x
   4: sample_b(texture2d)(float,float,float,float) r0.xyz, v1.xyxx, t0.xyzw, s0, cb0[26].x
   5: mul o4.xyz, r0.xyzx, cb2[6].xyzx
   6: sample_b(texture2d)(float,float,float,float) r0.xy, v1.xyxx, t1.xyzw, s0, cb0[26].x
   7: mad r0.xy, r0.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
   8: dp2 r0.z, r0.xyxx, r0.xyxx
   9: min r0.z, r0.z, l(1.000000)
  10: add r0.z, -r0.z, l(1.000000)
  11: sqrt r0.z, r0.z
  12: max r0.z, r0.z, l(0.000000)
  13: mul r0.xy, r0.xyxx, cb2[0].wwww
  14: mul r1.xyz, v3.zxyz, v4.yzxy
  15: mad r1.xyz, v3.yzxy, v4.zxyz, -r1.xyzx
  16: mul r1.xyz, r1.xyzx, v4.wwww
  17: mul r1.xyz, r0.yyyy, r1.xyzx
  18: mad r0.xyw, r0.xxxx, v4.xyxz, r1.xyxz
  19: mad r0.xyz, r0.zzzz, v3.xyzx, r0.xywx
  20: max r0.w, v5.z, l(0.000000)
  21: div r1.xy, v5.xyxx, r0.wwww
  22: max r0.w, v6.z, l(0.000000)
  23: div r1.zw, v6.xxxy, r0.wwww
  24: add r1.xy, -r1.zwzz, r1.xyxx
  25: mul r2.xy, r1.xyxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  26: sqrt r2.xy, abs(r2.xyxx)
  27: sqrt r2.xy, r2.xyxx
  28: mov r1.z, -r1.y
  29: lt r1.yw, l(0, 0, 0, 0), r1.xxxz
  30: lt r1.xz, r1.xxzx, l(0, 0, 0, 0)
  31: iadd r1.xy, -r1.ywyy, r1.xzxx
  32: itof r1.xy, r1.xyxx
  33: mul r1.xy, r1.xyxx, r2.xyxx
  34: mad o1.xy, r1.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
  35: ishl r0.w, v7.x, l(4)
  36: dp3 r1.x, r0.xyzx, r0.xyzx
  37: max r1.x, r1.x, l(0.000000)
  38: rsq r1.x, r1.x
  39: mul r0.xyz, r0.xyzx, r1.xxxx
  40: mad r1.x, cb2[1].y, l(2.000000), l(-1.000000)
  41: movc r1.x, v8.x, l(1.000000), r1.x
  42: mul r0.xyz, r0.xyzx, r1.xxxx
  43: dp3 r1.x, r0.xyzx, r0.xyzx
  44: rsq r1.x, r1.x
  45: mul r0.xyz, r0.xyzx, r1.xxxx
  46: dp3 r1.x, l(1.000000, 1.000000, 1.000000, 0.000000), abs(r0.xyzx)
  47: div r0.xz, r0.xxzx, r1.xxxx
  48: ge r0.y, l(0), r0.y
  49: ge r1.xy, r0.xzxx, l(0, 0, 0, 0)
  50: add r1.zw, -abs(r0.zzzx), l(0.000000, 0.000000, 1.000000, 1.000000)
  51: movc r1.xy, r1.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  52: mul r1.xy, r1.xyxx, r1.zwzz
  53: movc r0.xy, r0.yyyy, r1.xyxx, r0.xzxx
  54: mad o3.xy, r0.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
  55: and r0.x, l(1023), cb1[r0.w + 5].z
  56: utof r0.x, r0.x
  57: mul o2.x, r0.x, l(0.000978)
  58: ubfe r0.xy, l(10, 10, 0, 0), l(10, 20, 0, 0), cb1[r0.w + 5].zzzz
  59: utof r0.xy, r0.xyxx
  60: mul o2.yz, r0.xxyx, l(0.000000, 0.000978, 0.000978, 0.000000)
  61: ushr r0.x, cb1[r0.w + 5].z, l(30)
  62: utof r0.x, r0.x
  63: mul o2.w, r0.x, l(0.333333)
  64: mov o0.xyzw, l(0, 0, 0, 0)
  65: mov o1.zw, l(0.000000, 0.000000, 1.000000, 0.400000)
  66: mov o3.zw, l(0.000000, 0.000000, 0.000000, 1.000000)
  67: mov o4.w, l(1.000000)
  68: ret
