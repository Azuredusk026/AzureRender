Shader hash b26ca68f-0bbaa1be-50f7e79b-5ed7d1ea

cs_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[1], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_uav_typed_texture2d (float,float,float,float) u0
      dcl_input vThreadID.xy
      dcl_temps 14
      dcl_thread_group 8, 8, 1
   0: iadd r0.xyzw, vThreadID.xyxy, l(0, 1, 1, 1)
   1: itof r0.xyzw, r0.xyzw
   2: add r0.xyzw, r0.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
   3: mul r0.xyzw, r0.xyzw, cb0[0].zwzw
   4: sample_l(texture2d)(float,float,float,float) r1.xyz, r0.zwzz, t1.xyzw, s0, l(0)
   5: sample_l(texture2d)(float,float,float,float) r0.xyz, r0.xyxx, t1.xyzw, s0, l(0)
   6: dp2 r0.w, r1.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
   7: add r2.y, r0.w, l(0.501961)
   8: dp3 r0.w, r1.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
   9: dp3 r2.x, r1.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  10: add r2.z, r0.w, l(0.501961)
  11: dp2 r0.w, r0.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  12: add r1.y, r0.w, l(0.501961)
  13: dp3 r0.w, r0.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  14: dp3 r1.x, r0.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  15: add r1.z, r0.w, l(0.501961)
  16: iadd r0.xyzw, vThreadID.xyxy, l(1, 0, -1, 1)
  17: itof r0.xyzw, r0.xyzw
  18: add r0.xyzw, r0.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
  19: mul r0.xyzw, r0.xyzw, cb0[0].zwzw
  20: sample_l(texture2d)(float,float,float,float) r3.xyz, r0.zwzz, t1.xyzw, s0, l(0)
  21: sample_l(texture2d)(float,float,float,float) r0.xyz, r0.xyxx, t1.xyzw, s0, l(0)
  22: dp2 r0.w, r3.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  23: add r4.y, r0.w, l(0.501961)
  24: dp3 r0.w, r3.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  25: dp3 r4.x, r3.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  26: add r4.z, r0.w, l(0.501961)
  27: dp2 r0.w, r0.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  28: add r3.y, r0.w, l(0.501961)
  29: dp3 r0.w, r0.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  30: dp3 r3.x, r0.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  31: add r3.z, r0.w, l(0.501961)
  32: iadd r0.xyzw, vThreadID.xyxy, l(1, -1, -1, 0)
  33: itof r0.xyzw, r0.xyzw
  34: add r0.xyzw, r0.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
  35: mul r0.xyzw, r0.xyzw, cb0[0].zwzw
  36: sample_l(texture2d)(float,float,float,float) r5.xyz, r0.zwzz, t1.xyzw, s0, l(0)
  37: sample_l(texture2d)(float,float,float,float) r0.xyz, r0.xyxx, t1.xyzw, s0, l(0)
  38: dp2 r0.w, r5.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  39: add r6.y, r0.w, l(0.501961)
  40: dp3 r0.w, r5.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  41: dp3 r6.x, r5.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  42: add r6.z, r0.w, l(0.501961)
  43: dp2 r0.w, r0.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  44: add r5.y, r0.w, l(0.501961)
  45: dp3 r0.w, r0.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  46: dp3 r5.x, r0.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  47: add r5.z, r0.w, l(0.501961)
  48: iadd r0.xyzw, vThreadID.xyxy, l(-1, -1, 0, -1)
  49: itof r0.xyzw, r0.xyzw
  50: add r0.xyzw, r0.xyzw, l(0.500000, 0.500000, 0.500000, 0.500000)
  51: mul r0.xyzw, r0.xyzw, cb0[0].zwzw
  52: sample_l(texture2d)(float,float,float,float) r7.xyz, r0.xyxx, t1.xyzw, s0, l(0)
  53: sample_l(texture2d)(float,float,float,float) r0.xyz, r0.zwzz, t1.xyzw, s0, l(0)
  54: dp2 r0.w, r7.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  55: add r8.y, r0.w, l(0.501961)
  56: dp3 r0.w, r7.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  57: dp3 r8.x, r7.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  58: add r8.z, r0.w, l(0.501961)
  59: dp2 r0.w, r0.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  60: add r7.y, r0.w, l(0.501961)
  61: dp3 r0.w, r0.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  62: dp3 r7.x, r0.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  63: add r7.z, r0.w, l(0.501961)
  64: mul r0.xyz, r7.xyzx, r7.xyzx
  65: mad r0.xyz, r8.xyzx, r8.xyzx, r0.xyzx
  66: mad r0.xyz, r5.xyzx, r5.xyzx, r0.xyzx
  67: mad r0.xyz, r6.xyzx, r6.xyzx, r0.xyzx
  68: itof r9.xy, vThreadID.xyxx
  69: add r9.xy, r9.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000)
  70: mul r9.xy, r9.xyxx, cb0[0].zwzz
  71: sample_l(texture2d)(float,float,float,float) r10.xyz, r9.xyxx, t1.xyzw, s0, l(0)
  72: dp2 r0.w, r10.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
  73: add r11.y, r0.w, l(0.501961)
  74: dp3 r0.w, r10.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
  75: add r11.z, r0.w, l(0.501961)
  76: dp3 r11.x, r10.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
  77: mad r0.xyz, r11.xyzx, r11.xyzx, r0.xyzx
  78: mad r0.xyz, r3.xyzx, r3.xyzx, r0.xyzx
  79: mad r0.xyz, r4.xyzx, r4.xyzx, r0.xyzx
  80: mad r0.xyz, r1.xyzx, r1.xyzx, r0.xyzx
  81: mad r0.xyz, r2.xyzx, r2.xyzx, r0.xyzx
  82: add r12.xyz, r7.xyzx, r8.xyzx
  83: add r12.xyz, r5.xyzx, r12.xyzx
  84: add r12.xyz, r6.xyzx, r12.xyzx
  85: add r12.xyz, r11.xyzx, r12.xyzx
  86: add r12.xyz, r3.xyzx, r12.xyzx
  87: add r12.xyz, r4.xyzx, r12.xyzx
  88: add r12.xyz, r1.xyzx, r12.xyzx
  89: add r12.xyz, r2.xyzx, r12.xyzx
  90: mul r13.xyz, r12.xyzx, l(0.111111, 0.111111, 0.111111, 0.000000)
  91: mul r13.xyz, r13.xyzx, r13.xyzx
  92: mad r0.xyz, r0.xyzx, l(0.111111, 0.111111, 0.111111, 0.000000), -r13.xyzx
  93: sqrt r0.xyz, r0.xyzx
  94: mul r0.xyz, r0.xyzx, l(1.250000, 1.250000, 1.250000, 0.000000)
  95: mad r13.xyz, r12.xyzx, l(0.111111, 0.111111, 0.111111, 0.000000), -r0.xyzx
  96: mad r0.xyz, r12.xyzx, l(0.111111, 0.111111, 0.111111, 0.000000), r0.xyzx
  97: min r12.xyz, r5.xyzx, r8.xyzx
  98: max r5.xyz, r5.xyzx, r8.xyzx
  99: min r8.xyz, r2.xyzx, r4.xyzx
 100: max r2.xyz, r2.xyzx, r4.xyzx
 101: max r2.xyz, r2.xyzx, r5.xyzx
 102: min r4.xyz, r8.xyzx, r12.xyzx
 103: min r5.xyz, r1.xyzx, r3.xyzx
 104: max r1.xyz, r1.xyzx, r3.xyzx
 105: min r3.xyz, r6.xyzx, r11.xyzx
 106: max r6.xyz, r6.xyzx, r11.xyzx
 107: max r1.xyz, r1.xyzx, r6.xyzx
 108: max r1.xyz, r1.xyzx, r7.xyzx
 109: min r3.xyz, r5.xyzx, r3.xyzx
 110: min r3.xyz, r3.xyzx, r7.xyzx
 111: min r4.xyz, r4.xyzx, r3.xyzx
 112: add r3.xyz, r3.xyzx, r4.xyzx
 113: mul r3.xyz, r3.xyzx, l(0.500000, 0.500000, 0.500000, 0.000000)
 114: min r3.xyz, r13.xyzx, r3.xyzx
 115: max r2.xyz, r2.xyzx, r1.xyzx
 116: add r1.xyz, r1.xyzx, r2.xyzx
 117: mul r1.xyz, r1.xyzx, l(0.500000, 0.500000, 0.500000, 0.000000)
 118: max r0.xyz, r0.xyzx, r1.xyzx
 119: add r1.xyz, -r3.xyzx, r0.xyzx
 120: add r0.xyz, r3.xyzx, r0.xyzx
 121: mul r1.xyz, r1.xyzx, l(0.500000, 0.500000, 0.500000, 0.000000)
 122: sample_l(texture2d)(float,float,float,float) r2.xy, r9.xyxx, t0.xyzw, s0, l(0)
 123: add r2.zw, r2.xxxy, l(0.000000, 0.000000, -0.500000, -0.500000)
 124: mad r2.xy, abs(r2.xyxx), l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 125: mul r2.xy, r2.xyxx, r2.xyxx
 126: mul r2.xy, r2.xyxx, r2.xyxx
 127: lt r3.xy, l(0, 0, 0, 0), r2.zwzz
 128: lt r2.zw, r2.zzzw, l(0, 0, 0, 0)
 129: iadd r2.zw, -r3.xxxy, r2.zzzw
 130: itof r2.zw, r2.zzzw
 131: mad r2.xy, -r2.xyxx, r2.zwzz, r9.xyxx
 132: sample_l(texture2d)(float,float,float,float) r0.w, r9.xyxx, t2.yzwx, s0, l(0)
 133: mul r0.w, r0.w, l(0.970000)
 134: sample_l(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t3.xyzw, s0, l(0)
 135: dp2 r1.w, r2.xzxx, l(0.500000, -0.500000, 0.000000, 0.000000)
 136: add r3.y, r1.w, l(0.501961)
 137: dp3 r1.w, r2.xzyx, l(-0.250000, -0.250000, 0.500000, 0.000000)
 138: dp3 r3.x, r2.xzyx, l(0.250000, 0.250000, 0.500000, 0.000000)
 139: add r3.z, r1.w, l(0.501961)
 140: mad r2.xyz, -r0.xyzx, l(0.500000, 0.500000, 0.500000, 0.000000), r3.xyzx
 141: div r1.xyz, r2.xyzx, r1.xyzx
 142: max r1.y, abs(r1.z), abs(r1.y)
 143: max r1.x, r1.y, abs(r1.x)
 144: div r1.yzw, r2.xxyz, r1.xxxx
 145: lt r1.x, l(1.000000), r1.x
 146: mad r0.xyz, r0.xyzx, l(0.500000, 0.500000, 0.500000, 0.000000), r1.yzwy
 147: movc r0.xyz, r1.xxxx, r0.xyzx, r3.xyzx
 148: add r0.yz, r0.yyzy, l(0.000000, -0.501961, -0.501961, 0.000000)
 149: add r1.xy, r0.yzyy, r0.xxxx
 150: add r1.xw, -r0.zzzz, r1.xxxx
 151: add r0.y, -r0.y, r0.x
 152: add r1.z, -r0.z, r0.y
 153: add r1.xyzw, -r10.xyzx, r1.xyzw
 154: mad r0.xyzw, r0.wwww, r1.xyzw, r10.xyzx
 155: max r0.xyzw, r0.xyzw, l(0.001000, 0.001000, 0.001000, 0.001000)
 156: store_uav_typed u0.xyzw, vThreadID.xyyy, r0.xyzw
 157: ret
