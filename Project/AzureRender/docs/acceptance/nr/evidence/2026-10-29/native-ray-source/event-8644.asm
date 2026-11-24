Shader hash 873b1cdb-32698da1-5473a633-45b52937

cs_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[16], immediateIndexed
      dcl_constantbuffer cb1[5], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u0
      dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u1
      dcl_input vThreadID.xy
      dcl_temps 12
      dcl_thread_group 8, 8, 1
   0: mov r0.xy, vThreadID.yxyy
   1: mov r0.zw, l(0, 0, 32, 0)
   2: loop
   3:   uge r1.x, l(0), r0.z
   4:   breakc_nz r1.x
   5:   and r1.xy, r0.zzzz, r0.yxyy
   6:   umin r1.xy, r1.xyxx, l(1, 1, 0, 0)
   7:   imul null, r1.z, r0.z, r0.z
   8:   imul null, r1.w, r1.x, l(3)
   9:   xor r1.w, r1.y, r1.w
  10:   imad r0.w, r1.z, r1.w, r0.w
  11:   ieq r1.x, r1.x, l(1)
  12:   iadd r1.zw, -r0.yyyx, l(0, 0, 63, 63)
  13:   movc r1.xz, r1.xxxx, r1.zzwz, r0.yyxy
  14:   movc r0.xy, r1.yyyy, r0.xyxx, r1.xzxx
  15:   ushr r0.z, r0.z, l(1)
  16: endloop
  17: ftou r0.x, cb1[1].z
  18: and r0.x, r0.x, l(63)
  19: imad r0.x, l(288), r0.x, r0.w
  20: utof r0.x, r0.x
  21: mad r0.xy, r0.xxxx, l(0.754878, 0.569840, 0.000000, 0.000000), l(0.500000, 0.500000, 0.000000, 0.000000)
  22: frc r0.xy, r0.xyxx
  23: utof r0.zw, vThreadID.xxxy
  24: add r1.xy, r0.zwzz, l(0.500000, 0.500000, 0.000000, 0.000000)
  25: mul r1.zw, r1.xxxy, cb1[4].zzzw
  26: mul r0.zw, r0.zzzw, cb1[4].zzzw
  27: gather4(texture2d)(float,float,float,float) r2.xyz, r0.zwzz, t1.xyzw, s0.x
  28: gather4(1,1,0)(texture2d)(float,float,float,float) r3.yw, r0.zwzz, t1.yzwx, s0.x
  29: mov r3.xz, r2.xxzx
  30: add r3.xyzw, -r2.yyyy, r3.xyzw
  31: add r4.xyzw, -r3.xxzz, r3.yyww
  32: mad r4.xyzw, r4.xyzw, l(0.500000, -0.500000, 0.500000, -0.500000), r3.xyzw
  33: min r3.xyzw, abs(r3.xyzw), abs(r4.xyzw)
  34: mul r2.xw, r2.yyyy, l(0.011000, 0.000000, 0.000000, 0.999990)
  35: div r3.xyzw, r3.xyzw, r2.xxxx
  36: add_sat r3.xyzw, -r3.xyzw, l(1.250000, 1.250000, 1.250000, 1.250000)
  37: mul r3.xyzw, r3.xyzw, l(2.900000, 2.900000, 2.900000, 2.900000)
  38: round_ne r3.xyzw, r3.xyzw
  39: dp4 r0.z, r3.xyzw, l(0.250980, 0.062745, 0.015686, 0.003922)
  40: store_uav_typed u1.xyzw, vThreadID.xyyy, r0.zzzz
  41: sample_l(texture2d)(float,float,float,float) r0.zw, r1.zwzz, t0.zwxy, s0, l(0)
  42: mad r0.zw, r0.zzzw, l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
  43: dp2 r3.x, l(1.000000, 1.000000, 0.000000, 0.000000), abs(r0.zwzz)
  44: add r3.y, -r3.x, l(1.000000)
  45: lt r3.w, r3.y, l(0)
  46: ge r4.xy, r0.zwzz, l(0, 0, 0, 0)
  47: add r4.zw, -abs(r0.wwwz), l(0.000000, 0.000000, 1.000000, 1.000000)
  48: movc r4.xy, r4.xyxx, l(1.000000, 1.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
  49: mul r4.xy, r4.xyxx, r4.zwzz
  50: movc r3.xz, r3.wwww, r4.xxyx, r0.zzwz
  51: dp3 r0.z, r3.xyzx, r3.xyzx
  52: rsq r0.z, r0.z
  53: mul r3.xyz, r0.zzzz, r3.xyzx
  54: mul r4.xyz, r3.yyyy, cb0[1].xyzx
  55: mad r3.xyw, cb0[0].xyxz, r3.xxxx, r4.xyxz
  56: mad r3.xyz, cb0[2].xyzx, r3.zzzz, r3.xywx
  57: mad r0.zw, r1.zzzw, l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
  58: mul r1.zw, r0.wwww, cb0[13].xxxy
  59: mad r0.zw, cb0[12].xxxy, r0.zzzz, r1.zzzw
  60: add r0.zw, r0.zzzw, cb0[14].xxxy
  61: add r0.zw, r0.zzzw, cb0[15].xxxy
  62: mul r2.xy, r2.wwww, r0.zwzz
  63: mov r2.z, -r2.y
  64: dp3 r0.z, -r2.xzwx, -r2.xzwx
  65: rsq r0.z, r0.z
  66: mul r4.xyz, r0.zzzz, -r2.xzwx
  67: mad r0.zw, r1.xxxy, cb1[4].zzzw, cb1[4].zzzw
  68: mad r0.zw, r0.zzzw, l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
  69: mul r0.w, r0.w, cb0[13].x
  70: mad r0.z, cb0[12].x, r0.z, r0.w
  71: add r0.z, r0.z, cb0[14].x
  72: add r0.z, r0.z, cb0[15].x
  73: mad r0.z, r0.z, r2.w, -r2.x
  74: mul r0.w, cb1[0].y, cb1[0].x
  75: div r0.w, r0.w, r0.z
  76: min r0.w, r0.w, l(48.000000)
  77: mul r0.z, r0.z, r0.w
  78: mul r1.z, r0.z, cb1[0].z
  79: div r1.w, l(-1.000000), r1.z
  80: add r2.y, l(1.000000), -cb1[0].z
  81: mul r0.z, r0.z, r2.y
  82: div r0.z, r0.z, r1.z
  83: add r0.z, r0.z, l(1.000000)
  84: div r1.z, l(1.300000), r0.w
  85: add r2.y, -r0.w, l(10.000000)
  86: mul_sat r2.y, r2.y, l(0.010000)
  87: mul r2.y, r2.y, l(0.500000)
  88: add r4.w, l(-1.000000), cb1[3].x
  89: add r5.x, l(1.000000), cb1[1].x
  90: mov r6.z, l(0)
  91: mov r3.w, -r3.z
  92: mov r5.y, r2.y
  93: mov r5.z, l(0)
  94: loop
  95:   ge r3.z, r5.z, l(2.000000)
  96:   breakc_nz r3.z
  97:   add r3.z, r0.x, r5.z
  98:   mul r3.z, r3.z, l(1.570796)
  99:   sincos r7.x, r6.x, r3.z
 100:   mov r6.y, r7.x
 101:   mul r7.xy, r0.wwww, r6.xyxx
 102:   dp2 r3.z, r6.xyxx, r4.xyxx
 103:   mad r6.xyw, -r4.xyxz, r3.zzzz, r6.xyxz
 104:   mul r8.xyz, r4.yzxy, r6.wxyw
 105:   mad r8.xyz, r6.ywxy, r4.zxyz, -r8.xyzx
 106:   dp3 r3.z, r8.xyzx, r8.xyzx
 107:   rsq r3.z, r3.z
 108:   mul r8.xyz, r3.zzzz, r8.xyzx
 109:   dp3 r3.z, r3.xywx, r8.xyzx
 110:   mad r8.xyz, -r8.xyzx, r3.zzzz, r3.xywx
 111:   dp3 r3.z, r8.xyzx, r8.xyzx
 112:   sqrt r3.z, r3.z
 113:   dp3 r7.z, r8.xyzx, r4.xyzx
 114:   div_sat r7.z, r7.z, r3.z
 115:   dp3 r6.x, r6.xywx, r8.xyzx
 116:   lt r6.y, l(0), r6.x
 117:   lt r6.x, r6.x, l(0)
 118:   iadd r6.x, -r6.y, r6.x
 119:   itof r6.x, r6.x
 120:   mad r6.y, r7.z, l(-0.156583), l(1.570796)
 121:   add r6.w, -r7.z, l(1.000000)
 122:   ishr r6.w, r6.w, l(1)
 123:   iadd r6.w, r6.w, l(0.000000)
 124:   mul r6.y, r6.w, r6.y
 125:   max r6.y, r6.y, l(0)
 126:   min r6.y, r6.y, l(3.141593)
 127:   mul r6.w, r6.y, r6.x
 128:   mad r8.xy, r6.xxxx, r6.yyyy, l(1.570796, -1.570796, 0.000000, 0.000000)
 129:   sincos null, r8.xy, r8.xyxx
 130:   mad r7.w, r5.z, l(0.618034), r0.y
 131:   frc r7.w, r7.w
 132:   mul r7.w, r7.w, l(0.500000)
 133:   log r7.w, r7.w
 134:   mul r7.w, r7.w, cb1[0].w
 135:   exp r7.w, r7.w
 136:   add r7.w, r1.z, r7.w
 137:   mul r8.zw, r7.wwww, r7.xxxy
 138:   dp2 r7.w, r8.zwzz, r8.zwzz
 139:   sqrt r7.w, r7.w
 140:   log r7.w, r7.w
 141:   add r7.w, r7.w, -cb1[1].w
 142:   max r7.w, r7.w, l(0)
 143:   min r7.w, r4.w, r7.w
 144:   round_ne r8.zw, r8.zzzw
 145:   mul r8.zw, r8.zzzw, cb1[4].zzzw
 146:   mad r9.xy, r1.xyxx, cb1[4].zwzz, r8.zwzz
 147:   sample_l(texture2d)(float,float,float,float) r10.w, r9.xyxx, t1.yzwx, s0, r7.w
 148:   mad r9.xy, r9.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 149:   mul r9.yz, r9.yyyy, cb0[13].xxyx
 150:   mad r9.xy, cb0[12].xyxx, r9.xxxx, r9.yzyy
 151:   add r9.xy, r9.xyxx, cb0[14].xyxx
 152:   add r9.xy, r9.xyxx, cb0[15].xyxx
 153:   mul r10.xy, r10.wwww, r9.xyxx
 154:   mad r8.zw, r1.xxxy, cb1[4].zzzw, -r8.zzzw
 155:   sample_l(texture2d)(float,float,float,float) r9.w, r8.zwzz, t1.yzwx, s0, r7.w
 156:   mad r8.zw, r8.zzzw, l(0.000000, 0.000000, 2.000000, 2.000000), l(0.000000, 0.000000, -1.000000, -1.000000)
 157:   mul r11.xy, r8.wwww, cb0[13].xyxx
 158:   mad r8.zw, cb0[12].xxxy, r8.zzzz, r11.xxxy
 159:   add r8.zw, r8.zzzw, cb0[14].xxxy
 160:   add r8.zw, r8.zzzw, cb0[15].xxxy
 161:   mul r9.xy, r9.wwww, r8.zwzz
 162:   mov r10.z, -r10.y
 163:   add r10.xyz, -r2.xzwx, r10.xzwx
 164:   mov r9.z, -r9.y
 165:   add r9.xyz, -r2.xzwx, r9.xzwx
 166:   dp3 r7.w, r10.xyzx, r10.xyzx
 167:   sqrt r7.w, r7.w
 168:   div r11.xyz, r10.xyzx, r7.wwww
 169:   dp3 r7.w, r11.xyzx, r4.xyzx
 170:   mul r10.w, r5.x, r10.z
 171:   dp3 r8.z, r10.xywx, r10.xywx
 172:   sqrt r8.z, r8.z
 173:   mad_sat r8.z, r8.z, r1.w, r0.z
 174:   add r7.w, -r8.x, r7.w
 175:   mad r7.w, r8.z, r7.w, r8.x
 176:   max r7.w, r7.w, r8.x
 177:   dp3 r8.z, r9.xyzx, r9.xyzx
 178:   sqrt r8.z, r8.z
 179:   div r10.xyz, r9.xyzx, r8.zzzz
 180:   dp3 r8.z, r10.xyzx, r4.xyzx
 181:   mul r9.w, r5.x, r9.z
 182:   dp3 r8.w, r9.xywx, r9.xywx
 183:   sqrt r8.w, r8.w
 184:   mad_sat r8.w, r8.w, r1.w, r0.z
 185:   add r8.z, -r8.y, r8.z
 186:   mad r8.z, r8.w, r8.z, r8.y
 187:   max r8.z, r8.z, r8.y
 188:   add r5.zw, r5.zzzz, l(0.000000, 0.000000, 1.000000, 2.000000)
 189:   mad r5.w, r5.w, l(0.618034), r0.y
 190:   frc r5.w, r5.w
 191:   add r5.w, r5.w, l(1.000000)
 192:   mul r5.w, r5.w, l(0.500000)
 193:   log r5.w, r5.w
 194:   mul r5.w, r5.w, cb1[0].w
 195:   exp r5.w, r5.w
 196:   add r5.w, r1.z, r5.w
 197:   mul r7.xy, r5.wwww, r7.xyxx
 198:   dp2 r5.w, r7.xyxx, r7.xyxx
 199:   sqrt r5.w, r5.w
 200:   log r5.w, r5.w
 201:   add r5.w, r5.w, -cb1[1].w
 202:   max r5.w, r5.w, l(0)
 203:   min r5.w, r4.w, r5.w
 204:   round_ne r7.xy, r7.xyxx
 205:   mul r7.xy, r7.xyxx, cb1[4].zwzz
 206:   mad r9.xy, r1.xyxx, cb1[4].zwzz, r7.xyxx
 207:   sample_l(texture2d)(float,float,float,float) r10.w, r9.xyxx, t1.yzwx, s0, r5.w
 208:   mad r9.xy, r9.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 209:   mul r9.yz, r9.yyyy, cb0[13].xxyx
 210:   mad r9.xy, cb0[12].xyxx, r9.xxxx, r9.yzyy
 211:   add r9.xy, r9.xyxx, cb0[14].xyxx
 212:   add r9.xy, r9.xyxx, cb0[15].xyxx
 213:   mul r10.xy, r10.wwww, r9.xyxx
 214:   mad r7.xy, r1.xyxx, cb1[4].zwzz, -r7.xyxx
 215:   sample_l(texture2d)(float,float,float,float) r9.w, r7.xyxx, t1.yzwx, s0, r5.w
 216:   mad r7.xy, r7.xyxx, l(2.000000, 2.000000, 0.000000, 0.000000), l(-1.000000, -1.000000, 0.000000, 0.000000)
 217:   mul r11.xy, r7.yyyy, cb0[13].xyxx
 218:   mad r7.xy, cb0[12].xyxx, r7.xxxx, r11.xyxx
 219:   add r7.xy, r7.xyxx, cb0[14].xyxx
 220:   add r7.xy, r7.xyxx, cb0[15].xyxx
 221:   mul r9.xy, r9.wwww, r7.xyxx
 222:   mov r10.z, -r10.y
 223:   add r10.xyz, -r2.xzwx, r10.xzwx
 224:   mov r9.z, -r9.y
 225:   add r9.xyz, -r2.xzwx, r9.xzwx
 226:   dp3 r5.w, r10.xyzx, r10.xyzx
 227:   sqrt r5.w, r5.w
 228:   div r11.xyz, r10.xyzx, r5.wwww
 229:   dp3 r5.w, r11.xyzx, r4.xyzx
 230:   mul r10.w, r5.x, r10.z
 231:   dp3 r7.x, r10.xywx, r10.xywx
 232:   sqrt r7.x, r7.x
 233:   mad_sat r7.x, r7.x, r1.w, r0.z
 234:   add r5.w, -r8.x, r5.w
 235:   mad r5.w, r7.x, r5.w, r8.x
 236:   max r5.w, r5.w, r7.w
 237:   dp3 r7.x, r9.xyzx, r9.xyzx
 238:   sqrt r7.x, r7.x
 239:   div r7.xyw, r9.xyxz, r7.xxxx
 240:   dp3 r7.x, r7.xywx, r4.xyzx
 241:   mul r9.w, r5.x, r9.z
 242:   dp3 r7.y, r9.xywx, r9.xywx
 243:   sqrt r7.y, r7.y
 244:   mad_sat r7.y, r7.y, r1.w, r0.z
 245:   add r7.x, -r8.y, r7.x
 246:   mad r7.x, r7.y, r7.x, r8.y
 247:   max r7.x, r7.x, r8.z
 248:   mad r7.y, abs(r7.x), l(-0.156583), l(1.570796)
 249:   add r7.w, -abs(r7.x), l(1.000000)
 250:   ishr r7.w, r7.w, l(1)
 251:   iadd r7.w, r7.w, l(0.000000)
 252:   mul r8.x, r7.w, r7.y
 253:   mad r8.y, abs(r5.w), l(-0.156583), l(1.570796)
 254:   add r8.z, -abs(r5.w), l(1.000000)
 255:   ishr r8.z, r8.z, l(1)
 256:   iadd r8.z, r8.z, l(0.000000)
 257:   mul r8.w, r8.z, r8.y
 258:   ge r7.x, r7.x, l(0)
 259:   mad r7.y, -r7.y, r7.w, l(3.141593)
 260:   movc r7.x, r7.x, r8.x, r7.y
 261:   max r7.x, r7.x, l(0)
 262:   min r7.x, r7.x, l(3.141593)
 263:   mul r7.y, r7.x, l(-2.000000)
 264:   sincos r7.w, null, r6.w
 265:   ge r5.w, r5.w, l(0)
 266:   mad r8.x, -r8.y, r8.z, l(3.141593)
 267:   movc r5.w, r5.w, r8.w, r8.x
 268:   max r5.w, r5.w, l(0)
 269:   min r5.w, r5.w, l(3.141593)
 270:   add r5.w, r5.w, r5.w
 271:   add r8.x, -r3.z, l(1.000000)
 272:   mad r3.z, r8.x, l(0.050000), r3.z
 273:   mad r7.y, r7.y, r7.w, r7.z
 274:   mad r6.w, r7.x, l(-2.000000), -r6.w
 275:   sincos null, r6.w, r6.w
 276:   add r6.w, -r6.w, r7.y
 277:   mad r7.x, r5.w, r7.w, r7.z
 278:   mad r5.w, -r6.x, r6.y, r5.w
 279:   sincos null, r5.w, r5.w
 280:   add r5.w, -r5.w, r7.x
 281:   add r5.w, r5.w, r6.w
 282:   mul r3.z, r3.z, r5.w
 283:   mad r5.y, r3.z, l(0.250000), r5.y
 284: endloop
 285: mul r0.x, r5.y, l(0.500000)
 286: log r0.x, abs(r0.x)
 287: mul r0.x, r0.x, cb1[1].y
 288: exp r0.x, r0.x
 289: max r0.x, r0.x, l(0.030000)
 290: mul r0.x, r0.x, l(0.666667)
 291: min r0.x, r0.x, l(1.000000)
 292: store_uav_typed u0.xyzw, vThreadID.xyyy, r0.xxxx
 293: ret
