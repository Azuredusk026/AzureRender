# 莱万汀首次资产接入记录

## 当前结果

莱万汀已作为第一个私有角色资产接入 Vulkan 渲染器。当前版本使用 Skeletal Mesh 的参考姿势导出静态 GLB，尚不包含骨骼、蒙皮和动画。

已验证的数据：

- 81,198 个顶点
- 283,809 个索引
- 13 个 Mesh Primitive
- 14 个 glTF Material
- 11 个材质绑定 Base Color，共 7 张去重纹理
- 7 个材质绑定 Normal Map，共 4 张去重并重建的纹理
- 5 个服装/武器材质绑定 Metallic-Roughness，共 3 张去重纹理
- 5 个服装/武器材质绑定 Specular，其中 2 个同时绑定 Emissive
- S10 材质版 GLB 大小约 58.5 MB
- Debug Validation Layer 120 帧无错误
- Release 120 帧运行通过

运行命令：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset ".\assets_private\laevat_static\laevat_static_material.glb"
```

自动测试：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset ".\assets_private\laevat_static\laevat_static_material.glb" `
  --smoke-frames 300
```

## 资产边界

源资产位于：

```text
D:\Epic\UE Project\ZMDRender\Content\ZMD\莱万汀
```

原始 `.uasset` 始终只读；所有导出结果写入：

```text
assets_private\laevat_static
```

`assets_private/` 不应提交到公开仓库。

Unreal Python 工具默认根据自身所在的 `tools/` 目录自动解析项目根目录，因此工程
目录改名后无需修改脚本。若 Unreal 以不提供 `__file__` 的方式执行脚本，或需要把
结果写入另一份工作副本，可先设置 `AZURERENDER_PROJECT_ROOT` 环境变量覆盖根目录。

## 可复现导入流程

### 1. 导出静态网格

`tools/unreal_export_laevat.py` 调用 Unreal 5.7 GLTF Exporter，将角色 Skeletal Mesh 的参考姿势导出为 `laevat_static.glb`。Python Commandlet 使用 Null RHI，适合稳定导出几何，但不会烘焙复杂 Unreal 材质。

### 2. 提取纹理和材质映射

`tools/unreal_extract_laevat_textures.py` 会：

- 将莱万汀目录内的 Texture2D 导出为 PNG；
- 读取 Material Instance 及父级链的纹理、标量与向量参数；
- 生成 `unreal_material_textures.json`；
- 不修改 Unreal 原资产。

主要贴图参数如下：

| 材质 | Base Color | Normal / Data |
|---|---|---|
| body_01 / body_02 | body_01_D | body_01_N |
| cloth_01 | cloth_01_D | cloth_01_N / P / E |
| cloth_02 / 03 / 05 | cloth_02_D | cloth_02_N / P / M |
| cloth_04（武器） | wpn_misc_0016_01_D | wpn_misc_0016_01_N / P |
| face / brow | face_01_D | 暂无独立 Normal |
| iris | iris_01_D | 暂无独立 Normal |
| hair | hair_01_D | hair_01_HN / P |

### 3. BC5 Normal 转换

Unreal 导出的 `_N` PNG 是 BC5 双通道数据：R/G 有效，B 为 0。`tools/bc5_normal_png.js` 会重建
`Z = sqrt(max(0, 1 - X² - Y²))`，翻转 DirectX/Unreal 的绿色通道，再编码成标准 RGB glTF Normal。

### 4. `_P` 通道结论

服装母材 `/Game/ZMD/MaterialLibrary/M_Common_Cloth` 使用 `MSRE` 向量参数。Unreal 输出端反向检查确认：

- Metallic 输出读取名为 `Metallic` 的 MSRE 分量；
- Specular 输出读取名为 `Specular` 的 MSRE 分量；
- 默认值为 `(0, 1, 1, 0)`；
- Roughness 经母材 Reroute 链接；
- `_M` 同时具有 `_M_Color` 和 `_M_Offset`，因此是可着色的风格遮罩，不是 Metallic。

结合参数名、输出端口和贴图统计，`T_RGBA_P` 的通道语义为：

| `_P` 通道 | 语义 | 当前状态 |
|---|---|---|
| R | Metallic | 已乘材质实例强度后转换到 glTF B |
| G | Specular | S10 已接入 dielectric F0 |
| B | Roughness | 已应用材质实例偏移后转换到 glTF G |
| A | Emissive mask | S10 已与 `_E` 相乘后接入 |

`tools/unreal_trace_cloth_material.py` 会把母材输出端追踪结果写入私有目录中的 `cloth_material_graph.json`，作为本结论的可复查证据。

### 5. 生成材质版 GLB

```powershell
node .\tools\inject_gltf_textures.js
```

脚本保留原始几何 GLB，将 Base Color、转换后的 Normal 与 Metallic-Roughness PNG 嵌入 `laevat_static_material.glb`。相同输入与相同材质参数组合只嵌入一次；共享 `_P` 但实例强度不同的材质会得到独立转换结果。

S14 将 `GGX_Metallic_Strengh` 乘入 Metallic。`Roughnessmap_Strengh`
含有正值和负值，在缺少完整 Unreal 母材图的情况下暂按
`clamp(_P.B + strength × 0.25, 0.08, 1.0)` 作保守离线近似。该近似明确
保留在转换工具中，后续取得母材函数后可替换，而不需要修改 Vulkan
运行时材质接口。

S10 同时生成 Specular-Emissive 复合纹理：

| 复合通道 | 数据 |
|---|---|
| RGB | `_E.rgb × _P.a` |
| A | `_P.g` Specular |

`_P` 为 2048²、`_E` 为 1024²；转换脚本按照相同 UV 将 `_E` 重采样到 `_P` 尺寸。RGB 通过 sRGB Vulkan Image 采样，Alpha 保持线性。材质实例的 `_E_Strengh` 被归一化后作为材质强度传给 shader。

## Vulkan S8 材质路径

渲染器目前支持：

- 每材质 Base Color、Normal、Metallic-Roughness、Specular-Emissive 四张 GPU 纹理；
- Metallic-Roughness 在线性色彩空间采样；
- per-fragment 世界坐标与相机方向；
- 粗糙度控制的 Blinn-Phong 高光宽度；
- Metallic 控制的 F0 颜色；
- 一张渲染器全局共享的线性经纬环境纹理；
- 法线方向环境漫反射、视线相关 Fresnel 与反射方向采样；
- Roughness 控制的反射方向展宽及平均环境混合；
- Specular 控制非金属材质的 0.04 F0；
- Emissive 在直接光和环境光之后叠加，不受阴影方向影响；
- 没有 `_P` 的材质使用 Metallic 0、Roughness 0.75 的回退纹理；
- OPAQUE、MASK、BLEND 和双面材质路径；
- OPAQUE/MASK 先绘制，BLEND 关闭深度写入并按视空间深度从远到近排序。

视觉 QA 未发现法线翻转、UV 爆裂或新增的描述符错误。S9 已加入基础环境反射，黑色服装和武器的层次比仅使用直接光时更清晰。环境纹理当前由程序生成并使用单层采样；它是作品集常规渲染节点的可运行基线，不替代后续的 HDR 预过滤 Cubemap。

## 后续节点

1. 接入 `_M` 风格遮罩和更接近原作的明暗分层。
2. 恢复 Matcap、AO Color、Lam Shadow Color 等角色专用材质层。
3. 为 Emissive 增加 Bloom。
4. 将程序环境升级为 HDR Cubemap、irradiance 与 prefiltered specular。
5. 在复杂透明资产出现后评估 per-triangle sorting 或 OIT。
6. 之后再进入自定义 Pose、GPU Skinning 和 Idle Animation。

## 作品集展示控制

S12 已加入：

- `Space`：暂停/继续自动旋转；
- `R`：恢复自动旋转；
- `1/2/3/4`：正面、右侧、背面、左侧固定角度；
- `Left/Right`：每次微调 5°；
- `F12`：将当前 Vulkan Swapchain Image 保存为 PNG。
- `F9`：切换常规连续光照与风格化光照；
- `F7/F8`：降低/提高 `_M` 遮罩强度，每次 0.1，范围 0–2；
- `F5/F6`：降低/提高漫反射分段阈值，每次 0.05，范围 0.05–0.95。

由于相机位于世界空间对角方向，四个预设统一加入 45° 模型偏移，使数字键 1 对应实际正面。截图通过 Transfer Source、GPU→Host Buffer 和 BGRA→RGBA 转换生成，不依赖系统截屏。输出目录为 `captures/`，并因包含私有角色画面而加入 `.gitignore`。

## 当前角色的半透明与穿透问题

角色主体没有使用 Alpha 混合，但 S14 截图中确实存在真实的后方结构泄漏：

- body、cloth、weapon、face、iris 和 hair 主体都是 `OPAQUE`；
- OPAQUE pipeline 的 `blendEnable = false`；
- OPAQUE/MASK 在 shader 中固定输出 Alpha 1；
- 只有发影、眉毛和眼影三个局部叠层使用 `BLEND`；
- 这三个 BLEND primitive 关闭深度写入，并且已经按视空间深度排序。

因此问题不是传统 Alpha 半透明，而是两类效果叠加：

1. 材质观感问题：S14 已应用 Metallic/Roughness 实例参数并降低环境反射，减轻洗灰和玻璃感。
2. 几何遮挡问题：Unreal 导出的部分三角形绕序与顶点法线方向不一致，在单面管线中会被背面剔除，形成真实孔洞。S14 截图里可从脸部看到口腔、从衣服看到手臂，属于这一问题，不能仅解释为反射错觉。
3. 风格材质缺失：`_M`、Matcap、AO Color、Lam Shadow Color 和风格化明暗层尚未接入。
4. 真实几何间隙：服装本身仍包含分离薄片、镂空和长条尾饰。

S14.1 对三角形进行统计：hair 主体约 40.3% 的三角形朝向与法线不一致；face 有 3 个、cloth_03 有 27 个、cloth_01 有 14 个异常三角形。资产预处理现在将所有 OPAQUE/MASK 角色表面设为双面，仍保持 Depth Test/Depth Write 开启。这样会补回被错误剔除的近表面，让它写入深度并遮挡口腔和身体，而不会启用颜色混合。

## S13 几何描边

- 新增 `outline.vert` 和 `outline.frag`；
- 沿顶点法线按资产最大尺寸的 0.4% 外扩；
- 使用 Front Face Culling，只绘制 inverted hull 背面；
- 描边开启 Depth Test、关闭 Depth Write；
- 描边先绘制，主材质随后覆盖壳层内部；
- BLEND 叠层不参与外扩，避免眉毛和眼影生成卡片边框；
- 当前颜色为深蓝黑 `(0.018, 0.028, 0.042)`。

视觉检查确认正面、侧面和背面的轮廓宽度稳定。复杂交叠部位会出现少量内部结构线，这是几何外扩法的预期特征；后续可与屏幕空间边缘检测组合。

## S14 材质实例强度与环境反射校正

材质实例参数：

| 材质 | Metallic Strength | Roughness Adjustment |
|---|---:|---:|
| cloth_01 | 0.5 | +0.5 |
| cloth_02 | 0.5 | +0.3 |
| cloth_03 | 默认 0.5 | 0.0 |
| cloth_04 / weapon | 0.5 | -0.7 |
| cloth_05 | 0.5 | +0.3 |

实现与结果：

- Metallic 在离线转换时乘实例强度；
- Roughness 使用四分之一尺度的带符号保守偏移并限制到 `[0.08, 1.0]`；
- Metallic-Roughness 缓存键包含输入文件、Metallic Strength 和 Roughness Adjustment；
- 环境镜面能量从 `mix(1.15, 0.35, roughness)` 降为 `mix(0.70, 0.20, roughness)`；
- 金属 Base Color 补偿从 `mix(0.18, 0.05, roughness)` 降为 `mix(0.06, 0.015, roughness)`；
- 固定正面截图对比显示肩甲、裙装和武器的大面积冷灰反射收敛，暗部更稳定，肤色与基础色亮度基本保留；
- 新截图为 `captures/capture_1784904559877.png`，S13 对照图为 `captures/capture_1784903926858.png`。

验证：

- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- 重新生成 GLB 为 60,561,264 bytes，含 19 张去重内嵌纹理；
- 最大化窗口、固定正面预设和原生交换链截图路径通过。

## S14.1 双面遮挡修复

用户复查 S14 截图后指出：从前上方能够看见本应被头部遮挡的口腔结构，手臂也会穿过衣服。这一观察是正确的；此前仅以环境反射解释“半透明感”的结论不完整。

诊断：

- OPAQUE 管线的 Depth Test 与 Depth Write 均正常开启；
- OPAQUE 输出 Alpha 为 1，Swapchain Composite Alpha 也是 OPAQUE；
- GLB 根节点没有负尺度或镜像变换；
- 三角形绕序统计发现 hair、face 和部分 cloth 表面存在局部朝向不一致；
- 单面背面剔除会把这些近表面移除，使口腔、身体等后方几何通过孔洞露出。

修复：

- `inject_gltf_textures.js` 将 Laevat 的所有 OPAQUE/MASK 材质设为 `doubleSided = true`；
- 三个 BLEND 叠层保持原有单/双面状态和透明排序路径；
- 双面 OPAQUE 继续使用无混合、Depth Test 开启、Depth Write 开启的管线；
- shader 已有 `gl_FrontFacing` 法线翻转，因此补回的背面使用正确的光照朝向。

验证：

- 正面固定机位中，脸中央原先暴露的圆形口腔结构消失；
- 肩部、袖口和衣服区域的手臂泄漏明显减少；
- 右侧固定机位的头部、胸甲、裙装遮挡连续；
- Debug Validation Layer 120 帧通过，无 warning/error；
- Release 120 帧通过；
- 修复后截图：`captures/capture_1784905076440.png`。

## S15 `_M` 风格遮罩与分段漫反射

资产盘点：

- 只有 `cloth_02` 与 `cloth_05` 使用真实 `_M`；
- 对应纹理为 `T_actor_laevat_cloth_02_M.png` 和 `T_actor_laevat_cloth_03_M.png`；
- 两张纹理均为 1024×1024、8-bit 单通道灰度图；
- 黑色像素分别占 96.30% 和 97.69%，亮值集中在纹理岛和服装结构边缘；
- 两个实例都具有 `_M_Color = (7.0, 0.35, 0.0, 1.0)`；
- 该向量更接近强度、阈值与暖色控制的打包值，不按普通 LDR RGB 直接相乘。

数据路径：

- PNG 工具增加 8-bit grayscale 解码支持；
- GLB 注入器暂时使用 glTF `occlusionTexture` 槽携带项目内部 `_M` 数据；
- Loader 将该槽解码为 RGBA Style Mask；
- 没有 `_M` 的材质使用 2×2 黑色回退纹理；
- `GpuMaterial` 新增 Style Mask GPU Texture；
- Descriptor Set Layout 新增 binding 6；
- 每材质组合采样器数量从 5 增加到 6；
- 新 GLB 为 60,674,036 bytes，含 2 个 Style Mask 材质和 21 张去重内嵌纹理。

Shader 映射：

- `smoothstep(0.08, 0.62, mask)` 提取稀疏边缘；
- 暖色强调为 `mask × (BaseColor × 0.12 + (0.10, 0.015, 0.004))`；
- 连续直接漫反射改为 `smoothstep(0.28, 0.52, N·L) × BaseColor × 0.50`；
- 环境光、Metallic/Roughness、Specular、Emissive 与描边路径保持不变。

视觉结果：

- `_M` 只在 cloth_02/05 的少量边缘产生暖色强调，没有整体染色；
- 肩甲、腰部和裙装的亮暗切换更集中；
- 正面与右侧机位没有新增硬断层、穿透或遮挡回归；
- S15 截图：`captures/capture_1784905639082.png`。

验证：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过，黑色 Style Mask 回退正常；
- Debug/Release 全量 C++ 与 shader 构建通过。

当前限制：

- `occlusionTexture` 当前是项目内部 Style Mask 载体，尚未同时提供标准 glTF AO 语义；
- `_M_Color` 的精确 Unreal 母材运算仍未完整恢复；
- S15 时风格遮罩强度仍为 shader 常量；该限制已在下方 S16 解决。

## S16 运行时风格参数与作品集对比

运行时参数复用 Camera UBO 的 `renderingParameters`：

| 分量 | 用途 | 默认值 |
|---|---|---:|
| X | Outline Width | `largestExtent × 0.004` |
| Y | `_M` Style Mask Strength | 1.0 |
| Z | Diffuse Band Threshold | 0.40 |
| W | Diffuse Band Softness | 0.12 |

控制：

- `F9`：开启/关闭完整风格化响应；
- `F7/F8`：Style Mask Strength -/+ 0.1；
- `F5/F6`：Diffuse Band Threshold -/+ 0.05；
- 参数修改实时进入每帧 Camera UBO，不需要重建 pipeline 或 descriptor。

关闭风格化时：

- Style Mask Strength 变为 0；
- Diffuse Band Threshold 使用负值哨兵；
- shader 将分段结果切回连续 `N·L`；
- Direct Diffuse Scale 从风格化 0.50 恢复为常规 0.55；
- 相机、模型角度、环境、材质纹理和描边均保持相同。

交互调试：

- 初版使用 `T`、方括号和逗号/句号；
- 中文 Windows 输入法会截获这些字母/标点输入，导致 GLFW 不稳定接收；
- 最终改为 `F5–F9` 功能键，全部通过控制台日志验证；
- `F8` 输出 `Style mask strength: 1.1`；
- `F6` 输出 `Diffuse band threshold: 0.45`。

作品集对比：

- 风格化开启：`captures/capture_1784906439658.png`；
- 风格化关闭：`captures/capture_1784906454927.png`；
- 两张图均为 2560×1334、数字键 1 固定正面机位；
- 共有 19,076 个像素发生变化，占全图 0.559%；
- 最大单通道差值为 61；
- 强变化集中在角色明暗边界和 `_M` 稀疏边缘，背景保持一致。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- Debug 与 Release 最终构建通过。

## S17 Lam Shadow Color 与 AO Color

目标：恢复 Unreal 材质实例中已经存在的逐材质阴影色，让皮肤、头发、布料和武器的 toon 阴影带具有各自的色相与体积层次，同时保持 S14.1 的实体遮挡修复。

资产数据：

- 注入器同时识别 `Lam_Shadow_Color` 与头发材质使用的 `Lam_ShadowColor`；
- `AO_Color` 与 Lam Shadow Color 均从材质实例参数读取并写入 glTF `material.extras`；
- 生成的 GLB 含 7 个有效 AO Color、6 个有效 Lam Shadow Color、21 张去重内嵌纹理；
- 全零或缺失颜色不视为有效阴影色，避免没有对应遮罩的脸部被整体染黑；
- 当前代表值包括：布料 Lam 灰约 0.396、武器/cloth_04 约 0.474、头发 Lam 灰约 0.464、头发 AO 灰约 0.255、身体 AO 为暖色 `(0.45, 0.23, 0.23)`。

数据与 shader 路径：

- `AssetMaterial` 增加 `aoColor` 与 `lamShadowColor`；
- 每个颜色使用 RGBA，其中 A 作为“该参数有效”的标记；
- Material Push Constants 从 16 bytes 扩展为 48 bytes，并由 C++ `static_assert` 固定布局；
- Lam Shadow 只作用于 toon 阴影权重，混合强度为 `shadowWeight × 0.35`；
- AO Color 同样只作用于阴影带，混合强度为 `shadowWeight × 0.20`；
- F9 关闭风格化时 `bandEnabled = 0`，两种材质阴影色自动退回白色，不影响常规连续光照对照；
- 该路径没有修改 Alpha Mode、Blend State、Depth Test 或 Depth Write。

视觉 QA：

- 数字键 1 正面与数字键 2 右侧固定机位均通过；
- 开启后深色服装与头发阴影获得轻微分层，没有出现整体脏染或压黑；
- 暖色身体 AO 只在阴影带产生克制偏色，亮部肤色保持不变；
- 头部、衣裙与手臂的前后遮挡连续，未发现口腔或后层手臂重新穿透；
- 正面截图：`captures/capture_1784907473858.png`；
- 右侧截图：`captures/capture_1784907508007.png`。

验证：

- Laevat Debug Validation Layer 120 帧通过，无 warning/error；
- Laevat Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过，缺失 extras 时使用中性回退色；
- Debug/Release 全量 C++ 与 shader 构建通过。

下一节点：

- S18 优先盘点并接入脸部 SDF/阴影遮罩；当前脸部没有可安全直接套用 AO Color 的遮罩，因此仍主要依赖通用 `N·L` 分段。
- 若现有资产无法提供可靠的脸部 SDF，再转向逐材质 Matcap，优先改善头发与布料，而不是伪造脸部数据。

## S18 脸部数据审计与 Matcap

Face SDF 审计结论：

- `M_actor_laevat_face_01` 暴露 `SDF_Location = 0`、`SDF_Color ≈ 0.477` 等参数；
- 该材质没有绑定 SDF Texture 或 Shadow Mask；
- 莱万汀角色目录不存在命名为 SDF/Face Shadow 的候选纹理；
- `T_actor_laevat_face_01_D.png` 为 1024×1024 RGBA Base Color；
- Base Color Alpha 平均值为 254.978，只有 1,240 个像素不是 255，属于边缘抗锯齿，不具备距离场结构；
- 因此不使用普通 Base Color/AO 伪造 Face SDF。

真实可用数据：

- 脸材质绑定 `/Game/Matcap/Matcap01.Matcap01`；
- 使用 Unreal Python Commandlet 将其导出为 `textures/Matcap01.png`；
- 纹理为 256×256 黑底、上方白色胶囊形局部高光；
- 新工具：`tools/unreal_export_laevat_matcap.py`。

渲染路径：

- GLB `material.extras` 写入 `afterglowMatcapTexture` 与 `afterglowMatcapColor`；
- Loader 增加 Matcap 像素、尺寸与颜色；
- 无 Matcap 材质使用 2×2 黑色回退；
- Descriptor Set Layout 增加 binding 7；
- 每材质组合采样器由 6 个增加至 7 个；
- Material Push Constants 从 48 bytes 扩展至 64 bytes；
- Shader 将世界法线投影到相机 Right/Up 基向量生成 Matcap UV；
- V 方向翻转，使纹理上方高光对应朝上的脸部法线；
- S18 初版高光使用 `smoothstep(0.10, 0.85, sample)` 并以 0.10 强度叠加；该全身机位基线在 S19 近景检查后进一步软化；
- `bandEnabled` 控制 Matcap，F9 关闭时完整禁用。

视觉与量化 QA：

- 固定全身正面机位没有明显暴露脸部边界泄漏或其他材质误采样；
- 固定右侧机位没有出现视角相关闪烁或高光粘到头发；
- 与同尺寸 S17 正面截图比较，仅 269 个像素变化；
- 变化包围盒为 `(1288,326)–(1357,396)`，完整位于脸部 70×71 区域；
- 260 个像素的最大通道差不小于 8，最大单通道差为 40；
- S18 风格化截图：`captures/capture_1784908447255.png`；
- F9 关闭对照：`captures/capture_1784908458610.png`。

验证：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过，黑色 Matcap 回退正常；
- Debug/Release 全量构建通过；
- 新 GLB 为 60,676,252 bytes，含 1 个 Matcap 和 22 张去重内嵌纹理。

下一节点：

- S19 增加作品集近景/脸部机位，使脸部 Matcap、眼睛和后续脸部效果可以稳定展示与调试。
- 随后盘点 Hair Matcap 与 `_HN`，选择头发高光或角色展示背景作为下一项视觉增强。

## S19 脸部近景作品集机位

相机设计：

- 数字键 `5` 进入脸部近景并暂停自动旋转；
- 保持模型归一化缩放不变，不通过放大 Model Matrix 破坏描边宽度或材质尺度；
- 相机目标从原点改为 `(0, 0.82, 0)`；
- 相机位置设为 `(0.915, 1.507, 1.046)`，沿原全身视线方向缩短距离；
- 近景沿用正面模型角度 `45°`；
- 数字键 `1–4` 会恢复相机 `(2.8, 2.1, 3.2)`、目标原点和对应全身角度；
- Matcap 相机基向量改为从相机指向当前 fragment，兼容非原点观察目标。

近景发现与修复：

- 初版 S18 Matcap 在全身视图仅覆盖约 70×71 像素，没有暴露明显形状问题；
- S19 近景中发现原始黑白 Matcap 会在左脸产生边缘较硬的白色胶囊斑；
- F9 关闭后白斑消失，确认来源为 Matcap 而不是 Base Color 或几何穿透；
- 最终改为中心、四个轴向与四个对角方向共 9 次采样；
- 采样半径为 5 个 Matcap texel，权重总和为 1；
- Matcap 颜色改为 90% Base Color 与 10% 材质 Matcap Color；
- 最终强度由 0.10 降至 0.055；
- 结果保留柔和肤色提亮，不再出现可辨识的胶囊边界。

视觉 QA：

- 头角与发梢未被顶部裁切；
- 双眼、脸部 Base Color、眉毛和发影可在近景中直接检查；
- 左右肩甲形成框景，没有遮挡脸部；
- 数字键 `1` 可完整恢复原全身正面构图；
- 风格化近景：`captures/capture_1784909055506.png`；
- F9 关闭近景：`captures/capture_1784909068495.png`；
- 两张图均为 2560×1334；
- F9 开关变化 804,976 像素，强变化 545,778 像素，最大单通道差 46；
- 差异包围盒为 `(578,265)–(2202,1333)`，限制在角色头肩区域，背景不变。

回归：

- 调校后莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- Debug/Release 构建通过。

下一节点：

- S20 审计头发 `_HN`、`_P` 与 KK 高光参数，决定是否实现切线方向头发高光。
- 若头发数据不足，则优先进入作品集背景、地台与灯光构图。

## S20 头发 `_HN` 与 KK 高光数据

资产判断：

- `T_actor_laevat_hair_01_HN.png` 为 2048×2048 RGBA 纹理；
- RGB 作为普通切线空间法线解码时，平均向量长度只有约 0.315，且正 Z 像素约占 49.3%，因此它不是常规 RGB Normal Map；
- 四通道均围绕 0.5 分布，且母材质中同时存在 `NormalMap_Base`、`NormalMap_HeighLight`、`Tangent`、`T_Shift` 与 `KK_*` 表达式；
- 当前采用与数据和命名都一致的保守解释：`RG` 为基础头发法线 XY，`BA` 为高光法线 XY，Z 在 shader 中重建；
- `_P` 继续走既有 Metallic/Roughness 与 Specular 打包转换，不重复占用头发高光槽。

材质参数：

- `KK_Power = 496.2387`；
- `KK_Ramp = 6.0`；
- `KK_Ramp_Strengh = 0.2`；
- 另记录到 `KK_Shift_UV`、`KK_Spe_Upper_Limit`、`KK_Spe_Camera_Offset` 等母材质参数；
- 为避免 Unreal 指数直接套用造成数值过尖，shader 将 Power 映射并限制到 24–96。

导出约定：

- 注入器把 `_HN` 原始 PNG 内嵌到 GLB；
- glTF `material.extras.afterglowHairDataTexture` 保存纹理索引；
- `afterglowHairParameters` 保存 Power、Ramp Strength、Ramp 与启用标记；
- Loader 对无 `_HN` 材质提供 `(128,128,128,128)` 的 2×2 中性纹理，并把启用标记设为 0；
- 新 GLB 为 63,308,828 bytes，包含 1 个 Hair Data 材质与 23 张去重内嵌纹理。

渲染约定：

- Descriptor binding 8 为线性 UNORM Hair Data；
- Material Push Constants 从 64 bytes 扩展到 80 bytes；
- `RG` 重建后的基础法线以 0.45 权重混合到头发着色法线，恢复发束表面起伏；
- `BA` 重建后的高光法线参与切线偏移和高光朝向控制；
- Kajiya–Kay 项使用发束 Bitangent 与 Half Vector 计算，随后通过 Ramp 形成窄光带；
- 高光颜色向角色暖红发色收敛，并由 F9 风格化总开关控制；
- 该路径不修改 Alpha Mode、Blend State、Depth Test 或 Depth Write。

当前限制：

- Unreal Python 能定位母材质的 `Material Attributes` 输出，但 UE 5.7 未通过当前接口暴露 Set Material Attributes 内部连线；
- 因此 RG/BA 语义是基于纹理统计、双法线命名和 KK/Tangent 参数的工程性还原，不宣称逐节点复刻原母材质；
- KK Ramp 目前使用解析近似，尚未直接采样 Unreal `CurveLinearColorAtlas`；
- 下一阶段如需更接近原作，可单独导出 `CB_LWT_KK_Ramp_01` 或制作项目自有 Hair Ramp。

## S21 运行时作品集展示节点

S21 不修改私有 GLB。地台与背景在载入任意 glTF 后由 renderer 运行时生成，因此莱万汀资产导出仍保持 S20 的 63,308,828 bytes 和 23 张内嵌纹理。

程序化地台：

- 在角色原始 bounds 计算完成后生成 96 段圆形地台；
- 半径为角色最大原始 extent 的 40%；
- 顶面位于 `boundsMin.y - extent × 0.004`，厚度为 extent 的 5%；
- 新增 289 个顶点、864 个索引、1 个 primitive 与 1 个运行时材质；
- 地台不重新写入角色 bounds，因此不会改变既有 2.5 单位自动适配、数字键 `1–5` 机位或描边比例；
- 圆台跟随角色 Model Matrix 旋转，但由于轴对称，视觉朝向保持稳定；
- 材质使用专用 `showcasePlatform` 标记，在 UV 中生成中心接触压暗和边缘环带。

背景：

- 新增 `background.vert` 与 `background.frag`；
- 使用 `gl_VertexIndex` 绘制无顶点缓冲的全屏三角形；
- 背景在角色前先绘制，关闭 Depth Test 与 Depth Write；
- 颜色由深蓝黑到蓝灰渐变，并叠加角色背后的低强度暖紫光晕和屏幕边缘 vignette；
- 背景不使用材质描述符，也不依赖莱万汀私有纹理。

灯光：

- 主光方向为 `(0.48, 0.82, 0.32)`，颜色略暖；
- 辅光方向为 `(-0.62, 0.34, -0.48)`，使用低强度冷蓝色；
- 轮廓光根据 `1 - N·V` 与背侧方向形成，强度保持在 0.12；
- 轮廓光由 F9 风格化总开关控制，地台不接受角色轮廓光；
- 没有新增 Shadow Map；地台中心压暗是稳定的展示构图近似，不宣称真实动态阴影。

通用性：

- 莱万汀运行时统计变为 81,487 顶点、284,673 索引、14 primitives、15 materials；
- 公共测试模型运行时统计变为 337 顶点、900 索引、3 primitives、4 materials；
- 两种资产使用同一背景、地台和灯光代码，私有/公共资产边界保持不变。

## S22 运行时展示预设

S22 仍不修改或重新导出私有 GLB。三套预设完全属于 renderer 运行时状态，通过 `F1/F2/F3` 切换：

- `F1 Afterglow Gallery`：保留深蓝紫渐变、暖色主光与低强度冷色辅光，适合作为默认作品集构图；
- `F2 Endfield Industrial`：青绿色工业网格、右侧低对比度警戒条纹、偏青的主/辅/轮廓光，用于类终末地主题展示；
- `F3 Neutral Material Check`：中性灰渐变与接近白平衡的三点光，用于检查 Base Color、法线、粗糙度和材质分区。

数据边界：

- Camera UBO 新增 `showcaseParameters`，增加 16 bytes；
- 四个分量依次保存预设编号、主光强度、辅光强度和轮廓光强度；
- 背景管线复用当前帧已有 Descriptor Set 的 Camera UBO，不增加私有纹理或 GLB extras；
- 地台继续使用 S21 的运行时 primitive，只按预设改变色调；
- Material Push Constants 仍为 80 bytes；
- Descriptor binding 0–8、私有 GLB 大小、23 张内嵌纹理以及材质导出约定均不变；
- 三套预设不修改 Alpha Mode、Blend State、Depth Test、Depth Write 或 S14.1 的双面实体遮挡修复。

最终运行时统计保持为 81,487 vertices、284,673 indices、14 primitives、15 materials。公共测试模型仍通过同一套预设路径和中性材质回退。

## S23 方向光 Shadow Map

S23 继续保持私有资产只读。Shadow Map、光源矩阵和阴影采样全部由 renderer 运行时生成，莱万汀 GLB 的大小、23 张内嵌纹理和 material extras 均不改变。

运行时资源：

- 新增一张固定 2048×2048 的设备本地深度图；
- 格式通过现有 `findDepthFormat` 从 GPU 支持格式中选择；
- Usage 为 `DEPTH_STENCIL_ATTACHMENT | SAMPLED`；
- 新增独立的纯深度 Render Pass、Framebuffer 和 Clamp-to-Border 线性采样器；
- Render Pass 在结束时把深度图转换为 `DEPTH_STENCIL_READ_ONLY_OPTIMAL`；
- 前后两个 Subpass Dependency 分别同步上一帧采样到本帧写入，以及本帧写入到主材质采样。

投影与材质：

- Camera UBO 新增 `lightModelViewProjection`，增加 64 bytes；
- 使用与主光一致的方向 `(0.48, 0.82, 0.32)`；
- 使用固定正交投影覆盖归一化后的角色和运行时地台；
- 新增 `shadow.vert` 与 `shadow.frag`；
- Shadow Fragment Shader 读取 Base Color Alpha；
- MASK 材质使用原 `alphaCutoff`，BLEND 材质使用 0.35 的保守投影阈值；
- 程序化地台不写入 Shadow Map，只作为接收面；
- Descriptor binding 9 为 Shadow Map；
- Descriptor binding 0–8 与 Material Push Constants 80 bytes 均保持不变。

采样约定：

- 主材质使用手动 3×3 PCF；
- 法线相关接收偏移在 0.00025–0.0011 之间；
- Shadow Pipeline 同时启用 1.25 constant / 1.75 slope raster depth bias；
- 角色的主光漫反射、直接高光和 KK 头发高光响应 Shadow Map；
- 环境光、辅光与轮廓光不会被方向光阴影完全压黑；
- 地台接收面额外衰减环境光，使投影在作品集画面中可读；
- S21 的程序化中心压暗由 0.48 减弱到 0.72，现作为低强度稳定回退而非主要阴影。

运行时统计仍为莱万汀 81,487 vertices、284,673 indices、14 primitives、15 materials；公共模型仍为 337 vertices、900 indices、3 primitives、4 materials。

## S24 屏幕空间内部轮廓

S24 不修改或重新导出莱万汀 GLB。新增数据完全属于 renderer 的交换链相关运行时资源，私有资产大小、23 张内嵌纹理、材质 extras 和 Descriptor binding 0–9 均保持不变。

运行时附件：

- 每张交换链图像新增一张 `VK_FORMAT_R8G8B8A8_UNORM` 法线附件；
- 主场景深度附件增加 `SAMPLED` usage，并在 Render Pass 结束时转为只读采样布局；
- 主材质 Fragment Shader 在 location 1 写入编码后的几何法线和内部轮廓参与权重；
- 新增第二个 Post-process Render Pass，在交换链颜色附件上以 `LOAD` 方式叠加结果；
- 新增最近点、Clamp-to-Edge 屏幕附件采样器，以及每张交换链图像对应的法线/深度 Descriptor Set；
- Swapchain 重建和销毁路径覆盖法线 Image、Memory、View、Post Framebuffer、Descriptor Pool、Pipeline、Pipeline Layout 与 Render Pass。

边缘检测约定：

- `inner_outline.frag` 采样中心像素和八个相邻像素；
- 深度差乘以 1400 后使用 0.18 阈值，法线差使用 0.20 阈值；
- 最终深蓝黑叠加强度为 0.40；
- 背景像素和程序化地台参与度为 0，因此不会重复描绘角色剪影或地台圆周；
- 普通服装与机械材质参与度为 1；
- Hair Data 或 Matcap 材质参与度降为 0.22，避免近景中逐条描绘头发法线岛或污染面部；
- 相邻像素使用两者较小参与度缩放深度差和法线差；
- `F10` 只把后处理强度切换为 0/0.40，不重建 Pipeline 或 Descriptor。

该路径保留 S13 inverted-hull 外描边；S24 只补充角色内部结构线，不改变 Alpha Mode、双面实体遮挡、透明排序、Shadow Map 或材质纹理语义。

## S25 骨骼版 GLB 与 Bind Pose GPU 蒙皮

资产审计首先确认：S7–S24 使用的 `laevat_static_material.glb` 是纯静态导出，只有 1 个节点，`skins=0`、`animations=0`，属性中没有 `JOINTS_0/WEIGHTS_0`。该文件继续保留为稳定的静态作品集基线，不进行覆盖。

新增导出：

- `tools/unreal_export_laevat_skinned.py` 只读加载 `/Game/ZMD/莱万汀/莱万汀`；
- `export_vertex_skin_weights = True`；
- `export_animation_sequences = False`，S25 只验证 Bind Pose；
- 输出到 `assets_private/laevat_skinned/laevat_skinned.glb`；
- 原始 `.uasset`、Skeleton 和 Physics Asset 均未修改；
- `inject_gltf_textures.js` 增加可选输入/输出参数，默认静态导出行为保持不变；
- 材质注入后生成 `laevat_skinned_material.glb`，大小为 64,324,908 bytes；
- 仍包含 23 张去重纹理及 S20–S24 的全部材质 extras。

新 GLB 数据：

- 469 nodes；
- 1 mesh；
- 1 skin；
- 468 joints；
- 1 个 inverse bind matrix accessor；
- 所有角色 primitive 提供 `JOINTS_0` 与 `WEIGHTS_0`；
- 当前仍为 0 animations；
- 运行时几何统计保持为 81,487 vertices、284,673 indices、14 primitives、15 materials。

Renderer 约定：

- `JOINTS_0` 支持 unsigned byte、unsigned short 和 unsigned int；
- `WEIGHTS_0` 支持 float，或 normalized unsigned byte/short；
- 每个顶点四权重会重新归一化，零权重顶点回退到 joint 0；
- 当前每个资产只支持一个 skin；
- Loader 计算节点全局 Bind Transform，并生成 `jointWorld × inverseBindMatrix`；
- Joint Palette 使用每帧 Host-visible Storage Buffer，S25 当前写入后保持不变；
- Descriptor binding 10 为 Vertex Shader 可读的 Joint Storage Buffer；
- 主材质、inverted-hull 外描边和 Shadow Pass 共用相同蒙皮矩阵；
- 无 skin 的静态 GLB 自动使用一张 Identity Joint Matrix，不需要另一套 Pipeline。

验证：

- 骨骼版莱万汀 Debug Validation 120 帧通过；
- 骨骼版莱万汀 Release 120 帧通过；
- 原静态莱万汀 Debug Validation 120 帧通过；
- 公共静态模型 Debug Validation 120 帧通过；
- 正面 Bind Pose：`captures/capture_1785253111387.png`；
- 近景 Bind Pose：`captures/capture_1785253132877.png`；
- 未发现顶点爆炸、关节索引越界、法线/切线错位、阴影分离、外描边脱离或实体遮挡回归。

## S26：动画盘点与程序化 Idle 测试资产

Unreal 动画盘点：

- `tools/unreal_list_laevat_animations.py` 使用 Unreal 5.7 Commandlet 扫描工程内的 `AnimSequence`；
- 共发现 2 个动画序列，但没有任何序列使用莱万汀的 Skeleton；
- 扫描结果保存于私有目录
  `assets_private/laevat_skinned/compatible_animations.json`；
- 因此本节点不强行重定向未知骨架，也不修改 Unreal 原始角色资产。

可复现测试动画：

- `tools/inject_gltf_idle_animation.js` 以
  `laevat_skinned_material.glb` 为输入，生成
  `laevat_idle_material.glb`；
- 动画名为 `Afterglow_ProceduralIdle`，时长 4 秒，共 17 个关键帧；
- `Bip001_Spine2` 绕本地 Z 轴做正负 0.9 度缓慢摆动；
- `Bip001_Head` 绕本地 Y 轴做正负 0.55 度摆动，并使用 π/4 相位偏移；
- 首尾关键帧一致，适合循环播放；
- 输出 GLB 为 64,326,176 bytes，保留 469 nodes、1 skin、468 joints、
  14 个角色 primitive 与 23 张去重纹理；
- 该文件仅用于证明动画运行时和动态蒙皮路径，不宣称复刻《终末地》原动画。

运行方式：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb"
```

- `F4` 暂停或继续动画；
- `F11` 将时间轴归零并恢复播放；
- 无动画的骨骼版和静态版 GLB 仍按原 Bind Pose 路径运行。

## S27：动画展示控制与作品集环绕镜头

S27 不修改或重新导出莱万汀 GLB，继续使用 S26 的
`laevat_idle_material.glb`。新增内容全部属于 Renderer Runtime：

- 数字键 `6` 进入作品集慢速环绕镜头；
- 镜头使用 Endfield Industrial 展示预设；
- 相机位置为 `(2.32, 1.80, 2.66)`，观察目标为 `(0, 0.05, 0)`；
- 角色以 `0.16 rad/s` 旋转，完整一圈约 39.3 秒；
- 进入镜头时会恢复风格化光照、内部轮廓、动画播放并重置时间轴；
- 数字键 `7` / `8` 循环选择上一或下一动画，并从头播放；
- 数字键 `9` 输出动画索引、名称、当前时间、总时长和播放状态；
- 使用数字键而不是字母键或导航键，是为了规避中文 IME 和笔记本扩展键区
  对 GLFW 输入的影响。

视觉验收：

- 角色头顶、左右武器/衣摆与脚底均保留安全边距；
- 平台允许在画面底部轻微裁切，但角色主体保持完整；
- 正面、侧面和背面环绕过程中没有骨骼、阴影、外描边或内部遮挡分离；
- 当前程序化 Idle 幅度较小，镜头主要用于展示渲染材质、轮廓与动态蒙皮稳定性。

## S28：确定性 1080p60 样片捕获

S28 继续使用 `laevat_idle_material.glb`，没有修改角色 Mesh、Skeleton、
Animation 或材质纹理。新增功能全部位于 Renderer 捕获与离线编码路径。

正式样片配置：

- 输入资产：`assets_private/laevat_skinned/laevat_idle_material.glb`；
- 作品集镜头：S27 数字键 `6` 对应的 Portfolio Orbit；
- 输出分辨率：1920×1080；
- 固定帧率：60 fps；
- 帧数：240；
- 时长：4.000 秒；
- 动画：`Afterglow_ProceduralIdle`；
- GPU：NVIDIA GeForce RTX 4060 Laptop GPU；
- PNG 序列目录：`captures/s28_portfolio_1080p60`；
- MP4：`captures/Afterglow_S28_Portfolio_1080p60.mp4`。

确定性：

- 捕获时关闭窗口 Resize；
- Swapchain Extent 必须与请求分辨率完全一致，否则直接报错；
- 第一帧使用 `deltaTime = 0`，后续帧固定使用 `1 / fps`；
- 动画、角色环绕和 Joint Palette 不读取墙钟时间；
- 两次相同参数的六帧探针逐帧 SHA-256 完全一致；
- 每张 PNG 使用 `frame_%06d.png` 连续编号；
- 非空输出目录会被拒绝，避免覆盖旧作品集素材。

编码：

- `tools/encode_capture.ps1` 读取 `capture_manifest.json`；
- 使用 Manifest 的 fps、帧数和文件模式调用 FFmpeg；
- 输出 H.264 High Profile、YUV420p、CRF 15、Slow Preset；
- 写入 BT.709 Range、Matrix、Transfer 与 Primaries；
- 使用 MP4 Fast Start；
- 没有音轨；
- 已存在的目标视频不会被覆盖。

视觉 QA：

- 检查了第 0、120、239 帧；
- 角色头顶、肩甲、武器、衣摆、双腿和靴子始终处于画面安全区；
- 平台底部保持受控裁切；
- 四秒内角色从正面缓慢转向约 18 度侧面；
- 未观察到骨骼撕裂、阴影/外描边分离、内部结构穿透或半透明回归。

## S28.1：20 秒作品集样片

本次只延长 S28 的确定性捕获窗口，没有修改莱万汀 Mesh、Skeleton、
Animation、材质或纹理。

正式输出：

- PNG 序列：`captures/s28_portfolio_1080p60_20s`；
- MP4：`captures/Afterglow_S28_Portfolio_1080p60_20s.mp4`；
- 1920×1080、60 fps、1200 帧、20.000000 秒；
- H.264 High Profile、YUV420p、BT.709、无音轨；
- 原始 1200 张 PNG 全部连续，无缺帧，总大小约 1.033 GiB；
- MP4 大小为 16,485,087 bytes；
- MP4 SHA-256：
  `5305A559FDC425FB2E2A3CD65C947291A6B251CC1CDB68FA015B28268BC40F29`。

20 秒内角色约旋转 183°，程序化四秒 Idle 完成五次循环。视觉 QA 抽查
第 0、600、1199 帧，覆盖正面、侧面和接近背面的长时间运行状态。角色持续
处于安全框内，未发现骨骼撕裂、错误透明、衣服内结构穿透、阴影脱离或描边
错位。

## S29：GPU Timestamp 性能拆解

S29 只修改 Renderer Runtime，不修改或重新导出 GLB。每个并行帧拥有独立
`VkQueryPool`，通过四个时间戳记录三个渲染阶段：

1. Shadow Pass；
2. Main Scene Pass；
3. Internal Outline Pass。

RTX 4060 Laptop GPU、1920×1080、Release、Portfolio Orbit、600 个有效
样本的正式结果：

- Shadow 平均 0.189631 ms，占 21.520%；
- Main Scene 平均 0.620023 ms，占 70.363%；
- Internal Outline 平均 0.071526 ms，占 8.117%；
- 三阶段合计平均 0.881180 ms；
- 合计最小 0.825184 ms，最大 1.184576 ms；
- JSON：`captures/s29_gpu_timing_release_1080p.json`。

这些数据只表示 GPU 上三个 Render Pass 的工作量，不包含 Present、CPU
动画/提交、交换链等待、截图回读或 PNG 编码，因此不能直接换算为端到端
应用帧率。

## S30：作品集技术拆解视图

S30 没有修改莱万汀 GLB、Mesh、Skeleton、Animation、材质或纹理。新增内容
全部位于 Renderer Runtime 和离线捕获元数据。

诊断视图：

- Beauty：保持完整最终构图；
- World Normal：显示主场景 Normal Attachment 中编码的世界空间几何法线；
- Internal Outline：隔离显示 S24 的 Depth/Normal 边缘响应；
- Shadow Map：显示 2048×2048 光源空间深度，并仅为展示使用四次方对比曲线。

运行时按数字键 `0` 在四种视图间循环。确定性捕获可使用
`--diagnostic-view beauty|normal|outline|shadow`。`--no-stylized` 对应 F9，
`--no-inner-outline` 对应 F10。捕获 Manifest 新增：

- `diagnosticView`；
- `stylizedLighting`；
- `internalOutline`。

最终 1920×1080、同一 Portfolio Orbit 初始帧证据：

- Stylized Beauty：
  `captures/s30_final_beauty_stylized/frame_000000.png`；
- Conventional Beauty：
  `captures/s30_final_beauty_conventional/frame_000000.png`；
- World Normal：
  `captures/s30_final_world_normal/frame_000000.png`；
- Internal Outline：
  `captures/s30_final_internal_outline/frame_000000.png`；
- Shadow Map：
  `captures/s30_final_shadow_map/frame_000000.png`。

视觉验收：

- 五张图使用同一资产、分辨率、相机、模型角度和动画时间零点；
- Normal 图完整覆盖角色可见表面，背景与角色明确分离；
- Outline 图只显示内部结构响应，不混入 Beauty 色彩；
- Shadow Map 最终版保留角色各部位的灰阶深度，不再退化为纯黑剪影；
- Stylized On/Off 只改变风格化光照层，构图、轮廓和几何保持一致；
- 未出现半透明、隐藏内部结构穿透、骨骼撕裂或诊断图错位。

## S31：运行时技术 HUD

S31 不修改角色资产。HUD 是 Renderer 自身的 Vulkan 几何叠加层，使用
`stb_easy_font` 在 CPU 端把 ASCII 字符展开为四边形，再转换为三角形写入
每个 in-flight frame 独立的 Host-visible Vertex Buffer。

HUD 内容：

- 当前 Vulkan GPU；
- Framebuffer 分辨率；
- Beauty/Normal/Outline/Shadow 诊断模式；
- Stylized Lighting 与 Internal Outline 状态；
- 动画名、时间轴、时长与播放状态；
- Shadow、Main Scene、Internal Outline 和总 GPU 平均耗时。

控制：

- `--hud` 启动时显示 HUD，并自动启用 GPU Timestamp；
- `H` 在运行时显示或隐藏；
- 默认关闭，不进入现有作品集截图或 20 秒无 UI 成片；
- Manifest 新增 `hudEnabled` 字段。

最终证据：

- HUD 图：
  `captures/s31_hud_1080p_v2/frame_000002.png`；
- 1920×1080，显示 Beauty、Style On、Outline On、Idle 时间轴和完成预热的
  GPU Pass 平均值；
- PNG 大小 967,575 bytes；
- SHA-256：
  `4582AF5618802F218B94F4B38104D083A35C828071BCAF574D050963007FBAF9`。

HUD 默认关闭回归：

- `captures/s31_hud_off_regression/frame_000000.png`；
- 与 S30 Stylized Beauty 基线 SHA-256 均为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 因此默认关闭时新增 HUD Pipeline 不改变任何作品集像素。

## S32：20 秒确定性技术拆解视频

S32 仍不修改莱万汀资产。新增的 `--technical-sequence` 使用捕获帧号控制
Renderer 状态，把总帧数平均分成五章：

1. `0–239`：Beauty；
2. `240–479`：World Normal；
3. `480–719`：Internal Outline；
4. `720–959`：Shadow Map；
5. `960–1199`：Beauty + HUD。

正式配置：

- 1920×1080、60 fps、1200 帧、20.000000 秒；
- 每章 240 帧，即精确四秒；
- Portfolio Orbit、Idle Animation 和 Joint Palette 在切换处保持连续；
- `--technical-sequence` 自动启用 Portfolio 和 GPU Timestamp；
- 总帧数必须能被五整除，否则在窗口初始化前拒绝运行。

输出：

- PNG：`captures/s32_technical_1080p60_20s`；
- MP4：
  `captures/Afterglow_S32_TechnicalBreakdown_1080p60_20s.mp4`；
- 原始序列 1200 张、0 缺帧，共 658,909,553 bytes，约 628.4 MiB；
- MP4 大小 15,044,923 bytes；
- MP4 SHA-256：
  `184D70E40F9525AFE97825DBD11D0BA51B9FC1A8C62FD2403E6EB0199B571A75`。

视频规格：

- H.264 High Profile；
- YUV420p；
- BT.709 Range、Space、Transfer 和 Primaries；
- 60/1 fps；
- 1200 frames；
- 20.000000 seconds；
- 无音轨。

视觉 QA 检查正式帧 120、360、600、840、960、1080、1199，并在 25 帧
Debug 探针中检查所有章节首帧。五种输出与当前环绕角度一致，最后一章 HUD
方向、缩放和数据均正常；未发现半透明、内部结构穿透、骨骼撕裂、阴影错位
或章节状态延迟一帧。

## S33：技术序列原生标题与转场

S33 仍不修改莱万汀 GLB、贴图或材质参数。新增内容全部位于 Renderer 的
HUD 几何叠加层，仅在 `--technical-sequence` 中启用：

- 五章使用居中英文标题和技术副标题；
- 每章首尾加入由帧号控制的深色淡入淡出；
- 1920×1080、60 fps 正式版每次过渡为 21 帧，标题显示 120 帧；
- 第五章实时 GPU HUD 在淡入完成后出现；
- 动画、相机环绕和蒙皮状态在过渡期间继续运行，不重置时间线；
- Manifest 新增 `technicalFadeFrames: 21` 与
  `technicalTitleFrames: 120`。

正式输出：

- PNG：`captures/s33_technical_titles_1080p60_20s`；
- MP4：
  `captures/Afterglow_S33_TechnicalTitles_1080p60_20s.mp4`；
- 1200 张 PNG、0 缺帧，原始序列 635,212,913 bytes；
- MP4 为 16,007,831 bytes；
- SHA-256：
  `9915F887B0B70350711E6243AD357DB076FEB302A76CBA8D53D43AC4207B0DCB`；
- H.264 High、YUV420p、BT.709、1920×1080、60 fps、20.000000 秒。

资产兼容性回归：

- 普通 Beauty 单帧与 S30 基准 SHA-256 完全相同：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 因此标题和转场没有进入莱万汀的普通截图或 Beauty 视频路径；
- 私有动态角色 150 帧 Debug Validation 与公开静态模型 120 帧回归均通过；
- 未发现半透明、隐藏口腔/手臂穿透衣物、蒙皮撕裂或章节切换错位。

## S34：作品集交付包与资产公开边界

S34 不修改莱万汀模型、贴图、材质、骨骼或动画。它把已经验证的输出整理到
`portfolio/`，并明确区分可公开的 Renderer 内容与不应重新分发的角色资产。

新增内容：

- `portfolio/README_CN.md`：面试展示顺序、项目简介、技术亮点和资产声明；
- `portfolio/portfolio_manifest.json`：五项最终 Artifact 的相对路径、大小和
  SHA-256，以及技术视频章节和 GPU Timing 摘要；
- `portfolio/images/afterglow_cover_1920x1080.png`：S33 Beauty 帧生成的
  作品集封面；
- `portfolio/images/technical_contact_sheet_1920x1080.png`：五种 Renderer
  输出的 3×2 联系表；
- `tools/build_portfolio_package.ps1`：验证源文件并重新生成上述派生内容。

公开建议：

- 可公开 Renderer 源码、Shader、构建说明、程序化 Idle 生成工具和
  `assets_public/test_model.gltf`；
- 可在个人作品集页面展示最终截图和视频，但应标注角色资产用于非商业技术
  演示；
- 不把 `assets_private/laevat_skinned`、原始 Unreal 贴图或大型 PNG 序列
  推送到公开源码仓库；
- `portfolio_manifest.json` 只记录路径和校验信息，不嵌入或复制角色 GLB；
- 两个 MP4 由索引引用，未在 `portfolio/` 中重复保存。

视觉输出：

- 封面为 1920×1080，SHA-256：
  `BF076B2EB90642C34B4B5E55C033DB328C1ED0F957853898373CE2E709712EB0`；
- 联系表为 1920×1080，SHA-256：
  `84C542602126FA9E27188698287E5622E86C594566C9421F8223908ACF4EFBF7`；
- 两者均通过视觉 QA，没有文字裁切、透明错误或角色内部结构穿透。

## S35.1：AzureRender 命名迁移兼容性

S35.1 只迁移 Renderer 的构建目标、主类、运行时标题和外部品牌，不修改
莱万汀 GLB、贴图、材质、Skeleton、Joint Palette 或 Animation Channel。

迁移后状态：

- 使用 `AzureRender.exe` 加载
  `assets_private/laevat_skinned/laevat_idle_material.glb`；
- 81,487 vertices、284,673 indices、14 primitives、15 materials 保持不变；
- 468 个 Joint Matrix 正常上传；
- 动画仍使用 GLB 中的 `Afterglow_ProceduralIdle` Legacy Name；
- `afterglow*` 材质 extras 继续由 `GltfLoader` 读取；
- 普通捕获继续写出 `Afterglow PNG sequence v1`，保证旧编码脚本兼容。

验证：

- Release 1920×1080 Beauty 单帧与 S30 基准 SHA-256 完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 25 帧私有动态角色 Debug Validation 技术探针通过；
- 新 HUD 显示 AzureRender 品牌；
- 未出现半透明、隐藏口腔/手臂穿透衣物、蒙皮撕裂或材质丢失。

因此后续目录和代码结构重构可以继续进行，但不得直接全局替换
`afterglow*` 键。若未来引入 AzureRender v2 GLB Schema，应同时保留旧键读取。

## S35.2：Translation Unit 拆分资产回归

`AzureRenderSupport.cpp` 与 `AzureRenderCapture.cpp` 的拆分不修改莱万汀资产或
Loader。私有角色仍由同一个 `AzureRenderApp` 实例持有，GPU Skinning、材质
Descriptor、Animation Timeline 和 Joint Palette 的字段位置及更新顺序不变。

验证结果：

- Release 1920×1080 Beauty 与 S30 基准 SHA-256 完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 25 帧 Debug Technical Sequence 正确遍历五个章节；
- Capture/Timing 移入独立 `.cpp` 后仍能输出 Manifest 和 25 个 Timestamp 样本；
- 81,487 vertices、284,673 indices、14 primitives、15 materials、468 Joint
  Matrix 均保持不变；
- 未出现半透明、隐藏结构穿透、材质丢失或骨骼撕裂。

## S35.3：Frame 拆分资产兼容性

`AzureRenderFrame.cpp` 只承接现有 Draw、UBO/HUD 更新和 Command Recording
成员函数；`AzureRenderInternal.hpp` 只承接原有 inline 数学与 `vkCheck`。
莱万汀 GLB、Loader、材质 Descriptor、Joint Palette、动画时间线和每帧更新顺序
均未修改。

验证结果：

- 运行时统计仍为 81,487 vertices、284,673 indices、14 primitives、
  15 materials 与 468 Joint Matrix；
- 动画仍解析为 `Afterglow_ProceduralIdle`，Manifest 仍使用
  `Afterglow PNG sequence v1`；
- Release 1920×1080 Beauty 输出位于
  `captures/s35_frame_beauty_regression/frame_000000.png`；
- SHA-256 与 S30/S35.1/S35.2 基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 25 帧 Debug Technical Sequence 正确遍历五章并收集 25 个 Timestamp 样本；
- 未出现半透明、隐藏口腔或手臂穿透衣物、材质丢失、蒙皮撕裂或章节状态错位。

因此后续 Pipeline/Resource Creation 拆分可继续沿用相同回归门槛；任何 Beauty
哈希变化都必须视为逻辑变化并单独解释，不能归因于文件移动。

## S35.4：Pipeline 拆分资产兼容性

`AzureRenderPipeline.cpp` 只移动现有 Render Pass、Graphics Pipeline 与
Framebuffer 创建函数。莱万汀资产数据、Loader、Material Descriptor、Pipeline
State 数值、Attachment 顺序、Joint Palette 与 Animation Timeline 均未修改。

回归结果：

- 运行时仍报告 81,487 vertices、284,673 indices、14 primitives、15 materials、
  468 Joint Matrix 和 `Afterglow_ProceduralIdle`；
- Release 1920×1080 Beauty 位于
  `captures/s35_pipeline_beauty_regression/frame_000000.png`；
- SHA-256 与既有基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 25 帧 Debug Technical Sequence 正确遍历 Beauty、World Normal、Internal
  Outline、Shadow Map 与 Beauty + HUD；
- 未出现半透明、隐藏口腔/手臂穿透衣物、Framebuffer attachment 错位、材质
  丢失或蒙皮撕裂。

因此 Pipeline 文件拆分没有改变莱万汀的渲染兼容性。后续 Descriptor/Resource
拆分必须继续保留当前 Binding 编号、每帧资源数量、Image Layout 与销毁顺序。

## S35.5：Descriptor/Resource 拆分资产兼容性

`AzureRenderDescriptors.cpp` 和 `AzureRenderResources.cpp` 只移动原成员函数，
没有修改莱万汀材质数据或资源 ABI。主材质 Descriptor 仍使用 Binding 0–10，
Post-process 仍使用 Binding 0–2；每帧 Uniform/Joint Buffer、每材质纹理和 Shadow
资源的创建及写入顺序保持不变。

回归结果：

- 81,487 vertices、284,673 indices、14 primitives、15 materials、468 Joint
  Matrix 和 `Afterglow_ProceduralIdle` 均保持不变；
- Release Beauty 位于
  `captures/s35_resource_beauty_regression/frame_000000.png`；
- SHA-256 与既有基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 25 帧 Debug Technical Sequence 的 Beauty、World Normal、Internal Outline、
  Shadow Map 和 Beauty + HUD 均正常；
- 未出现 Descriptor 错配、默认贴图丢失、Shadow 采样错误、半透明、隐藏口腔/
  手臂穿透衣物或蒙皮撕裂。

因此后续路径与工程目录迁移不应再触碰资产资源布局；目录改名后的验收仍必须加载
同一私有 GLB，并重复 Beauty 哈希与五章节技术序列。

## S35.6：Unreal 工具路径可移植性

七个 `tools/unreal_*.py` 工具已移除 `MyVulkanApp` 绝对输出路径。新的根目录规则：

1. 若存在 `AZURERENDER_PROJECT_ROOT`，使用其展开后的绝对路径；
2. 否则由脚本 `__file__` 所在 `tools/` 目录向上一级解析项目根目录；
3. 若 Unreal 执行环境不提供 `__file__`，明确要求设置环境变量。

七个脚本均通过 AST 语法检查、默认脚本相对解析测试和环境变量覆盖测试。另在
`Project/AzureRender_s35_path_probe` 临时根目录完成全新 Debug/Release 构建，
Release 构建读取原私有 GLB 后的 Beauty SHA-256 仍为：

`8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`

临时探针已删除。下一步正式目录改名不会修改 Unreal 源资产、私有 GLB、贴图、
材质 JSON 或动画数据；改名后脚本应自动把输出写入新的 AzureRender 根目录。

## S35.7：正式目录改名资产回归

项目现位于 `Project/AzureRender`。七个 Unreal 工具会由各自 `tools/` 路径自动解析
新根目录；私有资产目录随工程同卷移动，没有复制、重新导出或修改内容。

新路径下使用全新 Debug/Release Cache 验证：

- 81,487 vertices、284,673 indices、14 primitives、15 materials；
- 468 Joint Matrix 与 `Afterglow_ProceduralIdle`；
- Release Beauty：
  `captures/s35_root_rename_beauty_regression/frame_000000.png`；
- SHA-256 与正式改名前完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 25 帧五章节 Debug Technical Sequence 完整；
- 未出现资产路径丢失、贴图/材质缺失、半透明、隐藏口腔或手臂穿透衣物、
  Shadow 错误或蒙皮撕裂。

因此根目录迁移没有改变莱万汀资产或 Renderer 输出。后续 S36 色彩管线会有意改变
Beauty 像素，必须建立新的视觉基准，但 S35 最终哈希继续作为“结构重构零变化”的
冻结证据。

## S36.1：色彩管线基线冻结

S36.1 不修改莱万汀 GLB、Texture、Material、Descriptor 或 Shader Attachment。
新增代码只查询目标 GPU 是否支持 `VK_FORMAT_R16G16B16A16_SFLOAT` 的采样、颜色
附件和颜色附件混合能力。

验证：

- RTX 4060 Laptop GPU 报告目标格式 `supported`；
- Debug/Release 构建与公共 Validation 通过；
- 莱万汀 LDR 冻结帧位于
  `captures/s36_ldr_color_baseline/frame_000000.png`；
- SHA-256 仍为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 未出现半透明、隐藏结构穿透、材质丢失或蒙皮错误。

S36.2 会首次有意改变 Beauty 输出。届时必须重点检查肤色、头发高光、金属武器、
Emissive、地台阴影和背景黑位，并建立新的 HDR/Tone Mapping 后基准。GLB 与材质
参数不应为了抵消 Tone Mapping 而在同一节点同步修改，否则无法区分色彩管线影响。
