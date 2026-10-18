# 原生脸部 SDF 证据与接入边界

状态：原生输入核验通过，渲染实现待验证。

## 来源

原生材质为 `M_actor_laevat_face_01`。
材质来源为以下对象：

```text
CAB-857c2a7da1a12d7185ddffbe85d07b99:-8531131173286331691
```

原生着色器引用为以下对象：

```text
CAB-81c10fe65ea9f33d11480e0ae9b5d883:4484747192473637154
```

原生输入由固定读取器导出。
提取记录中的提交为：

```text
5bb936ca1d3662fe9b956ec669ab7ebea08bfd07
```

当前读取器的实现提交为：

```text
6226108267942ec940cc65b6850ea8c49198c20f
```

第三方着色器参考固定于以下版本：

- 仓库：[RuriRipperImporter](https://github.com/ShiyumeMeguri/RuriRipperImporter)。
- 提交：`a6e85083da919d995e7efce6b7f55b8e122ec72c`。
- 文件：`Game/Endfield/shader/Substance/Ruri_Endfield_Uber.glsl`。
- 许可：`AGPL-3.0`。
- 文件摘要：`92e1fd48a30f69539c16c104dc1d898dfd0b171319f8b90405fc4292d38e0969`。

该文件属于第三方重建实现。
原生编译变体的逐项对照仍待完成。
代码借鉴应记录许可、来源与适配范围。

## 原生贴图

以下六张图均已核对对象、尺寸和 PNG 摘要。
导出记录包含一次垂直翻转。
消费端应沿用现有源 UV 转换契约。

| 槽位 | 原生名称 | 尺寸 | 颜色空间 |
| --- | --- | --- | --- |
| `_SDFLightmap` | `T_actor_common_female_face_02_SDF` | 1024² | 线性 |
| `_SDFMask` | `T_actor_common_female_face_01_cm_M` | 256² | 线性 |
| `_DiffRampMap` | `T_actor_common_face_01_RD` | 256×1 | 线性 |
| `_ShadowLutTex` | `T_actor_common_femaleskincolor01_lut_D` | 1024×32 | sRGB |
| `_HighlightMap` | `T_actor_common_face_01_hl_M` | 512² | 线性 |
| `_EmotionMap` | `T_actor_common_female_emotion_atlas_01_D` | 1024² | sRGB |

`_SDFLightmap` 的对象标识为：

```text
CAB-ea4c5fa4cfee8a46408e3abea5381c4a:-4042126265946545337
```

`_SDFMask` 的对象标识为：

```text
CAB-7e98ea052b73e925d9a37327a2e289e1:5056732087567382100
```

参考实现的通道用途如下。
行号均对应固定版本的 GLSL。

| 输入 | 用途 | 来源行号 |
| --- | --- | --- |
| Lightmap RG | 两通道平均，驱动平滑阈值 | 1673、1684 |
| Lightmap B | 随光侧翻转的脸部法线 | 1675—1679 |
| Lightmap A | 传入逐灯接口 | 1674、1776 |
| Mask R | 轮廓颜色的相机门控乘数 | 1640 |
| Mask G | 几何与 SDF 混合、镜面强度 | 1639、1645、1679、1689 |
| Mask B | 脸部与皮肤轮廓强度插值 | 1641 |
| Mask A | 皮肤镜面相机门控、逐灯参数 | 1753、1776 |

## 头部基底与主光阈值

参考实现的 1616—1625 行说明基底来源。
蒙皮分支使用头骨矩阵。
对象分支使用对象到世界矩阵。
同一矩阵的平移还用于径向半球中心。
具体骨骼索引和矩阵空间需要原生变体证据。
当前读取器的预览基底属于显式宿主参数。

以下公式描述已定位的局部算术。
`R/U/F` 为头部基底，`L` 为调整后的主光。
`N` 为当前表面法线，`M` 为独立 Mask 采样。
`C` 为相机前向，`Y` 为世界向上方向。
`CP` 表示原生角色全局参数。

```text
epsilon = 6.103515625e-5
lh = normalize([dot(L,R), epsilon, dot(L,F)])
positive = lh.x > 0
sampleU = positive ? u : 1-u
S = sampleLod0(SDFLightmap, [sampleU,v])
threshold = (S.r + S.g) / 2
nx = positive ? 2*S.b-1 : 1-2*S.b
nf = normalize([nx,epsilon,1-abs(nx)])
Nsdf = normalize(nf.x*R + nf.y*U + nf.z*F)
Nblend = normalize(lerp(Nsdf,N,M.g))

Cf = normalize(C-Y*dot(C,Y))
Lf = normalize(L-Y*dot(L,Y))
opposition = saturate(-dot(Cf,Lf))
back = opposition*saturate(-lh.z)*(1-CP12.x)
wrap = lh.z + back*0.5*(1-lh.z*lh.z)
halfWrap = wrap/2
t = clamp(0.5-halfWrap,0.001,0.999)
p = smoothstep(max(2*t-1,0),min(2*t,1),threshold)
sdfNdotL = 2*abs(p+ceil(halfWrap)*halfWrap)-1
geom = clamp(dot(N,L)+CP11.w*CP12.x,-1,1)
rampU = lerp(sdfNdotL,geom,M.g)/2+0.5
rampRGBA = sampleLod0(DiffRampMap,[rampU,0.5])
```

参考实现的 1663—1700 行支持这些公式。
读取器 `character_sdf_nodes.py` 有同类算术。
读取器预览把 `CP12.x/CP11.w` 显式设为零。
该预览参数尚需与实际场景状态对照。

## 遮罩与颜色

轮廓着色由相机、几何法线与独立遮罩共同驱动。
参考范围为 1634—1651 行。

```text
c = normalize([dot(C,R),dot(C,U),dot(C,F)])
cameraGate = saturate(c.z / length(c.xz) + 0.5)
gate = lerp(cameraGate,1,M.g)*M.r
scale = lerp(FaceRimOffScale,SkinRimOffScale,M.b)
amount = saturate((1-(saturate(dot(N,V))*0.85+0.15))*gate*scale)
rimAlbedo = albedo * lerp([1,1,1],SDFRimColor.rgb,amount)
specScale = M.g*Specular
```

原生脸部参数包含以下值：

```text
UseSDFLightmap = 1
FaceRimOffScale = 1
SkinRimOffScale = 0.7
SDFRimColor = [0.6509804,0.3882353,0.40784314,1]
Specular = 0.454
Smoothness = 0
EmotionBlend = 0
EmotionIndex = 0
```

中性表情保持上述原生表情参数。
影色表还需要保留独立颜色空间。
1385—1396 行以 sRGB 坐标访问三维颜色表切片。
1456—1490 行继续消费 ramp 的 RGB 和 A。
1753 行还使用 Mask A 参与皮肤镜面门控。

## 工具覆盖范围

固定参考文件保留了两处宿主占位接口。
1415—1417 行的屏幕阴影恒为 `[1,1]`。
1579—1582 行的逐灯接口直接返回输入颜色。
因此，完整阴影和附加灯需独立补齐生产路径。
Lightmap A 的逐灯实际公式仍待原生变体核对。

读取器的附加灯节点支持显式宿主灯参数。
其验证范围主要是已确认的普通灯响应。
原生灯模式、阴影生产和运行时全局量仍需对照。

## 引擎接入要求

当前 `faceSdf` 契约使用一张图和单通道距离。
加载器把距离复制到 RGB，把同图遮罩写入 A。
位置见 `GltfLoader.cpp` 的 956—1078、1240—1268 行。

完整接入需要版本化的多输入材质契约。
契约应明确独立贴图、颜色空间和采样层级。
两张 SDF 图应保留全部 RGBA 通道。
头骨引用、基底变换和对象中心应逐帧更新。
主光、场景阴影、角色阴影应各自保留语义。
角色全局参数应由可审计状态提供。
局部已确认算术可独立实现和验证。
完整等价状态以原生变体与实景对照为准。

最低验证应覆盖左右光、背光和头部旋转。
还需覆盖独立遮罩、影色表和附加灯组合。
几何、蒙皮和原生动画应保持已有验收结果。

## 本机证据

产物位于以下目录：

```text
build/runs/20261017-175206-nr-playable-integration-d4e789/face-sdf-evidence/
```

`download-receipt.json` 记录固定参考摘要。
`native-face-audit.json` 记录六张原生图的核验。
`audit_native_face.py` 可重复执行离线审计。
本轮审计核对了文件指纹和关键源码锚点。
渲染对照由后续引擎候选执行。
