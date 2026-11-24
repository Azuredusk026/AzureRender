Shader hash 08ed673f-3882d333-656eab4b-11a95587

ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[27], immediateIndexed
      dcl_constantbuffer cb1[4085], dynamicIndexed
      dcl_constantbuffer cb2[13], immediateIndexed
      dcl_constantbuffer cb3[1], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_input_ps linear v1.xy
      dcl_input_ps linear v1.zw
      dcl_input_ps linear v2.xyz
      dcl_input_ps linear v3.xyzw
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
   0: mov o0.xyzw, l(0.000000, 0.000000, 0.000000, 0.500000)
   1: max r0.x, v5.z, l(0.000000)
   2: div r0.xy, v5.xyxx, r0.xxxx
   3: max r0.z, v6.z, l(0.000000)
   4: div r0.zw, v6.xxxy, r0.zzzz
   5: add r0.xy, -r0.zwzz, r0.xyxx
   6: mov r0.z, -r0.y
   7: lt r1.xy, l(0, 0, 0, 0), r0.xzxx
   8: lt r0.zw, r0.xxxz, l(0, 0, 0, 0)
   9: mul r0.xy, r0.xyxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  10: sqrt r0.xy, abs(r0.xyxx)
  11: sqrt r0.xy, r0.xyxx
  12: iadd r0.zw, -r1.xxxy, r0.zzzw
  13: itof r0.zw, r0.zzzw
  14: mul r0.xy, r0.zwzz, r0.xyxx
  15: mad r0.xy, r0.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
  16: add r0.zw, -r0.xxxy, l(0.000000, 0.000000, 0.500000, 0.500000)
  17: ishl r1.x, v7.x, l(4)
  18: max r1.x, cb1[r1.x + 4].z, cb1[r1.x + 4].y
  19: add r1.x, r1.x, l(-0.100000)
  20: lt r1.y, l(0), r1.x
  21: lt r1.x, r1.x, l(0)
  22: iadd r1.x, -r1.y, r1.x
  23: itof r1.x, r1.x
  24: mov_sat r1.x, r1.x
  25: mad o1.xy, r1.xxxx, r0.zwzz, r0.xyxx
  26: lt r0.x, l(0), r1.x
  27: mul o1.w, r1.x, l(0.700000)
  28: movc o1.z, r0.x, l(1.000000), cb2[7].x
  29: lt r0.x, l(0.500000), cb3[0].x
  30: and r0.x, r0.x, cb3[0].w
  31: ftou r0.x, r0.x
  32: ushr r0.y, r0.x, l(2)
  33: and r0.x, r0.x, l(3)
  34: utof r0.x, r0.x
  35: mul o3.w, r0.x, l(0.333333)
  36: utof r0.x, r0.y
  37: mul o2.w, r0.x, l(0.333333)
  38: add_sat r0.x, l(-1.000000), cb2[3].w
  39: add r0.yz, -v1.xxyx, v1.zzwz
  40: mad r1.xy, cb2[3].xxxx, r0.yzyy, v1.xyxx
  41: mad r0.yz, cb2[2].wwww, r0.yyzy, v1.xxyx
  42: mad r0.yz, r0.yyzy, cb2[11].xxyx, cb2[11].zzwz
  43: sample_b(texture2d)(float,float,float,float) r0.yzw, r0.yzyy, t0.wxyz, s0, cb0[26].x
  44: mul r0.yzw, r0.yyzw, cb2[8].xxyz
  45: mul_sat r0.yzw, r0.yyzw, cb2[4].zzzz
  46: mad r1.xy, r1.xyxx, cb2[12].xyxx, cb2[12].zwzz
  47: sample_b(texture2d)(float,float,float,float) r2.xyz, r1.xyxx, t2.xyzw, s2, cb0[26].x
  48: add r1.z, -r2.x, cb2[4].y
  49: mad r0.x, r0.x, r1.z, r2.x
  50: mul r1.z, r0.x, cb2[5].y
  51: mov o2.x, r0.x
  52: add r0.x, -cb2[0].z, cb2[0].w
  53: mad r0.x, r2.y, r0.x, cb2[0].z
  54: add r1.w, r2.z, l(-1.000000)
  55: mad o2.y, cb2[1].x, r1.w, l(1.000000)
  56: mad r1.z, cb2[4].w, r0.x, r1.z
  57: mov o3.z, r0.x
  58: add_sat r0.x, r1.z, cb2[5].x
  59: mad r0.x, r0.x, l(0.950000), l(0.050000)
  60: add r1.z, l(1.000000), -cb2[7].y
  61: mul o2.z, r0.x, r1.z
  62: add r0.x, cb0[26].x, cb2[3].y
  63: sample_b(texture2d)(float,float,float,float) r1.xyz, r1.xyxx, t1.xywz, s1, r0.x
  64: mul r1.x, r1.x, r1.z
  65: mad r1.xy, r1.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  66: dp2 r0.x, r1.xyxx, r1.xyxx
  67: mul r1.xy, r1.xyxx, cb2[0].xxxx
  68: min r0.x, r0.x, l(1.000000)
  69: add r0.x, -r0.x, l(1.000000)
  70: sqrt r0.x, r0.x
  71: max r0.x, r0.x, l(0.000000)
  72: lt r1.z, l(0), cb2[1].w
  73: movc r1.z, r1.z, l(-1.000000), l(1.000000)
  74: movc r1.z, v8.x, l(1.000000), r1.z
  75: mul r0.x, r0.x, r1.z
  76: lt r1.z, l(0), v3.w
  77: movc r1.z, r1.z, l(1.000000), l(-1.000000)
  78: mul r2.xyz, v2.zxyz, v3.yzxy
  79: mad r2.xyz, v2.yzxy, v3.zxyz, -r2.xyzx
  80: mul r2.xyz, r1.zzzz, r2.xyzx
  81: mul r1.yzw, r1.yyyy, r2.xxyz
  82: mad r1.xyz, v3.xyzx, r1.xxxx, r1.yzwy
  83: mad r1.xyz, v2.xyzx, r0.xxxx, r1.xyzx
  84: dp3 r0.x, r1.xyzx, r1.xyzx
  85: max r0.x, r0.x, l(0.000000)
  86: rsq r0.x, r0.x
  87: mul r1.xyz, r0.xxxx, r1.xyzx
  88: dp3 r0.x, r1.xyzx, r1.xyzx
  89: rsq r0.x, r0.x
  90: mul r1.xyz, r0.xxxx, r1.xyzx
  91: dp3 r0.x, l(1.000000, 1.000000, 1.000000, 0.000000), abs(r1.xyzx)
  92: div r1.xz, r1.xxzx, r0.xxxx
  93: ge r0.x, l(0), r1.y
  94: ge r1.yw, r1.xxxz, l(0, 0, 0, 0)
  95: movc r1.yw, r1.yyyw, l(0.000000, 1.000000, 0.000000, 1.000000), l(0.000000, -1.000000, 0.000000, -1.000000)
  96: add r2.xy, -abs(r1.zxzz), l(1.000000, 1.000000, 0.000000, 0.000000)
  97: mul r1.yw, r1.yyyw, r2.xxxy
  98: movc r1.xy, r0.xxxx, r1.ywyy, r1.xzxx
  99: mad o3.xy, r1.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
 100: add r1.xyz, -r0.yzwy, cb2[8].xyzx
 101: mad o4.xyz, cb2[4].xxxx, r1.xyzx, r0.yzwy
 102: mov o4.w, l(0)
 103: ret
