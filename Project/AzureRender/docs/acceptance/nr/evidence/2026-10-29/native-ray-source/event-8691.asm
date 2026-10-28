Shader hash 9c67e92a-7e8f58c4-92fd8dec-b690ce10

cs_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[28], immediateIndexed
      dcl_constantbuffer cb1[3], immediateIndexed
      dcl_constantbuffer cb2[7], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (uint,uint,uint,uint) t4
      dcl_uav_raw u0
      dcl_uav_structured u1, 4
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u2
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u3
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u4
      dcl_input vThreadIDInGroupFlattened
      dcl_input vThreadGroupID.xy
      dcl_input vThreadID.xy
      dcl_temps 6
      dcl_tgsm_structured g0, 4, 64
      dcl_thread_group 8, 8, 1
   0: utof r0.xy, vThreadID.xyxx
   1: add r0.xy, r0.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000)
   2: mul r0.xy, r0.xyxx, cb2[0].zwzz
   3: sample_l(texture2d)(float,float,float,float) r0.z, r0.xyxx, t1.yzxw, s0, l(0)
   4: ftou r1.xy, cb2[0].xyxx
   5: ult r1.xy, vThreadID.xyxx, r1.xyxx
   6: and r0.w, r1.y, r1.x
   7: ne r1.x, r0.z, l(0)
   8: and r0.w, r0.w, r1.x
   9: if_nz r0.w
  10:   sample_l(texture2d)(float,float,float,float) r0.w, r0.xyxx, t0.yzwx, s0, l(0)
  11:   mad r1.xy, r0.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  12:   mul r2.xyzw, -r1.yyyy, cb0[25].xyzw
  13:   mad r2.xyzw, cb0[24].xyzw, r1.xxxx, r2.xyzw
  14:   mad r3.xyzw, cb0[26].xyzw, r0.wwww, r2.xyzw
  15:   add r3.xyzw, r3.xyzw, cb0[27].xyzw
  16:   mad r2.xyzw, cb0[26].xyzw, r0.zzzz, r2.xyzw
  17:   add r2.xyzw, r2.xyzw, cb0[27].xyzw
  18:   sample_l(texture2d)(float,float,float,float) r4.xyz, r0.xyxx, t2.xyzw, s0, l(0)
  19:   sample_l(texture2d)(float,float,float,float) r5.xyz, r0.xyxx, t3.xyzw, s0, l(0)
  20:   div r3.xyz, r3.xyzx, r3.wwww
  21:   div r2.xyz, r2.xyzx, r2.wwww
  22:   add r2.xyz, -r2.xyzx, r3.xyzx
  23:   dp3 r1.z, r2.xyzx, r2.xyzx
  24:   sqrt r1.z, r1.z
  25:   mul r1.z, r1.z, l(10.000000)
  26:   min r1.z, r1.z, l(1.000000)
  27:   add r2.xyz, -r4.xyzx, r5.xyzx
  28:   mad r2.xyz, r1.zzzz, r2.zxyz, r4.zxyz
  29:   mul r3.xyzw, r1.yyyy, cb0[13].xyzw
  30:   mad r1.xyzw, cb0[12].xyzw, r1.xxxx, r3.xyzw
  31:   mad r1.xyzw, cb0[14].xyzw, r0.zzzz, r1.xyzw
  32:   add r1.xyzw, r1.xyzw, cb0[15].xyzw
  33:   div r1.xyz, r1.xyzx, r1.wwww
  34:   mad r3.xy, r2.yzyy, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  35:   dp2 r3.z, l(1.000000, 1.000000, 0.000000, 0.000000), abs(r3.xyxx)
  36:   add r4.y, -r3.z, l(1.000000)
  37:   lt r3.z, r4.y, l(0)
  38:   ge r5.xy, r3.xyxx, l(0, 0, 0, 0)
  39:   add r5.zw, -abs(r3.yyyx), l(0.000000, 0.000000, 1.000000, 1.000000)
  40:   movc r5.xy, r5.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  41:   mul r5.xy, r5.xyxx, r5.zwzz
  42:   movc r4.xz, r3.zzzz, r5.xxyx, r3.xxyx
  43:   dp3 r3.x, r4.xyzx, r4.xyzx
  44:   rsq r3.x, r3.x
  45:   mul r3.xyz, r3.xxxx, r4.xyzx
  46:   dp3 r3.w, l(1.000000, 1.000000, 1.000000, 0.000000), abs(r3.xyzx)
  47:   div r4.xy, r3.xzxx, r3.wwww
  48:   ge r3.w, l(0), r3.y
  49:   ge r4.zw, r4.xxxy, l(0, 0, 0, 0)
  50:   add r5.xy, -abs(r4.yxyy), l(1.000000, 1.000000, 0.000000, 0.000000)
  51:   movc r4.zw, r4.zzzw, l(0.000000, 0.000000, 1.000000, 1.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
  52:   mul r4.zw, r4.zzzw, r5.xxxy
  53:   movc r4.xy, r3.wwww, r4.zwzz, r4.xyxx
  54:   mad r0.z, cb1[2].z, r0.z, cb1[2].w
  55:   div r0.z, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  56:   add r3.w, r2.x, l(-0.200000)
  57:   mul_sat r3.w, r3.w, l(2.500000)
  58:   mad r4.z, r3.w, l(-2.000000), l(3.000000)
  59:   mul r3.w, r3.w, r3.w
  60:   mad r3.w, -r4.z, r3.w, l(1.000000)
  61:   mul r5.xyz, r3.yyyy, cb0[1].xyzx
  62:   mad r5.xyz, cb0[0].xyzx, r3.xxxx, r5.xyzx
  63:   mad r3.xyz, cb0[2].xyzx, r3.zzzz, r5.xyzx
  64:   mov r1.w, -r1.y
  65:   dp3 r1.y, r1.xzwx, r1.xzwx
  66:   rsq r1.y, r1.y
  67:   mul r1.xyz, r1.yyyy, r1.xwzx
  68:   dp3 r1.x, r3.xyzx, r1.xyzx
  69:   mad_sat r1.x, abs(r1.x), cb2[1].z, cb2[1].w
  70:   add r1.x, -r1.x, l(1.000000)
  71:   mul r1.x, r1.x, r3.w
  72:   lt r1.yzw, r2.xxxx, l(0.000000, 0.100000, 0.300000, 0.600000)
  73:   add r3.x, -r0.z, l(100.000000)
  74:   mul_sat r3.x, r3.x, l(0.100000)
  75:   movc r1.y, r1.y, l(1.000000), r3.x
  76:   mul r1.x, r1.y, r1.x
  77:   mul r0.xy, r0.xyxx, cb1[0].xyxx
  78:   ftoi r3.xy, r0.xyxx
  79:   mov r3.zw, l(0, 0, 0, 0)
  80:   ld_indexable(texture2d)(uint,uint,uint,uint) r0.x, r3.xyzw, t4.yxzw
  81:   and r0.x, r0.x, l(4)
  82:   mad r0.y, cb1[2].z, r0.w, cb1[2].w
  83:   div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  84:   add r0.y, -r0.z, r0.y
  85:   ge r0.y, l(0), r0.y
  86:   movc r0.y, r0.y, l(0), r1.x
  87:   movc r0.x, r0.x, r0.y, r1.x
  88:   ge r0.y, r0.x, l(0.001000)
  89:   and r0.z, r1.w, l(0.007813)
  90:   movc r0.z, r1.z, l(1.000000), r0.z
  91:   and r0.y, r0.z, r0.y
  92:   mad r2.yz, r4.xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 0.500000, 0.500000, 0.000000)
  93: else
  94:   mov r2.xyz, l(0, 0, 0, 0)
  95:   mov r0.xy, l(0, 0, 0, 0)
  96: endif
  97: store_uav_typed u2.xyzw, vThreadID.xyyy, r0.xxxx
  98: store_uav_typed u3.xyzw, vThreadID.xyyy, r0.yyyy
  99: mov r2.w, l(0)
 100: store_uav_typed u4.xyzw, vThreadID.xyyy, r2.yzxw
 101: store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.y
 102: sync_g_t
 103: ult r1.xyzw, vThreadIDInGroupFlattened.xxxx, l(32, 16, 8, 4)
 104: if_nz r1.x
 105:   iadd r0.x, vThreadIDInGroupFlattened.x, l(32)
 106:   ld_structured r0.x, r0.x, l(0), g0.xxxx
 107:   add r0.x, r0.x, r0.y
 108:   store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.x
 109: endif
 110: sync_g_t
 111: if_nz r1.y
 112:   iadd r0.x, vThreadIDInGroupFlattened.x, l(16)
 113:   ld_structured r0.x, r0.x, l(0), g0.xxxx
 114:   ld_structured r0.y, vThreadIDInGroupFlattened.x, l(0), g0.xxxx
 115:   add r0.x, r0.x, r0.y
 116:   store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.x
 117: endif
 118: sync_g_t
 119: if_nz r1.z
 120:   iadd r0.x, vThreadIDInGroupFlattened.x, l(8)
 121:   ld_structured r0.x, r0.x, l(0), g0.xxxx
 122:   ld_structured r0.y, vThreadIDInGroupFlattened.x, l(0), g0.xxxx
 123:   add r0.x, r0.x, r0.y
 124:   store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.x
 125: endif
 126: sync_g_t
 127: if_nz r1.w
 128:   iadd r0.x, vThreadIDInGroupFlattened.x, l(4)
 129:   ld_structured r0.x, r0.x, l(0), g0.xxxx
 130:   ld_structured r0.y, vThreadIDInGroupFlattened.x, l(0), g0.xxxx
 131:   add r0.x, r0.x, r0.y
 132:   store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.x
 133: endif
 134: sync_g_t
 135: ult r0.xy, vThreadIDInGroupFlattened.xxxx, l(2, 1, 0, 0)
 136: if_nz r0.x
 137:   iadd r0.x, vThreadIDInGroupFlattened.x, l(2)
 138:   ld_structured r0.x, r0.x, l(0), g0.xxxx
 139:   ld_structured r0.z, vThreadIDInGroupFlattened.x, l(0), g0.xxxx
 140:   add r0.x, r0.x, r0.z
 141:   store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.x
 142: endif
 143: sync_g_t
 144: if_nz r0.y
 145:   ld_structured r0.x, l(1), l(0), g0.xxxx
 146:   ld_structured r0.y, vThreadIDInGroupFlattened.x, l(0), g0.xxxx
 147:   add r0.x, r0.x, r0.y
 148:   store_structured g0.x, vThreadIDInGroupFlattened.x, l(0), r0.x
 149: endif
 150: sync_g_t
 151: if_z vThreadIDInGroupFlattened.x
 152:   ld_structured r0.x, l(0), l(0), g0.xxxx
 153:   imad r0.y, vThreadGroupID.x, l(0x00010000), vThreadGroupID.y
 154:   ge r0.z, r0.x, l(1.000000)
 155:   if_nz r0.z
 156:     ftou r0.z, cb2[6].x
 157:     bfi r0.z, l(30), l(2), r0.z, l(0)
 158:     imm_atomic_iadd r1.x, u0, r0.z, l(1)
 159:     store_structured u1.x, r1.x, l(0), r0.y
 160:   else
 161:     ge r0.x, r0.x, l(0.007813)
 162:     if_nz r0.x
 163:       ftou r0.xz, cb2[6].xxzx
 164:       ishl r0.x, r0.x, l(2)
 165:       iadd r0.x, r0.x, l(12)
 166:       imm_atomic_iadd r1.x, u0, r0.x, l(1)
 167:       iadd r0.x, r0.z, r1.x
 168:       store_structured u1.x, r0.x, l(0), r0.y
 169:     else
 170:       ftou r0.xz, cb2[6].xxzx
 171:       ishl r0.xz, r0.xxzx, l(2, 0, 1, 0)
 172:       iadd r0.xz, r0.xxzx, l(24, 0, -1, 0)
 173:       imm_atomic_iadd r1.x, u0, r0.x, l(1)
 174:       iadd r0.x, r0.z, -r1.x
 175:       store_structured u1.x, r0.x, l(0), r0.y
 176:     endif
 177:   endif
 178:   ftou r0.xy, cb2[6].xzxx
 179:   ishl r0.x, r0.x, l(2)
 180:   iadd r0.x, r0.x, l(36)
 181:   imm_atomic_iadd r1.x, u0, r0.x, l(1)
 182:   iadd r0.x, r0.y, l(-1)
 183:   ieq r0.x, r0.x, r1.x
 184:   if_nz r0.x
 185:     ftou r0.x, cb2[6].y
 186:     ishl r0.y, r0.x, l(2)
 187:     bfi r0.x, l(30), l(2), r0.x, l(0)
 188:     store_raw u0.xyz, r0.x, l(0, 1, 1, 0)
 189:     iadd r0.xz, r0.yyyy, l(12, 0, 24, 0)
 190:     store_raw u0.xyz, r0.x, l(0, 1, 1, 0)
 191:     store_raw u0.xyzw, r0.z, l(0, 1, 1, 0)
 192:   endif
 193: endif
 194: ret
