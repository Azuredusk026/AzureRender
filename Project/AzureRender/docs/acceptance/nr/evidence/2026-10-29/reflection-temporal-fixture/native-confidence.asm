Shader hash 70ea2d94-a3b8e75d-2757f8fe-f6443021

cs_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[1], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u0
      dcl_input vThreadID.xy
      dcl_temps 4
      dcl_thread_group 8, 8, 1
   0: iadd r0.xyzw, vThreadID.xyxy, l(0, 1, 1, 1)
   1: itof r0.xyzw, r0.xyzw
   2: add r0.xyzw, r0.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
   3: mul r0.xyzw, r0.xyzw, cb0[0].zwzw
   4: sample_l(texture2d)(float,float,float,float) r0.z, r0.zwzz, t0.yzxw, s0, l(0)
   5: sample_l(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, l(0)
   6: iadd r1.xyzw, vThreadID.xyxy, l(1, 0, -1, 1)
   7: itof r1.xyzw, r1.xyzw
   8: add r1.xyzw, r1.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
   9: mul r1.xyzw, r1.xyzw, cb0[0].zwzw
  10: sample_l(texture2d)(float,float,float,float) r0.y, r1.zwzz, t0.yxzw, s0, l(0)
  11: sample_l(texture2d)(float,float,float,float) r0.w, r1.xyxx, t0.yzwx, s0, l(0)
  12: iadd r1.xyzw, vThreadID.xyxy, l(-1, -1, 0, -1)
  13: itof r1.xyzw, r1.xyzw
  14: add r1.xyzw, r1.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
  15: mul r1.xyzw, r1.xyzw, cb0[0].zwzw
  16: sample_l(texture2d)(float,float,float,float) r1.z, r1.zwzz, t0.yzxw, s0, l(0)
  17: sample_l(texture2d)(float,float,float,float) r1.x, r1.xyxx, t0.xyzw, s0, l(0)
  18: mul r1.y, r1.z, r1.z
  19: mad r1.y, r1.x, r1.x, r1.y
  20: iadd r2.xyzw, vThreadID.xyxy, l(1, -1, -1, 0)
  21: itof r2.xyzw, r2.xyzw
  22: add r2.xyzw, r2.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
  23: mul r2.xyzw, r2.xyzw, cb0[0].zwzw
  24: sample_l(texture2d)(float,float,float,float) r1.w, r2.xyxx, t0.yzwx, s0, l(0)
  25: sample_l(texture2d)(float,float,float,float) r2.x, r2.zwzz, t0.xyzw, s0, l(0)
  26: mad r1.y, r1.w, r1.w, r1.y
  27: mad r1.y, r2.x, r2.x, r1.y
  28: itof r2.yz, vThreadID.xxyx
  29: add r2.yz, r2.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000)
  30: mul r2.yz, r2.yyzy, cb0[0].zzwz
  31: sample_l(texture2d)(float,float,float,float) r2.w, r2.yzyy, t0.yzwx, s0, l(0)
  32: mad r1.y, r2.w, r2.w, r1.y
  33: mad r1.y, r0.w, r0.w, r1.y
  34: mad r1.y, r0.y, r0.y, r1.y
  35: mad r1.y, r0.x, r0.x, r1.y
  36: mad r1.y, r0.z, r0.z, r1.y
  37: add r3.x, r1.z, r1.x
  38: add r3.x, r1.w, r3.x
  39: add r3.x, r2.x, r3.x
  40: add r3.x, r2.w, r3.x
  41: add r3.x, r0.w, r3.x
  42: add r3.x, r0.y, r3.x
  43: add r3.x, r0.x, r3.x
  44: add r3.x, r0.z, r3.x
  45: mul r3.y, r3.x, l(0.111111)
  46: mul r3.y, r3.y, r3.y
  47: mad r1.y, r1.y, l(0.111111), -r3.y
  48: sqrt r1.y, r1.y
  49: mul r1.y, r1.y, l(1.250000)
  50: mad r3.y, r3.x, l(0.111111), -r1.y
  51: mad r1.y, r3.x, l(0.111111), r1.y
  52: min r3.x, r1.w, r1.x
  53: max r1.x, r1.w, r1.x
  54: min r1.w, r0.z, r0.y
  55: max r0.y, r0.z, r0.y
  56: max r0.y, r0.y, r1.x
  57: min r0.z, r1.w, r3.x
  58: min r1.x, r0.x, r0.w
  59: max r0.x, r0.x, r0.w
  60: min r0.w, r2.w, r2.x
  61: max r1.w, r2.w, r2.x
  62: max r0.x, r0.x, r1.w
  63: max r0.x, r0.x, r1.z
  64: min r0.w, r1.x, r0.w
  65: min r0.w, r0.w, r1.z
  66: min r0.z, r0.z, r0.w
  67: add r0.z, r0.z, r0.w
  68: mul r0.z, r0.z, l(0.500000)
  69: min r0.z, r3.y, r0.z
  70: max r0.y, r0.y, r0.x
  71: add r0.x, r0.y, r0.x
  72: mul r0.x, r0.x, l(0.500000)
  73: max r0.x, r1.y, r0.x
  74: add r0.y, r0.z, r0.x
  75: add r0.x, -r0.z, r0.x
  76: mul r0.x, r0.x, l(0.500000)
  77: sample_l(texture2d)(float,float,float,float) r0.zw, r2.yzyy, t1.zwxy, s0, l(0)
  78: add r1.xy, r0.zwzz, l(-0.500000, -0.500000, 0.000000, 0.000000)
  79: mad r0.zw, abs(r0.zzzw), l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
  80: mul r0.zw, r0.zzzw, r0.zzzw
  81: mul r0.zw, r0.zzzw, r0.zzzw
  82: lt r1.zw, l(0, 0, 0, 0), r1.xxxy
  83: lt r1.xy, r1.xyxx, l(0, 0, 0, 0)
  84: iadd r1.xy, -r1.zwzz, r1.xyxx
  85: itof r1.xy, r1.xyxx
  86: mad r0.zw, -r0.zzzw, r1.xxxy, r2.yyyz
  87: sample_l(texture2d)(float,float,float,float) r1.x, r2.yzyy, t2.xyzw, s0, l(0)
  88: mul r1.x, r1.x, l(0.970000)
  89: sample_l(texture2d)(float,float,float,float) r0.z, r0.zwzz, t3.yzxw, s0, l(0)
  90: mad r0.w, -r0.y, l(0.500000), r0.z
  91: div r0.x, r0.w, r0.x
  92: div r0.w, r0.w, abs(r0.x)
  93: lt r0.x, l(1.000000), abs(r0.x)
  94: mad r0.y, r0.y, l(0.500000), r0.w
  95: movc r0.x, r0.x, r0.y, r0.z
  96: add r0.x, -r2.w, r0.x
  97: mad r0.x, r1.x, r0.x, r2.w
  98: store_uav_typed u0.xyzw, vThreadID.xyyy, r0.xxxx
  99: ret
