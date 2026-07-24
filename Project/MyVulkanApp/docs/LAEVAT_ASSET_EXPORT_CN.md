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
.\build\ninja-debug\MyVulkanApp.exe `
  --asset ".\assets_private\laevat_static\laevat_static_material.glb"
```

自动测试：

```powershell
.\build\ninja-debug\MyVulkanApp.exe `
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
Project\MyVulkanApp\assets_private\laevat_static
```

`assets_private/` 不应提交到公开仓库。

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
