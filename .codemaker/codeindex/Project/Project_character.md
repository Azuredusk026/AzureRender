---
type: "Fragment"
id: Project/character
title: "风格化角色渲染与资产管线"
description: "角色资产如何从 glTF 与 Material Profile 变成分层光照结果，聚簇光照、级联阴影与 PCSS、Face SDF、Hair KK、描边与眉毛 Overlay 各自的约束是什么。"
parent: /Project/_overview.md
fragment: character
entity_names:
  constants:
    - name: kClusterGridX
      value: "16"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kClusterGridY
      value: "9"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kClusterGridZ
      value: "24"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kMaxSceneLights
      value: "128"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kShadowCascadeCount
      value: "4"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kEnvironmentMipLevels
      value: "7"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kIblPrefilterSamples
      value: "32"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kSharedTextureSlots
      value: "3"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kMaterialTextureSlots
      value: "8"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: kAssetVertexWordCount
      value: "26"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: MaterialPushConstants 尺寸
      value: "128 bytes"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: MorphPushConstants 尺寸
      value: "80 bytes"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: InstanceGpuData 尺寸
      value: "400 bytes"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: SkinningPushConstants 尺寸
      value: "16 bytes"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
    - name: 级联分割对数权重
      value: "0.65（near 0.1 / far 100.0）"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.cpp
    - name: 阴影图集布局
      value: "2 × 2（每级 1024 × 1024）"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.cpp
    - name: PCSS blocker 搜索采样数
      value: "12"
      source: Project/AzureRender/shaders/mesh.frag
    - name: PCSS 过滤采样数
      value: "16"
      source: Project/AzureRender/shaders/mesh.frag
    - name: PCSS 半影范围 clamp
      value: "1.0 – 16.0（texel）"
      source: Project/AzureRender/shaders/mesh.frag
    - name: AssetAlphaMode
      value: "Opaque=0, Mask=1, Blend=2"
      source: Project/AzureRender/src/assets/GltfLoader.hpp
    - name: AssetMaterialClass
      value: "Generic=0, Skin=1, Face=2, Hair=3, Fabric=4, Metal=5, Eye=6, Overlay=7, Emissive=8, Showcase=9"
      source: Project/AzureRender/src/assets/GltfLoader.hpp
    - name: MaterialFeatureStylizedShadow / HairAnisotropy / FaceSdfEligible / EmissiveMask / Overlay / NeutralFallback / BrowOverlay
      value: "1<<0 / 1<<1 / 1<<2 / 1<<3 / 1<<4 / 1<<5 / 1<<6"
      source: Project/AzureRender/src/assets/GltfLoader.hpp
    - name: AssetFaceSdfProfile::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/assets/GltfLoader.hpp
    - name: AssetMaterial::materialProfileVersion 默认
      value: "1"
      source: Project/AzureRender/src/assets/GltfLoader.hpp
    - name: AssetFaceSdfChannel
      value: "Red=0, Green=1, Blue=2, Alpha=3"
      source: Project/AzureRender/src/assets/GltfLoader.hpp
    - name: 眉毛局部推出量（glTF 米制）
      value: "0.04679 m（原实例 4.679 厘米）"
      source: Project/AzureRender/docs/character-rendering.md
    - name: skin.comp local_size
      value: "64 × 1 × 1"
      source: Project/AzureRender/shaders/skin.comp
    - name: ibl_prefilter.comp local_size
      value: "8 × 8 × 1"
      source: Project/AzureRender/shaders/ibl_prefilter.comp
    - name: 展示平台环网格分段
      value: "96"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.cpp
    - name: IblPrefilterPushConstants 尺寸
      value: "8 bytes"
      source: Project/AzureRender/src/scenes/CharacterSceneRenderer.hpp
retrieval_hints:
  - "角色材质分类是怎么定的，改了 Class 会有什么后果？"
  - "为什么要给点光源建 16×9×24 的聚簇网格？"
  - "PCSS 的 blocker 搜索与半影过滤分别在几步？"
  - "Face SDF 打开后画面没变化，应该先查什么？"
  - "头发的高光条带由哪几张纹理和哪些参数决定？"
  - "眉毛、睫毛和眼睛共用一个 Primitive 时为什么不能整体缩放？"
  - "⚠️ 你要找的是黑洞的光线积分与时间累积，不在这里，在 Project_blackhole"
  - "⚠️ 你要找的是场景文档格式与编辑器字段，不在这里，在 Project_scene_editor"
  - "本子系统也叫 Character Renderer / 角色渲染，需求里的『脸』『发丝』『描边』『软阴影』都归这里"
  - "角色专用的光照/材质逻辑只能改 `shaders/mesh.*` 与 `CharacterSceneRenderer.*`，不得为某一套角色资产写 Shader 分支"
architectural_role: "风格化角色渲染子系统，必须在公共宿主 Attachment 内产出，禁止自建后处理链路"
---

## 业务意图

角色渲染解决的是"同一套 PBR 公式无法同时满足二次元脸、发丝、布料与金属的表现需求"这个问题：它按材质类别给出各自的 Ramp、AO、镜面与轮廓规则，把美术设计好的脸部阴影形状编码进 Face SDF，并把点光源、级联阴影、Hair 双层 Kajiya-Kay、屏内/外壳描边与眉毛 Overlay 组合成可被固定机位复核的画面，而不是靠单一光照模型套所有表面。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| glTF Material `extras.azureRenderMaterial` | 资产→Loader | `schemaVersion`, `class`, `features[]`, `styleParameters[4]`, `featureParameters[4]` | 类别决定基础规则，Feature 只负责开关能力 | `src/assets/GltfLoader.cpp` |
| `faceSdf` Profile | 资产→运行时 | `schemaVersion`, `texture`, `texCoord`(=0), `channel`, `maskChannel`, `shadowOnLowValues`, `horizontalAxis`, `headNode` | 只有 Feature 而无 Profile 与有效纹理时绑定回退纹理 | `src/assets/GltfLoader.hpp:AssetFaceSdfProfile` |
| `LoadedAsset` | Loader→Renderer | 顶点（Position/Normal/Tangent/UV/Color/Joint/Weight/Morph0/Morph1）、Primitive、Node 层级、Skin、Animation、材质与纹理 | CPU 侧解析结果，上传与分 Descriptor 由 Renderer 负责 | `src/assets/GltfLoader.hpp` |
| `ApplyShowcasePresetLook` / Look Catalog | 配置→设置 | 5 个索引：Azure Gallery / Endfield Industrial / Neutral Material Check / Specular Rim / Rear Emissive | Look 只拥有 Grade、Bloom、Outline | `src/render/RenderSettings.cpp` |
| 角色快捷键 | 输入→Renderer | `1`-`4` 机位、`5` 脸部近景、`6` 环绕展示、`F4` 动画暂停、`F9` 风格化开关、`F10` 内部描边、`F12` 保存 PNG | 人工 QA 的固定观察面 | `src/app/AzureRenderApp.cpp:onAnimationKey` |
| `--qa-camera` / `--qa-light` / `--qa-effect` / `--qa-isolation` | CLI→Renderer | 5 机位、4 灯光、10 效果、21 个隔离视图 | 隔离图用于分辨材质、光照与后处理各自的贡献 | `src/app/CommandLine.cpp` |
| `ShaderFeatureDescriptor("character.material"/"character.outline")` | 构建→校验 | `mesh.vert/frag`, `outline.vert/frag` | Shader 归属登记 | `src/scenes/BuiltinRendererCatalog.cpp` |
| `schemas/azure_render_material.schema.json` | 工具→资产 | 结构依据 | 外部工具校验用；运行时以 C++ Loader 为准，两边必须同步 | `Project/AzureRender/schemas/azure_render_material.schema.json` |

## 跨模块依赖

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project_render_core` | 环境资源、上传环、Compute Pass、RenderSettings | `loadEnvironmentImage`, `UploadRingBuffer`, `ComputePass` | extracted |
| `Project_host` | 阴影图、HDR Scene Color、Depth、Normal、公共合成 | `RenderContext::shadowRenderPass`, `kShadowMapSize` | extracted |
| `Project_renderer_sdk` | 生命周期与能力声明 | `ISceneRenderer` | extracted |
| `Project_scene_editor` | 场景快照、节点变换与点光源列表 | `SceneDescription`, `SceneLight` | extracted |
| `src/assets` | glTF 与纹理解码（tinygltf、stb） | `GltfLoader` | extracted |
| `shaders/*` | 角色私有 Pipeline 使用 `mesh.*`、`outline.*`、`skin.comp`、`ibl_prefilter.comp` | `mesh.frag` | extracted |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|--------|---------|---------|
| `Project_host` | 公共后处理读取角色写出的 HDR Color/Depth/Normal | `PostProcessPushConstants` |
| 视觉回归与 QA | 固定机位与隔离视图生成公共基线 | `tools/visual_regression_cases.json` |
| `Project_scene_editor` | 编辑器 Inspector 修改材质、Look、Face SDF、Shadow、Outline | `EditorContext::renderSettings` |

## 典型调用链

### 一帧角色渲染

```
ISceneRenderer::updateFrame(SceneFrameData)
  → 更新节点动画 / Joint Matrix / morphWeights → 写入当前帧槽 Joint Buffer
  → 透明 Primitive 计算视图相关排序索引
ISceneRenderer::recordScene(RenderContext)
  → RenderGraph 声明 Shadow / Main / Outline Pass                       ← 跨子系统：render_core
  → 阴影 Pass：4 级联光源视投影矩阵 → 2×2 深度图集
  → skin.comp dispatch（computeSkinning 为真）或 mesh.vert 回退           ← 跨子系统：render_core
  → mesh.frag：聚簇查表 → PCSS → 分类 Ramp → Face SDF → Hair KK/AO
  → outline.*：背面壳 Silhouette Outline
  → 宿主 Composite：inner_outline.frag 采样 Depth/Normal → Bloom → Grade → Tone Mapping
```

### 点光源到片元

```
.azscene lights[] → SceneLight nodeId → resolveNodeWorldTransforms
  → LightBuffer::setLights(按 stableId 排序, 截断 128) → 视图空间位置
    → ClusteredLightGrid::assign（16×9×24，对数深度分层）
      → clusterHeads / clusterIndices 三个 Storage Buffer
        → mesh.frag 用片元屏幕坐标 + 前向深度查聚簇 → 半径衰减 + 漫反射 + 高光
```

## 实现约束清单

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------------------|
| `AssetMaterialClass` | `0`–`9` | `GltfLoader.hpp` | 类别是基础规则来源 | 名称猜测会让材质规则随命名漂移；枚举值进 Profile 与 `material-id` 隔离图 |
| `AssetMaterialFeature` | `1<<0`–`1<<6` | `GltfLoader.hpp` | 能力开关位 | Feature 与 Class 不能互相代替 |
| `AssetAlphaMode` | `0/1/2` | `GltfLoader.hpp` | Opaque/Mask/Blend | Blend 走排序路径，禁止误写深度 |
| `kClusterGridX/Y/Z` | `16/9/24` | `CharacterSceneRenderer.hpp` | 聚簇网格 | `kMaxClusterLightIndices` 与 Storage Buffer 容量由三维乘积决定，改一维即需同步三处缓冲 |
| `kMaxSceneLights` | `128` | `CharacterSceneRenderer.hpp` | 每帧上传光源上限 | 超出由 `LightBuffer::setLights` 截断；性能脚本按 0/16/64/128 采集 |
| `kShadowCascadeCount` | `4` | `CharacterSceneRenderer.hpp` | 级联数 | 与 2×2 图集、级联 viewport、`NullRhiPass` 断言绑定 |
| `kEnvironmentMipLevels` / `kIblPrefilterSamples` | `7` / `32` | `CharacterSceneRenderer.hpp` | IBL 预滤波层数与采样数 | Mip 共享同一图像，按子资源屏障逐层写入 |
| `kSharedTextureSlots` / `kMaterialTextureSlots` | `3` / `8` | `CharacterSceneRenderer.hpp` | Bindless 布局：共享槽（环境/阴影/Ramp）在前，随后每材质 8 槽 | 槽位偏移经 Push Constant 传递，改动会同时影响固定表路径 |
| `MaterialPushConstants` | `128` bytes | `CharacterSceneRenderer.hpp`（`static_assert`） | 每次 Draw 的材质参数 | 128 是 Vulkan 保证的最小 Push Constant 容量，继续扩展必须迁 UBO/SSBO |
| `AssetVertex` | `26` 个 `uint32`（`104` 字节） | `CharacterSceneRenderer.cpp`（`static_assert`） | 顶点布局字长 | `skin.comp` 以 `vertexWordCount = 26u` 解析同一缓冲，两处必须一致 |
| `maximumFilterRadiusTexels` | `8.0`（范围 1–16） | `RenderSettings.hpp` | PCSS 半影上限 | Shader 内 `maximumRadius` 同区间 clamp |
| 级联分割对数权重 | `0.65` | `CharacterSceneRenderer.cpp` | 对数与均匀距离混合 | 改它会同时改变近处精度与远处覆盖 |
| 眉毛局部推出量 | `0.04679 m` | 资产 Profile | 沿 Pixel-to-Camera 方向的厚度补偿 | 原实例单位为厘米；写成 `4.679 m` 会把眉毛推离角色 |

### 必须包含的协议字段（资产契约）

| 契约 | 字段 | 类型 | 说明 |
|------|------|------|------|
| Material Profile v1 | `schemaVersion` | int =1 | 缺失即按 `neutral-fallback` 处理 |
| Material Profile v1 | `class` | string | 十个类别名之一，禁止用材质名代替 |
| Material Profile v1 | `features[]` | string[] | 仅表示"可以使用"，不表示数据已提供 |
| Material Profile v1 | `styleParameters[4]` / `featureParameters[4]` | float[4] | 分量含义由 Class 决定；Brow Overlay 复用同一向量表示局部厚度与偏移 |
| Face SDF Profile v1 | `texCoord` | int = `0` | 固定 UV Set 0 |
| Face SDF Profile v1 | `channel` / `maskChannel` | `r/g/b/a` | 距离通道与遮罩通道 |
| Face SDF Profile v1 | `horizontalAxis` | `left-to-right` / `right-to-left` | 脸部语义方向，自动推断不可靠 |
| Face SDF Profile v1 | `headNode` | string | 头部节点名，用于构造 Bind Pose 基底 |

### 必须在角色 Shader 中保留的处理

| 处理 | 所在 | 说明 |
|------|------|------|
| 头部局部基底与相对旋转 | `mesh.frag` | 用 `R = M_head,current · M_head,bind⁻¹` 把世界主光转到脸部语义坐标，否则转动会把脸长期锁在全亮 |
| 左右镜像连续混合 | `mesh.frag` | 以 `smoothstep(-ε, ε, l_x)` 在镜像与原始 UV 距离间插值；硬分支会在侧向光过零时整脸突变 |
| PCSS 两步 | `mesh.frag` | 12 次 blocker 搜索求平均遮挡深度，再按 `z_r - z_b` 估计半影并做 16 次过滤 |
| 双层 Kajiya-Kay | `mesh.frag` | 由 HN 的 BA、材质 Shift 与法线构造两条错开的发束方向 |
| Hair 色相保护 | `mesh.frag` | 环境镜面可能把红发照成灰白，漫反射在环境与 Toon 合成后恢复部分 Base Color 色相 |

### 边界约束（能做 / 禁止）

- 禁止：为某一套具体角色资产在 Shader 中硬编码材质名或最终颜色；Shader 只允许通用材质类别逻辑（来源：`docs/archive/legacy/MASTER_DEVELOPMENT_PLAN_CN.md`）。
- 禁止：Silhouette Outline 整体缩放 Clip Space `xy`；必须沿法线外扩并使用 Front Face Culling，否则头部过宽且远近宽度不一致。
- 禁止：围绕单一中心缩放整个眉毛 Primitive（含眉毛、睫毛与眼部小岛，骨骼不同）；禁止把 Index Buffer 偏移再加到顶点索引上（会读到错误顶点，表现为眉毛不可见）。
- 禁止：Normal/HN/打包数据纹理走 sRGB 解码（来源：`docs/assets-and-editor.md`）。
- 禁止：把材质 Feature 当作"数据已存在"；只有 Profile 与有效纹理同时存在才允许 UI 显示效果已开启。
- 禁止：把 `assets_private/` 的角色与派生媒体用于 CI、安装包或作品集（来源：`assets_private/README.md`）。
- 禁止：提高曝光来修复数据绑定问题（Hair KK 隔离图全黑先查 Class/Feature/HN Binding）。
- 允许：每 Primitive 最多两个 Morph Target；超过上限由 Loader 报告数量并拒绝导入。
- 边界：`generic` 作为保守 fallback，Skin/Face/Hair/Fabric/Eye 按介电质处理并限制 Metallic 上限；只有 Metal 走完整 Metallic/F0 路径。

### 设计决策

| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 材质表达 | 枚举 Class + Feature 位 + 定长参数向量 | 每种材质独立 Profile 字段 | 减少重复字段；代价是写入工具必须知道目标类别 |
| 纹理绑定 | 设备支持 Descriptor Indexing 时用全局纹理数组，否则逐材质固定表 | 只保留一条路径 | 两条路径共用同一份 Shader 源码条件编译变体，`--bindless-disabled` 可强制回退以保回归 |
| 蒙皮位置 | 设备支持时 `skin.comp` 计算，否则顶点着色器回退 | 只保留顶点蒙皮 | Compute 路径可复用输出给阴影/轮廓/主材质；回退路径保证降级可用 |
| 脸部阴影 | 资产 SDF 控制形状 + 实时 Shadow Map 处理遮挡 | 纯几何法线推导 | 二次元脸的分区来自美术设计，无法从法线可靠推导 |
| 透明 | 按视图排序的 Alpha Blend 路径 | 通用 OIT | 当前角色数量与材质规模下简单可靠；多层大面积透明仍有排序限制 |

## 变更风险

- 改 `MaterialPushConstants` 布局、Bindless 槽位定义或顶点字长：同时影响 GLSL、Descriptor Layout/Pool/写入、回退资源与 `skin.comp` 解析，需重新验证固定表与 Bindless 两条路径，`docs/reference.md` 与 Schema 一并同步。
- 改 `AssetMaterialClass` 取值或 Feature 位：已发布的私有角色资产 Profile 会整体失效或错分类，`material-id` / `style-mask` 隔离图与视觉基线同时漂移。
- 改 PCSS 步数、半影上限或级联权重：属于"已验收的视觉特征"而非纯性能参数，过滤参数与级联配置必须分离（来源 commit：`docs(plan): 重写引擎化路线为E0-E7结构替换队列`）。
- 改点光源数量上限或聚簇网格：`clustered_lights_16.azscene` 与 `AzureRender.ClusteredLightingGpu` 用例、`NullRhiPass` 的级联 viewport 断言同时受影响。
- 改 `kSharedTextureSlots`/材质槽位起点：Web 暴露的症状是部分材质采到错误纹理而其它材质正常，最易被误判为资产问题。
- 新增隔离视图而未同步 `tools/visual_regression_cases.json`：新视图不会被自动基线覆盖，仍依赖人工检查。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/character-rendering.md`、`Project/AzureRender/docs/assets-and-editor.md`、`Project/AzureRender/docs/runtime/clustered-lighting.md`、`Project/AzureRender/docs/runtime/compute-passes.md`、`Project/AzureRender/docs/reference.md`（原文已提炼，非完整转录）
