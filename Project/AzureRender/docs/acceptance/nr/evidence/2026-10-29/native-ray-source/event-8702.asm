Shader hash cbc21e4c-9cef7b57-ca278f12-a6bc9b92

cs_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[65], immediateIndexed
      dcl_constantbuffer cb1[3], immediateIndexed
      dcl_constantbuffer cb2[6], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_structured t0, 4
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_uav_typed_texture2d (float,float,float,float) u0
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u1
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u2
      dcl_input vThreadGroupID.x
      dcl_input vThreadIDInGroup.xy
      dcl_temps 14
      dcl_thread_group 8, 8, 1
   0: ld_structured_indexable(structured_buffer, stride=4)(mixed,mixed,mixed,mixed) r0.x, vThreadGroupID.x, l(0), t0.xxxx
   1: ushr r1.x, r0.x, l(16)
   2: and r1.y, r0.x, l(0x0000ffff)
   3: imad r0.xy, r1.xyxx, l(8, 8, 0, 0), vThreadIDInGroup.xyxx
   4: ftou r1.xy, cb2[0].xyxx
   5: uge r1.xy, r0.xyxx, r1.xyxx
   6: or r1.x, r1.y, r1.x
   7: if_z r1.x
   8:   ld_uav_typed(texture2d)(unorm,unorm,unorm,unorm) r1.x, r0.xyyy, u2.xyzw
   9:   itof r1.yz, r0.xxyx
  10:   add r1.yz, r1.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000)
  11:   mul r2.xy, r1.yzyy, cb2[0].zwzz
  12:   lt r1.w, l(0.001000), r1.x
  13:   if_nz r1.w
  14:     utof r2.zw, r0.xxxy
  15:     add r2.zw, r2.zzzw, l(0.000000, 0.000000, 0.500000, 0.500000)
  16:     mul r2.zw, r2.zzzw, cb2[0].zzzw
  17:     sample_l(texture2d)(float,float,float,float) r1.w, r2.zwzz, t5.yzwx, s0, l(0)
  18:     lt r1.w, l(0.500000), r1.w
  19:     mul r2.z, l(0.125000), cb2[3].x
  20:     round_pi r2.z, r2.z
  21:     max r2.z, r2.z, l(4.000000)
  22:     movc r1.w, r1.w, cb2[3].x, r2.z
  23:     sample_l(texture2d)(float,float,float,float) r3.z, r2.xyxx, t3.yzxw, s0, l(0)
  24:     mad r3.xy, r2.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  25:     mul r4.xyzw, r3.yyyy, cb0[13].xyzw
  26:     mad r4.xyzw, cb0[12].xyzw, r3.xxxx, r4.xyzw
  27:     mad r4.xyzw, cb0[14].xyzw, r3.zzzz, r4.xyzw
  28:     add r4.xyzw, r4.xyzw, cb0[15].xyzw
  29:     div r4.xyz, r4.xyzx, r4.wwww
  30:     mov r0.zw, l(0, 0, 0, 0)
  31:     ld_indexable(texture2d)(float,float,float,float) r5.xyz, r0.xyzw, t6.xyzw
  32:     mad r0.zw, r5.xxxy, l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
  33:     dp2 r2.z, l(1.000000, 1.000000, 0.000000, 0.000000), abs(r0.zwzz)
  34:     add r6.y, -r2.z, l(1.000000)
  35:     lt r2.z, r6.y, l(0)
  36:     ge r5.xy, r0.zwzz, l(0, 0, 0, 0)
  37:     add r7.xy, -abs(r0.wzww), l(1.000000, 1.000000, 0.000000, 0.000000)
  38:     movc r5.xy, r5.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  39:     mul r5.xy, r5.xyxx, r7.xyxx
  40:     movc r6.xz, r2.zzzz, r5.xxyx, r0.zzwz
  41:     dp3 r0.z, r6.xyzx, r6.xyzx
  42:     rsq r0.z, r0.z
  43:     mul r5.xyw, r0.zzzz, r6.xyxz
  44:     mul r0.z, r5.z, r5.z
  45:     mul r0.z, r0.z, r0.z
  46:     mov r4.w, -r4.y
  47:     dp3 r0.w, r4.xzwx, r4.xzwx
  48:     rsq r0.w, r0.w
  49:     mul r4.xyz, r0.wwww, r4.xwzx
  50:     mul r6.xyz, r5.yyyy, cb0[1].xyzx
  51:     mad r6.xyz, cb0[0].xyzx, r5.xxxx, r6.xyzx
  52:     mad r6.xyz, cb0[2].xyzx, r5.wwww, r6.xyzx
  53:     dp3 r0.w, r4.xyzx, r6.xyzx
  54:     add r0.w, r0.w, r0.w
  55:     mad r4.xyz, r6.xyzx, -r0.wwww, r4.xyzx
  56:     mul r6.xyzw, r4.yyyy, cb0[9].xyzw
  57:     mad r6.xyzw, cb0[8].xyzw, r4.xxxx, r6.xyzw
  58:     mad r4.xyzw, cb0[10].xyzw, r4.zzzz, r6.xyzw
  59:     mov r3.w, l(1.000000)
  60:     mad r4.xyzw, r4.xyzw, l(1.000000, -1.000000, 1.000000, 1.000000), r3.xyzw
  61:     div r4.xyz, r4.xyzx, r4.wwww
  62:     add r4.xyz, -r3.xyzx, r4.xyzx
  63:     dp2 r0.w, r4.xyxx, r4.xyxx
  64:     sqrt r0.w, r0.w
  65:     mul r2.z, r0.w, l(0.500000)
  66:     mad r6.xy, r3.xyxx, r2.zzzz, r4.xyxx
  67:     mad r6.xy, -r0.wwww, l(0.500000, 0.500000, 0.000000, 0.000000), abs(r6.xyxx)
  68:     max r6.xy, r6.xyxx, l(0, 0, 0, 0)
  69:     div r6.xy, r6.xyxx, abs(r4.xyxx)
  70:     add r6.xy, -r6.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000)
  71:     min r0.w, r6.y, r6.x
  72:     div r0.w, r0.w, r2.z
  73:     mul r4.xyz, r0.wwww, r4.xyzx
  74:     div r0.w, abs(r4.z), r1.w
  75:     max r0.w, r0.w, l(0.000100)
  76:     div r2.z, l(1.000000, 1.000000, 1.000000, 1.000000), r1.w
  77:     mul r4.xyz, r4.xyzx, l(0.500000, 0.500000, 1.000000, 0.000000)
  78:     mul r4.xyz, r2.zzzz, r4.xyzx
  79:     mul r2.zw, r4.xxxy, cb2[0].xxxy
  80:     max r2.z, abs(r2.w), abs(r2.z)
  81:     lt r2.w, r2.z, l(1.000000)
  82:     add r2.z, r2.z, l(0.001000)
  83:     div r2.z, l(1.000000, 1.000000, 1.000000, 1.000000), r2.z
  84:     round_z r3.w, cb2[1].x
  85:     mad r1.yz, r3.wwww, l(0.000000, 2.083000, 4.867000, 0.000000), r1.yyzy
  86:     dp2 r1.y, r1.yzyy, l(0.067111, 0.005837, 0.000000, 0.000000)
  87:     frc r1.y, r1.y
  88:     mul r1.y, r1.y, l(52.982918)
  89:     frc r1.y, r1.y
  90:     movc r1.y, r2.w, r2.z, r1.y
  91:     mad r6.xyz, r3.xyzx, l(0.500000, 0.500000, 1.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
  92:     mad r6.xyz, r4.xyzx, r1.yyyy, r6.xyzx
  93:     sample_l(texture2d)(float,float,float,float) r1.y, r6.xyxx, t3.yxzw, s0, l(0)
  94:     add r1.y, -r1.y, r6.z
  95:     mov r7.xyz, l(0, 0, 0, 0)
  96:     mov r8.xyz, l(0, 0, 0, 0)
  97:     mov r9.xyz, r6.xyzx
  98:     mov r1.z, l(0)
  99:     mov r2.z, l(0)
 100:     mov r2.w, r1.y
 101:     loop
 102:       lt r3.w, r2.z, r1.w
 103:       if_nz r3.w
 104:         mad r10.xyzw, r4.xyxy, l(1.000000, 1.000000, 2.000000, 2.000000), r9.xyxy
 105:         mad r11.xyzw, r4.xyxy, l(3.000000, 3.000000, 4.000000, 4.000000), r9.xyxy
 106:         mad r12.xyzw, r4.zzzz, l(1.000000, 2.000000, 3.000000, 4.000000), r9.zzzz
 107:         sample_l(texture2d)(float,float,float,float) r13.x, r10.xyxx, t3.xyzw, s0, l(0)
 108:         sample_l(texture2d)(float,float,float,float) r13.y, r10.zwzz, t3.yxzw, s0, l(0)
 109:         sample_l(texture2d)(float,float,float,float) r13.z, r11.xyxx, t3.yzxw, s0, l(0)
 110:         sample_l(texture2d)(float,float,float,float) r13.w, r11.zwzz, t3.yzwx, s0, l(0)
 111:         add r10.xyzw, r12.xyzw, -r13.xyzw
 112:         add r11.xyzw, r0.wwww, r10.xyzw
 113:         lt r11.xyzw, abs(r11.xyzw), r0.wwww
 114:         or r12.xy, r11.zwzz, r11.xyxx
 115:         or r3.w, r12.y, r12.x
 116:         if_nz r3.w
 117:           movc r12.xy, r11.zzzz, r10.zyzz, r10.wzww
 118:           movc r10.yz, r11.yyyy, r10.yyxy, r12.xxyx
 119:           movc r3.w, r11.x, r10.x, r10.y
 120:           movc r4.w, r11.x, r2.w, r10.z
 121:           movc r5.z, r11.z, l(2.000000), l(3.000000)
 122:           movc r5.z, r11.y, l(1.000000), r5.z
 123:           movc r5.z, r11.x, l(0), r5.z
 124:           add r3.w, -r3.w, r4.w
 125:           div_sat r3.w, r4.w, r3.w
 126:           add r3.w, r3.w, r5.z
 127:           mad r10.xyz, r4.xyzx, r3.wwww, r9.xyzx
 128:           mov r1.z, l(-1)
 129:           mov r8.xyz, r10.xyzx
 130:           break
 131:         endif
 132:         mad r9.xyz, r4.xyzx, l(4.000000, 4.000000, 4.000000, 0.000000), r9.xyzx
 133:         add r2.z, r2.z, l(4.000000)
 134:         mov r7.xyz, r9.xyzx
 135:         mov r1.z, l(0)
 136:         mov r2.w, r10.w
 137:         continue
 138:       else
 139:         mov r8.xyz, r7.xyzx
 140:         mov r1.z, l(0)
 141:         break
 142:       endif
 143:       mov r1.z, l(0)
 144:     endloop
 145:     if_nz r1.z
 146:       max r0.w, r8.y, r8.x
 147:       add r0.w, -r0.w, l(1.000000)
 148:       min r1.y, r8.y, r8.x
 149:       min r0.w, r0.w, r1.y
 150:       lt r1.y, cb2[1].y, r0.w
 151:       div_sat r0.w, r0.w, cb2[1].y
 152:       movc r0.w, r1.y, l(1.000000), r0.w
 153:       mul r0.w, r0.w, r1.x
 154:       sample_l(texture2d)(float,float,float,float) r1.yz, r8.xyxx, t4.zxyw, s0, l(0)
 155:       mad r2.zw, abs(r1.yyyz), l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
 156:       mul r2.zw, r2.zzzw, r2.zzzw
 157:       mul r2.zw, r2.zzzw, r2.zzzw
 158:       add r1.yz, r1.yyzy, l(0.000000, -0.500000, -0.500000, 0.000000)
 159:       lt r4.xy, l(0, 0, 0, 0), r1.yzyy
 160:       lt r1.yz, r1.yyzy, l(0, 0, 0, 0)
 161:       iadd r1.yz, -r4.xxyx, r1.yyzy
 162:       itof r1.yz, r1.yyzy
 163:       mul r1.yz, r1.yyzy, r2.zzwz
 164:       add r1.w, l(1.000000), -cb2[2].z
 165:       mul r2.zw, r1.wwww, r1.yyyz
 166:       mad r2.xy, -r1.yzyy, r1.wwww, r8.xyxx
 167:       sample_l(texture2d)(float,float,float,float) r1.y, r2.xyxx, t2.yxzw, s0, l(0)
 168:       lt r1.z, l(0), r1.y
 169:       lt r1.w, cb2[2].z, l(0.500000)
 170:       and r1.z, r1.w, r1.z
 171:       mad r4.xy, r2.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 172:       mul r6.xyzw, -r4.yyyy, cb0[62].xyzw
 173:       mad r4.xyzw, cb0[61].xyzw, r4.xxxx, r6.xyzw
 174:       mad r4.xyzw, cb0[63].xyzw, r1.yyyy, r4.xyzw
 175:       add r4.xyzw, r4.xyzw, cb0[64].xyzw
 176:       mad r1.yw, r8.xxxy, l(0.000000, 2.000000, 0.000000, 2.000000), l(0.000000, -1.000000, 0.000000, -1.000000)
 177:       mul r6.xyzw, -r1.wwww, cb0[25].xyzw
 178:       mad r6.xyzw, cb0[24].xyzw, r1.yyyy, r6.xyzw
 179:       mad r6.xyzw, cb0[26].xyzw, r8.zzzz, r6.xyzw
 180:       add r6.xyzw, r6.xyzw, cb0[27].xyzw
 181:       mad r1.y, cb1[2].z, r8.z, cb1[2].w
 182:       div r1.y, l(1.000000, 1.000000, 1.000000, 1.000000), r1.y
 183:       div r4.xyz, r4.xyzx, r4.wwww
 184:       div r6.xyz, r6.xyzx, r6.wwww
 185:       add r4.xyz, r4.xyzx, -r6.xyzx
 186:       dp3 r1.w, r4.xyzx, r4.xyzx
 187:       sqrt r1.w, r1.w
 188:       mul r1.y, r1.y, r1.y
 189:       mul r1.y, r1.y, l(0.010000)
 190:       max r1.y, r1.y, l(0.250000)
 191:       lt r1.y, r1.y, r1.w
 192:       movc r1.y, r1.y, l(0), r0.w
 193:       movc r1.x, r1.z, r1.y, r0.w
 194:     else
 195:       mov r2.zw, l(0, 0, 0, 0)
 196:       mov r1.x, l(0)
 197:     endif
 198:     sample_l(texture2d)(float,float,float,float) r1.yz, r2.xyxx, t4.zxyw, s0, l(0)
 199:     mad r4.xy, abs(r1.yzyy), l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 200:     mul r4.xy, r4.xyxx, r4.xyxx
 201:     div r0.w, l(2.000000), r0.z
 202:     add r0.w, r0.w, l(-1.000000)
 203:     div r0.w, l(1.000000, 1.000000, 1.000000, 1.000000), r0.w
 204:     mul r0.w, r0.w, l(-2.035047)
 205:     exp r0.w, r0.w
 206:     add r1.w, -r0.w, l(1.000000)
 207:     sqrt r1.w, r1.w
 208:     mad r3.w, r0.w, l(-0.018729), l(0.074261)
 209:     mad r3.w, r3.w, r0.w, l(-0.212114)
 210:     mad r0.w, r3.w, r0.w, l(1.570729)
 211:     mul r0.w, r1.w, r0.w
 212:     mul r6.xyzw, r3.yyyy, cb0[25].xyzw
 213:     mad r6.xyzw, cb0[24].xyzw, r3.xxxx, r6.xyzw
 214:     mad r6.xyzw, cb0[26].xyzw, r3.zzzz, r6.xyzw
 215:     add r6.xyzw, r6.xyzw, cb0[27].xyzw
 216:     div r6.xyzw, r6.xyzw, r6.wwww
 217:     mad r3.xy, r8.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 218:     mul r7.xyzw, r3.yyyy, cb0[25].xyzw
 219:     mad r7.xyzw, cb0[24].xyzw, r3.xxxx, r7.xyzw
 220:     mad r7.xyzw, cb0[26].xyzw, r8.zzzz, r7.xyzw
 221:     add r7.xyzw, r7.xyzw, cb0[27].xyzw
 222:     add r3.xyw, -r6.xyxz, cb0[44].xyxz
 223:     dp3 r1.w, r3.xywx, r3.xywx
 224:     rsq r4.z, r1.w
 225:     mul r8.xyz, r3.xywx, r4.zzzz
 226:     sqrt r1.w, r1.w
 227:     lt r4.w, abs(r5.w), l(0.999000)
 228:     movc r9.z, r4.w, l(0), l(1.000000)
 229:     and r9.y, r4.w, l(1.000000)
 230:     mov r9.x, l(0)
 231:     mul r10.xyz, r5.xywx, r9.xyzx
 232:     mad r9.xyz, r9.zxyz, r5.ywxy, -r10.xyzx
 233:     dp3 r4.w, r9.xyzx, r9.xyzx
 234:     rsq r4.w, r4.w
 235:     mul r9.xyz, r4.wwww, r9.xyzx
 236:     div r7.xyzw, r7.xyzw, r7.wwww
 237:     add r6.xyzw, -r6.xyzw, r7.xyzw
 238:     dp4 r4.w, r6.xyzw, r6.xyzw
 239:     sqrt r4.w, r4.w
 240:     mul r6.xyz, r8.xyzx, r9.zxyz
 241:     mad r6.xyz, r9.yzxy, r8.yzxy, -r6.xyzx
 242:     sincos r7.x, r10.x, r0.w
 243:     mad r7.yzw, -r8.xxyz, r1.wwww, cb0[44].xxyz
 244:     mul r10.yzw, r5.xxyw, r9.xxyz
 245:     mad r5.xyz, r5.wxyw, r9.yzxy, -r10.yzwy
 246:     mul r9.xyz, r8.xyzx, r5.xyzx
 247:     mad r5.xyz, r5.zxyz, r8.yzxy, -r9.xyzx
 248:     add r0.w, r1.w, r4.w
 249:     mul r9.xyz, r0.wwww, r8.xyzx
 250:     mul r10.yzw, -r9.yyyy, cb0[21].xxyw
 251:     mad r9.xyw, cb0[20].xyxw, -r9.xxxx, r10.yzyw
 252:     mad r9.xyz, cb0[22].xywx, -r9.zzzz, r9.xywx
 253:     add r9.xyz, r9.xyzx, cb0[23].xywx
 254:     div r9.xy, r9.xyxx, r9.zzzz
 255:     mad r9.xy, r9.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
 256:     mul r9.xy, r9.xyxx, cb2[0].xyxx
 257:     dp3 r0.w, r6.yzxy, -r8.xyzx
 258:     mul r10.yzw, r0.wwww, r6.yyzx
 259:     mad r11.xyz, -r3.xywx, r4.zzzz, -r10.yzwy
 260:     mad r10.yzw, r10.xxxx, r11.xxyz, r10.yyzw
 261:     mul r11.xyz, -r8.yzxy, r6.xyzx
 262:     mad r6.xyz, r6.zxyz, -r8.zxyz, -r11.xyzx
 263:     mad r6.xyz, r6.xyzx, r7.xxxx, r10.yzwy
 264:     mad r6.xyz, r6.xyzx, r4.wwww, r7.yzwy
 265:     add r6.xyz, r6.xyzx, -cb0[44].xyzx
 266:     mul r10.yzw, r6.yyyy, cb0[21].xxyw
 267:     mad r6.xyw, cb0[20].xyxw, r6.xxxx, r10.yzyw
 268:     mad r6.xyz, cb0[22].xywx, r6.zzzz, r6.xywx
 269:     add r6.xyz, r6.xyzx, cb0[23].xywx
 270:     dp3 r0.w, r5.yzxy, -r8.xyzx
 271:     mul r10.yzw, r0.wwww, r5.yyzx
 272:     mad r3.xyw, -r3.xyxw, r4.zzzz, -r10.yzyw
 273:     mad r3.xyw, r10.xxxx, r3.xyxw, r10.yzyw
 274:     mul r10.xyz, -r8.yzxy, r5.xyzx
 275:     mad r5.xyz, r5.zxyz, -r8.zxyz, -r10.xyzx
 276:     mad r3.xyw, r5.xyxz, r7.xxxx, r3.xyxw
 277:     mad r3.xyw, r3.xyxw, r4.wwww, r7.yzyw
 278:     add r3.xyw, r3.xyxw, -cb0[44].xyxz
 279:     mul r5.xyz, r3.yyyy, cb0[21].xywx
 280:     mad r5.xyz, cb0[20].xywx, r3.xxxx, r5.xyzx
 281:     mad r3.xyw, cb0[22].xyxw, r3.wwww, r5.xyxz
 282:     add r3.xyw, r3.xyxw, cb0[23].xyxw
 283:     mul r4.xy, r4.xyxx, r4.xyxx
 284:     add r1.yz, r1.yyzy, l(0.000000, -0.500000, -0.500000, 0.000000)
 285:     lt r4.zw, l(0, 0, 0, 0), r1.yyyz
 286:     lt r1.yz, r1.yyzy, l(0, 0, 0, 0)
 287:     iadd r1.yz, -r4.zzwz, r1.yyzy
 288:     itof r1.yz, r1.yyzy
 289:     mad r1.yz, r4.xxyx, r1.yyzy, r2.zzwz
 290:     mul r1.yz, r1.yyzy, cb1[0].xxyx
 291:     dp2 r0.w, r1.yzyy, r1.yzyy
 292:     sqrt r0.w, r0.w
 293:     mad r1.y, cb1[2].z, r3.z, cb1[2].w
 294:     div r1.y, l(0.020000), r1.y
 295:     log r1.y, r1.y
 296:     mad_sat r1.y, r1.y, l(0.693147), l(0.010000)
 297:     mul r0.w, -r0.w, r1.y
 298:     mul r0.w, r0.w, l(1.442695)
 299:     exp r4.x, r0.w
 300:     ge r0.z, l(0.000100), r0.z
 301:     div r1.yz, r6.xxyx, r6.zzzz
 302:     mad r1.yz, r1.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 0.500000, 0.500000, 0.000000)
 303:     mad r1.yz, -r1.yyzy, cb2[0].xxyx, r9.xxyx
 304:     dp2 r0.w, r1.yzyy, r1.yzyy
 305:     div r1.yz, r3.xxyx, r3.wwww
 306:     mad r1.yz, r1.yyzy, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 0.500000, 0.500000, 0.000000)
 307:     mad r1.yz, -r1.yyzy, cb2[0].xxyx, r9.xxyx
 308:     dp2 r1.y, r1.yzyy, r1.yzyy
 309:     add r0.w, r0.w, r1.y
 310:     sqrt r0.w, r0.w
 311:     mul r0.w, r0.w, l(0.500000)
 312:     log r0.w, r0.w
 313:     max r0.w, r0.w, l(0)
 314:     min r0.w, r0.w, cb2[5].y
 315:     div r0.w, r0.w, cb2[5].y
 316:     movc r4.y, r0.z, l(0), r0.w
 317:   else
 318:     mov r4.xy, l(0, 0, 0, 0)
 319:   endif
 320:   sample_l(texture2d)(float,float,float,float) r1.yzw, r2.xyxx, t1.wxyz, s0, l(0)
 321:   max r0.z, r1.z, r1.y
 322:   max r0.z, r1.w, r0.z
 323:   add r0.z, r0.z, l(1.000000)
 324:   div r0.z, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
 325:   mul r2.xyzw, r0.zzzz, r1.yzwy
 326:   store_uav_typed u0.xyzw, r0.xyyy, r2.xyzw
 327:   store_uav_typed u1.xyzw, r0.xyyy, r4.xyxx
 328:   store_uav_typed u2.xyzw, r0.xyyy, r1.xxxx
 329: endif
 330: ret
