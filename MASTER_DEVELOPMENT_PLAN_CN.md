# AzureRender FYP 长期开发主计划

> **版本：** 2.0  
> **状态：** Active / 后续开发唯一总路线图  
> **项目：** AzureRender — Vulkan 风格化角色与场景渲染器  
> **研究课题：** Comparative Evaluation of Vulkan Subpasses and Dynamic Rendering Local Read for Real-Time NPR Rendering  
> **最终 DDL：** 2026-12-25（本计划不分配工期，只规定依赖顺序和退出条件）  
> **当前主优先级：** 先达到作品集级角色美术质量，再完成场景与作品集发布，随后冻结研究工作负载并开展三路径实验。

---

## 0. 文档权威、使用方法与变更规则

### CQ-3～CQ-6 当前状态（2026-08-14）

- `azureRenderMaterial.faceSdf` v1 契约、公共 PNG 资产和私有 CQ-3 GLB 已建立。
- Loader 已将任意输入通道规范化为 `R=距离、A=参与遮罩`；descriptor binding 12、Head-local 光向量 UBO、Face Shader 分带和 `face-sdf` QA effect/isolation 已接入。
- CQ-3 Face SDF/Overlay、CQ-4 双层 Hair KK、CQ-5 Rim/Specular/Emissive/Bloom v1 均已完成 Debug/Release 构建与 enabled/disabled/isolation 捕获。Bloom 当前为轻量 HDR Scene Color 后处理，不代表论文三路径 benchmark。
- CQ-6 已完成版本化 Outline/Grade 参数、最终合成 ABI、Manifest/状态哈希闭环和 Release Vulkan 代表捕获。M2 技术门禁已通过公共/私有 Debug Validation、动画长跑和 Resize 生命周期回归；当前只剩用户视觉确认。

### 0.1 文档层级

本项目从现在起使用以下权威顺序：

1. `DMT2309242-WuChenfeng-Proposal.docx`：研究问题、实验目标和论文范围的最高依据。
2. 本文件：产品、作品集、工程、研究与提交的长期总路线图。
3. 节点专项设计文档：只解释当前主计划节点的实现细节，不得扩大总范围。
4. `Project/AzureRender/docs/DEVELOPMENT_LOG_CN.md`：事实日志和验证证据，不负责决定方向。
5. `Project/AzureRender/docs/PROJECT_HANDOFF_CN.md`：当前可运行状态和恢复入口。

如果专项计划、开发日志或临时对话与本计划冲突，以本计划为准；如果本计划与 Proposal 的研究范围冲突，先建立 Scope Change Decision，再修改本计划。

### 0.2 每次开发必须遵守的循环

每次开发开始前必须明确：

- 当前属于哪个里程碑和 Work Package；
- 本次只完成哪个可独立验证的 Vertical Slice；
- 退出条件是什么；
- 需要保存什么截图、Capture、日志、数据或文档证据；
- 哪些内容明确不在本次范围内。

实现后必须依次完成 Build、运行验证、视觉或数值检查、证据保存、日志更新和本计划状态更新。没有满足退出条件时，不得用“后面再修”进入依赖节点。

### 0.3 状态定义

| 状态 | 定义 |
|---|---|
| Pending | 前置依赖未满足，不能开始。 |
| Ready | 前置依赖满足，可领取为唯一 Active 节点。 |
| Active | 当前唯一主要开发节点。 |
| Review | 实现完成，正在执行视觉、技术或研究验收。 |
| Complete | 退出条件全部满足且证据已保存。 |
| Blocked | 外部能力、资产、设备或决定阻塞；必须记录替代方案。 |
| Deferred | 明确不阻塞主目标，推迟到 Must 项完成之后。 |
| Rejected | 经验证不适合项目，保留原因，不重复实现。 |

### 0.4 范围变更门槛

只有出现以下情况才修改长期路线：

- Proposal、Supervisor 或学校提交要求发生变化；
- 目标设备不支持 DRLR，导致研究矩阵需要调整；
- 资产许可或关键数据不可用，无法达到既定视觉目标；
- 关键架构被实验事实证明不能满足公平比较；
- 用户明确改变作品集或研究的最终目标。

普通 Bug、调参失败、新想法或某个实现较困难，都不能直接改变主路线。

---

## 1. North Star：项目最终目的

### 1.1 一句话目标

构建一个可公开展示和解释的 C++17/Vulkan 风格化角色与工业科幻场景渲染器，以高质量类《终末地》角色画面作为作品集案例，并在冻结的 deferred NPR 工作负载上公平比较 Multi-pass、Vulkan Subpasses 与 Dynamic Rendering Local Read。

### 1.2 四条成功主线

| 主线 | 最终成功定义 |
|---|---|
| 美术表现 | 角色脸部、头发、皮肤、布料、金属、阴影、轮廓与自发光形成清晰可辨的统一风格；最终画面不依赖“代码里存在效果”的解释。 |
| Renderer 工程 | Windows Release 稳定、Validation clean、资源生命周期可靠、可重现 Capture、公开资产回退可运行，架构能够承载多渲染路径。 |
| FYP 研究 | 三条路径共享同一工作负载和 Shader 数学；实验协议、原始数据、统计和结论可复现，不夸大平台或性能结论。 |
| 作品集与交付 | 有高质量图片、20 秒以上演示视频、技术分解、架构图、公开代码与完整 Case Study；论文和最终提交证据与工程一致。 |

### 1.3 项目级 Definition of Done

- 角色与场景达到本文第 6 节的视觉质量门槛，并获得用户最终确认。
- Windows Portfolio Release 可从干净环境构建或直接运行，常规操作、Resize、Capture 和长运行稳定。
- 公开仓库不包含无再分发许可的角色模型和贴图，但公共替代资产能验证完整渲染路径。
- Multi-pass、Subpass、DRLR 在支持设备上产生视觉等价输出；不支持的路径明确记录为 `NA`。
- 每个正式实验 Run 保存 Commit、Shader/Asset Hash、设备/驱动/API、配置、温控、电源和完整逐帧数据。
- 所有论文图表都能由原始数据和脚本重新生成。
- Thesis、Release、视频、演示、许可证、风险与限制说明完整，且最终结论逐项回答 Proposal 的 H1–H3。

### 1.4 明确的 Non-goals

以下内容不允许阻塞主目标：

- 通用游戏引擎、完整 ECS、可视化节点编辑器或通用资产商店；
- 大型开放世界场景、完整战斗系统、复杂角色 AI；
- 同时支持多种互不相关的 NPR 风格；
- 光线追踪、路径追踪、SSR、体积云等与研究问题无直接关系的系统；
- 在作品集画面未通过前提前开发 Subpass、DRLR 或 Android；
- 为追求抽象完美而重写已经稳定、可验证的 Vulkan 基础设施。

---

## 2. Proposal 对齐与双产品结构

### 2.1 研究问题保持不变

在完全相同的 deferred NPR 工作负载下，传统 Multi-pass、Vulkan Subpasses 与 DRLR 在 attachment 数据流、GPU 时间、CPU command recording 时间和 frame-time 稳定性方面是否存在可重复、平台相关的差异？

### 2.2 为什么项目分成 Portfolio Renderer 与 Benchmark Core

当前 AzureRender 是以角色画面为中心的 Forward Stylized Renderer，并通过 Depth/Normal Attachments 完成轮廓与诊断。Proposal 要求的是可控的 Deferred NPR Benchmark。二者共享资产、相机、材质数学、调试工具和视觉目标，但用途不同：

- Portfolio Renderer 允许为了最终画面使用 Hair、Face SDF、Rim、Bloom、HUD、动画和展示场景。
- Benchmark Core 只保留公平比较所需的固定 G-buffer、same-pixel toon lighting、公共 contour/composite 和确定性场景。
- 作品集画面通过后，才从已验证的视觉模型中提取 Benchmark Workload；不能反过来为了实验简化而牺牲作品集质量。
- Benchmark 冻结后，作品集的新美术效果不得静默进入实验路径；任何改变都需要提升 Protocol Version 并使旧数据失效。

### 2.3 三路径公平性核心

三条研究路径必须共享：

- 相同 Mesh、Draw Order、Camera、Light、Material Parameters 和 Random Seed；
- 相同 Attachment Format、Resolution、Clear Value、Sample Count；
- 来自同一 GLSL 源码和编译参数的等价 SPIR-V；
- 相同的 same-pixel toon lighting 数学和公共 contour/composite；
- 相同的计时边界、Warm-up、Sample Frame 和 Run Metadata。

允许变化的只有 Render Organization、Attachment Access Route、必要的 Layout/Dependency/Descriptor Binding。

---

## 3. 当前工程事实与差距基线

### 3.1 已完成并保留的基础

S1–S36.2 已形成以下可靠基础：

- Vulkan Instance、Validation、Device/Queue、Swapchain、Frames in Flight、Resize；
- glTF 资产、纹理、材质、Normal/Metallic/Roughness/Emissive、Alpha 与双面表面；
- Shadow Map、PCF、Inverted Hull、Depth/Normal Screen-space Outline；
- GPU Skinning、Animation、固定机位、Portfolio Orbit；
- 确定性 PNG Capture、20 秒视频工作流、Technical Sequence、HUD 与 GPU Timestamp；
- RGBA16F Scene Color、0 EV ACES fitted、Tone Mapping 后 HUD；
- AzureRender 品牌、目录和 Translation Unit 重构；
- 私有莱万汀资产与公共测试资产的隔离。

这些基础不应为了视觉调参被无目的重写。

### 3.2 角色美术审计结论

当前技术基础成熟度高于最终画面成熟度。现状不得描述为“角色渲染完成”：

| 模块 | 当前判断 | 主要缺口 |
|---|---|---|
| Face SDF | Missing | 莱万汀没有直接绑定 SDF Texture；当前只有通用光照与极弱 Matcap。 |
| Hair KK | Prototype | 使用 `_HN` 推测语义和解析 Ramp，真实 KK Ramp、Shift、Upper Limit、Camera Offset 未还原，画面不可辨识。 |
| Toon/Ramp | Prototype | 全材质共享 `smoothstep(N·L)`，不是材质感知 Ramp LUT；Ambient 冲淡阴影。 |
| Rim Light | Prototype | 强度和方向遮罩过弱，只在少数发梢产生颜色，不能塑造轮廓。 |
| Hair/Eye Shadow | Missing/Prototype | Overlay Primitive 存在，但 Unreal 的投影、Depth、Opacity 和颜色行为没有重建。 |
| Material Separation | Partial | 金属、布料、皮革和皮肤在最终明度与高光形状上区分不足。 |
| Emissive/Bloom | Partial | Emissive 数据存在，但没有 Bloom 和稳定的亮度层级。 |
| Outline | Partial | 技术路径完整，但线条偏黑、偏碎，压制材质细节。 |
| Lighting/Grade | Partial | 角色偏灰、偏平，暗部和发色受 Ambient/Tone Mapping 影响明显。 |

因此当前唯一 Active 主线必须是 **M1 Character Rendering Quality Foundation**，而不是 S36.3 性能或 Subpass。

### 3.3 当前可接受的历史基准

- S35/S36.1 LDR 基准用于证明重构没有改变像素，不再代表美术目标。
- S36.2 HDR Beauty v1 用于证明 HDR 路径稳定，不代表最终角色质量。
- 后续每个美术节点都要建立新的视觉基准；像素变化是预期行为，但必须保存 A/B、Isolation 和代表机位。

---

## 4. 长期架构目标

### 4.1 模块边界

| 模块 | 长期职责 | 禁止承担 |
|---|---|---|
| Platform | Windows/Android Window、Surface、Lifecycle、Input、文件访问。 | 材质逻辑和 Benchmark 统计。 |
| Vulkan Core | Device、Memory、Image/Buffer、Descriptor、Pipeline、Sync、Debug Label。 | 角色特例和展示场景规则。 |
| Asset Pipeline | UE 导出、glTF、Texture Channel、Manifest、License、Hash。 | 在 Shader 中猜测未知通道。 |
| Material System | Material Class、参数、Ramp/LUT、Texture Binding、Fallback。 | 把所有材质压进一个不可解释的 Push Constant。 |
| Portfolio Render Path | 高质量角色/场景渲染、动画、后处理、展示工具。 | 正式 Benchmark 计时。 |
| Benchmark Workload | 固定 G-buffer、same-pixel toon、公共 contour/composite。 | 作品集特有 Bloom、HUD、动态调参。 |
| Render Path Backend | Multi-pass/Subpass/DRLR 的组织差异。 | 改变 Shader 数学或资产。 |
| Capture/QA | Golden Image、A/B、Effect Isolation、Video、RenderDoc 证据。 | 修改渲染结果。 |
| Benchmark/Data | Runner、Timing、Metadata、CSV/JSON、统计脚本。 | 手工修正原始数据。 |
| Application/Editor | CLI、输入、未来 ImGui、Scene Outliner、Inspector、Asset Browser。 | 直接拥有 Vulkan Descriptor/Pipeline 或复制 Renderer 状态。 |

### 4.1.1 发布化与模块化支线

从 CQ-3 开始启用受控的 AR 支线，详细方案见
`Project/AzureRender/docs/RENDERER_MODULARIZATION_PLAN_CN.md`。该支线服务于角色调参、
场景复现和发布质量，不取代 CQ-3 至 CQ-6：

```text
AR-0 RenderSettings/Asset Contract
-> AR-1 Renderer Core Boundary
-> AR-2 Scene/Asset Model
-> AR-3 Editor Preview
-> AR-4 Feature Registry/Release Hardening
```

完整 ECS、脚本系统和动态插件 ABI 仍不在当前范围；先建立进程内可注册接口与稳定数据
所有权，接口冻结后再评估动态插件。

### 4.2 材质分类目标

长期材质模型至少区分：

- Face/Skin；
- Hair；
- Fabric；
- Leather/Rubber；
- Metal/Weapon；
- Eye/Iris；
- Transparent Overlay；
- Emissive；
- Environment/Platform。

每种分类拥有独立、可调、可序列化的 Ramp、Specular、Rim、Outline 和 Shadow 行为。Shader 可共享实现，但参数与数据不能继续使用同一套全局阈值。

### 4.3 数据驱动规则

- 材质参数保存为项目可拥有和可公开的 JSON/二进制配置。
- Ramp、Face SDF、Hair Ramp、Mask 和 LUT 使用明确的色彩空间和通道 Manifest。
- 所有资产专用猜测必须写入导出 Manifest；未知通道先进入 Debug View，不直接进入最终 Shader。
- Renderer 必须提供中性 Fallback，使公共资产和无特定贴图的材质仍能正确运行。
- Shader 中只允许通用材质类型逻辑，不允许按莱万汀材质名称硬编码最终颜色。

---

## 5. 总体里程碑与依赖顺序

| ID | 里程碑 | 当前状态 | 核心结果 | 前置依赖 |
|---|---|---|---|---|
| M0 | Master Plan 与质量治理 | Complete | 长期路线、质量门槛、决策和证据规则冻结。 | 无 |
| M1 | Character Rendering Quality Foundation | Active | 可独立调试的材质分类、Ramp、Face、Hair、Rim 基础。 | M0 |
| M2 | Character Hero Quality | Pending | 莱万汀多机位达到作品集级角色画面。 | M1 |
| M3 | Industrial Scene Integration | Pending | 模块化工业科幻场景、灯光、角色与环境统一。 | M2 |
| M4 | Portfolio Release | Pending | Windows Release、20s+ 视频、Case Study、公开仓库包。 | M3 |
| M5 | Benchmark Workload Freeze | Pending | Deferred NPR Workload、RenderPath 接口、Golden Images、Protocol v1.0。 | M4 |
| M6 | Multi-pass Research Baseline | Pending | Proposal 控制路径、自动计时和完整数据输出。 | M5 |
| M7 | Subpass Path | Pending | Input Attachment/Subpass 路径与 Multi-pass 视觉等价。 | M6 |
| M8 | DRLR Path 与能力决策 | Pending | 支持设备上 DRLR 路径；不支持设备明确 `NA`。 | M6、Capability Probe |
| M9 | Android Platform | Pending | Xiaomi 14 或替代设备上的共享 Workload 与支持路径。 | M5、设备能力决策 |
| M10 | Pilot 与正式实验 | Pending | 协议冻结、Pilot、正式 Runs、原始数据和统计。 | M7、M8、M9 |
| M11 | Thesis、演示与最终提交 | Pending | 论文、图表、最终 Release、演示、归档与提交。 | M10 |

依赖主链：

```text
M0 -> M1 -> M2 -> M3 -> M4 -> M5 -> M6
                                  |-> M7 -|
                                  |-> M8 -|-> M10 -> M11
                                  |-> M9 -|
```

---

## 6. M1–M2：角色美术质量主线

### 6.1 CQ-0 固定视觉 QA Harness

**状态：Complete（2026-08-02）。** 已形成 20 个固定 Camera/Light Baseline、12 个 Isolation、21 个 Enabled/Disabled/Isolation A/B Case，以及 Current/Reference/Isolation 对照表。Face SDF 与 Material ID 因对应系统尚未实现，分别在 CQ-3 与 CQ-1 接入，不伪造输出。

**目的：** 从“代码存在”改为“画面证据通过”。

必须实现：

- 固定 Full Body Front、Face Front、Face 3/4、Back Detail、Lighting Sweep 五类机位；
- 固定 Neutral Material、Stylized Key Light、Specular/Rim、Rear Emissive 四种灯光测试；
- 增加 Albedo、World Normal、Depth、Material ID、Diffuse Band、Shadow Visibility、Face SDF、Hair KK、Rim、Specular、Emissive、Outline 单项视图；
- 每项效果提供 Enabled/Disabled A/B Capture；
- Capture Manifest 写入 Camera、Light、Material Preset、Effect State 和 Hash；
- 建立 Current / Reference / Isolation 三列 Contact Sheet。

**退出条件：** 任一声称完成的风格效果都能在固定近景中看见，且关闭后产生局部、合理、可解释的视觉差异。

### 6.2 CQ-1 材质分类与参数系统

**状态：Complete（2026-08-02）。** Material Class/Data v1 已覆盖显式 glTF Profile、Feature Flags、两组材质参数、Schema、公共 Generic Fallback、Material ID、HUD 与 Manifest Inventory。

**任务：**

- 为每个 Primitive 建立明确 Material Class，不再只依赖通用 PBR Factor；
- 将 Toon/Ramp、Specular、Rim、Outline、Face/Hair 参数移入数据驱动配置；
- 建立 JSON Schema、默认值、版本号和公共中性 Fallback；
- 为 Debug/HUD 显示当前 Material Class 与关键参数；
- 从现有 Push Constant 逐步拆分，保持 Descriptor ABI 变更可审计。

**退出条件：** Face/Hair/Fabric/Metal 在相同灯光下使用不同、可解释的着色配置；修改一类材质不会无意改变其他材质。

### 6.3 CQ-2 真正的 Toon Ramp 与 Shadow 层级

**状态：Complete（2026-08-13）。** 已使用 CQ-1 的 Class/参数所有权接入版本化 Ramp Atlas，并把 Direct Diffuse、Ambient、Shadow Map Visibility、AO 与 Material Shadow Tint 拆分为可独立诊断的层级。

**任务：**

- 使用 1D Ramp LUT 或等价可编辑数据代替全局 `smoothstep`；
- 为 Face/Skin、Hair、Fabric、Metal 提供独立 Ramp；
- 分离 Direct Diffuse、Ambient、Shadow Map Visibility、AO 和 Material Shadow Tint；
- 让 Style/Material Mask 真正控制阴影与高光区域，而不是只添加弱 Accent；
- 支持两段或三段明暗、软硬边界和每材质 Shadow Color；
- 解决 Ambient 冲淡暗部和 ACES 后整体灰平的问题；
- Lighting Sweep 中检查明暗边界稳定性、视角独立性和 Shadow Map 接合。

**退出条件：** 参考图中的皮肤柔和分区、头发暗面、布料金属层次能在画面中直接辨认；关闭 Ramp 后差异明显但不破坏材质纹理。

**验收结果：** Skin/Face 使用线性软 Ramp，Hair/Fabric/Metal/Eye 使用阶梯 Ramp；Style Mask 控制 Ramp 坐标、Shadow/AO 和 Specular 权重。Toon Enabled/Disabled 在 1280×720 代表帧中产生 0.981119 Mean Absolute RGB Difference 和 18.503255% Changed Pixels，变化集中在角色区域；公共/私有 Debug Validation、Release、60 帧 Lighting Sweep、Alpha 与 Manifest Hash 检查通过。

### 6.4 CQ-3 Face SDF 与脸部 Overlay

**状态：Complete（2026-08-13）。** Face SDF、Face Overlay 和固定 QA 三态捕获均已完成。

**当前增量（2026-08-13）：** AR-0 已建立 `RenderSettings v1` 与 Face SDF v1
资产契约。兼容性审计确认莱万汀 Face Primitive 有 `TEXCOORD_0` 和
`face-sdf-eligible`，但当前 GLB 没有显式 SDF 纹理、通道、方向或 Head Node 绑定，
因此不复用普通脸部贴图，下一步制作 AzureRender 自有 Face SDF。

**实现顺序：**

1. 导出并验证现有公共女性 Face SDF 与莱万汀脸部 UV 的兼容性。
2. 如果兼容，记录来源、通道、方向和镜像规则；如果不兼容，制作 AzureRender 自有 Face SDF。
3. 使用 Head Bone/Head-local Basis 将世界光方向转换为脸部局部方向。
4. 实现左右翻转、阈值、Softness、Shadow Color、鼻侧/下颌控制和 Face Mask。
5. 重建 Hair Shadow 与 Eye Shadow Overlay 的 Depth、Opacity、Color 和偏移行为。
6. Face SDF 只控制脸部大尺度明暗；Matcap 只保留为低强度皮肤高光，不再冒充阴影模型。

**退出条件：** 光源绕头部旋转时脸部阴影方向连续、左右正确，不依赖脸部几何法线产生难看的鼻影；Hair Shadow 不穿透、漂浮或覆盖眼睛。

### 6.5 CQ-4 Hair Anisotropic / Kajiya–Kay

**状态：Complete（2026-08-13）。** 双层 Hair KK 与固定机位/Lighting Sweep 验收已完成。

**任务：**

- 导出并采样真实 `CB_LWT_KK_Ramp_01`；
- 验证 `_HN` RG/BA、Tangent/Bitangent、Shift 的真实语义；
- 接入 `KK_Shift_UV`、`KK_Spe_Upper_Limit`、Camera Offset、Ramp Strength；
- 接入 HairLine Mask 与发际线颜色；
- 实现至少主/次两层发束高光或等价可控模型；
- Hair Specular、Hair Rim 与 Base Hair Ramp 分离；
- 在 Front、3/4 和 Lighting Sweep 中检查高光连续、顺发流、不粘屏幕、不闪烁。

**退出条件：** 高光带在近景和半身镜头中清晰可见，形状沿发束延展；关闭后头发明显失去层次，但不会改变 Alpha、Depth 或轮廓。

### 6.6 CQ-5 Rim、Specular、Emissive 与 Bloom

**状态：Complete（2026-08-13）。** Rim/Specular/Emissive QA 与轻量 HDR Bloom v1 已完成。

**任务：**

- 将 Rim 从弱全局 Fresnel 改为 Material-aware Key/Fill/Rim 光照；
- Hair、Skin、Fabric、Metal 使用不同 Rim 强度、颜色、宽度和光向遮罩；
- 金属、布料、皮革使用不同 Specular Lobe 与环境反射强度；
- 校准 Emissive 的 HDR 亮度，避免只改变 Base Color；
- 增加受阈值和强度控制的轻量 Bloom，只作用于高亮 Emissive；
- 检查正面轮廓分离、黑色服装层次、背面发光和高光裁切。

**退出条件：** Rim 能稳定分离角色和背景；金属、布料、皮革在灰度图中也能通过高光形状区分；图 5 类型的背面 Emissive 具有明确视觉层级而不过曝。

### 6.7 CQ-6 Outline 与最终 Lighting/Grade

**状态：Review（2026-08-14）。** `RenderSettings v2` 已建立 Outline/Grade v1 参数所有权，最终合成从设置读取轮廓强度、深度/法线阈值、颜色、曝光、ACES 开关、饱和度、对比度与 Tint；Bloom strength/threshold 上传映射已修正。Debug/Release 构建、公共/私有 Debug Validation、动画 300 帧、Resize/最大化/恢复生命周期和 Release 代表捕获均通过；等待 M2 Gate 的用户视觉确认。

**任务：**

- 为 Face、Hair、Cloth、Metal 设置材质感知的 Outline 宽度、颜色和参与度；
- 降低内部线条噪声，避免法线贴图和细小硬表面产生碎线；
- 解决 Inverted Hull 与 Screen-space Outline 重叠加黑；
- 完成 Key/Fill/Rim 的稳定 Preset，锁定 Exposure、Tone Mapper 和 Color Grade；
- 在明背景、暗背景、工业场景和 Neutral Check 中验证轮廓；
- 形成最终 Hero Front、Face 3/4、Back Detail、Full Scene 四张冻结基准。

**M2 总退出条件：**

- 用户确认角色画面达到作品集标准；
- Face SDF、Hair KK、Ramp、Rim、Material Specular、Emissive、Outline 均有 Isolation 与 A/B 证据；
- 不存在半透明、隐藏结构穿透、蒙皮撕裂、阴影漂移或材质丢失；
- Debug/Release、Validation、Resize、动画、Capture 与长运行回归通过；
- 技术文档不再把 Prototype 描述成 Complete。

---

## 7. M3：工业科幻场景与展示整合

### 7.1 场景范围

场景只服务角色展示和作品集叙事，不构建开放世界。目标为模块化类《终末地》工业展示空间：

- 角色 Hero Stage；
- 一组地面、墙体、框架、灯带、终端和远景模块；
- 明确前景、中景、背景层次；
- 冷灰/青蓝主色与角色红粉 Emissive 形成对比；
- 支持 Full Scene、Hero Full Body、Material Close-up 三种构图。

### 7.2 Scene Work Packages

- SC-0：场景 Art Bible、参考板、色彩和材质清单；
- SC-1：模块化 Blockout、单位/比例、碰撞与 Camera Route；
- SC-2：场景材质、Decal/Mask、灯带和 Emissive；
- SC-3：Key/Fill/Rim、环境光、Shadow Composition 和雾化深度层次；
- SC-4：角色与场景 Color Grade、Bloom、Exposure 统一；
- SC-5：性能、Draw Call、纹理显存、LOD/Culling 和稳定性；
- SC-6：最终 Hero Shot 与 20 秒镜头路线。

### 7.3 M3 退出条件

- 场景明显提升角色构图，不遮挡或稀释角色材质；
- 三种固定镜头均具备前中后景和稳定视觉焦点；
- 场景材质与角色材质不混淆；
- 1080p Release 在目标 RTX 4060 上稳定运行并有 GPU Pass Timing；
- 公共版本可使用可再分发/自制的简化场景替代；
- Capture、动画、Resize、HUD 和 Debug View 在场景中正常。

---

## 8. M4：作品集发布

### 8.1 作品集交付物

- Windows Release Build；
- 至少四张最终角色/场景 Hero Images；
- 20 秒以上最终展示视频；
- 20 秒以上 Technical Breakdown Video；
- Current/Reference/Isolation 材质技术图；
- Renderer Architecture、Frame Flow、Material Model、Asset Pipeline 四类图；
- RenderDoc Capture 与 GPU Timing 证据；
- 公开 README、Build Guide、Controls、Known Limitations；
- Case Study：问题、目标、个人贡献、技术决策、Bug/Fix、结果和反思；
- 许可证、第三方代码和资产公开边界说明。

### 8.2 Portfolio Release Gate

- 陌生人能够在没有私有角色资产的情况下构建并运行公共测试场景；
- 私有角色只出现在合法的截图/视频或本地演示中，不进入公开仓库；
- 视频先展示最终画面，再展示材质分解、Debug Views、动画和性能证据；
- 项目描述强调个人实现的 Vulkan、Shader、资产工具和 QA，而不是把第三方模型当作贡献；
- 不宣称尚未完成的 Subpass/DRLR 性能优势；
- Portfolio Release 创建可恢复 Tag、Hash Manifest 和离线备份。

---

## 9. M5–M9：研究架构与三路径实现

### 9.1 M5 Benchmark Workload Freeze

**任务：**

- 从作品集材质中选择可控的 NPR 子集，不把全部美术复杂度带入 Benchmark；
- 建立 G-buffer：Normal、Albedo/Material、Depth，以及必要的轻量参数；
- Same-pixel Toon Lighting 作为 Local-read 核心段；
- Normal/Depth Contour 作为三路径共享的后续 Sampled-image Pass；
- 建立 `IRenderPath` 或等价最小接口；
- 固定低/高复杂度场景、两种分辨率、Camera Script、Shader Hash 和 Golden Images；
- Capability Probe 保存 Desktop/Android API、Extension、Feature 和 Properties；
- 冻结 Protocol v1.0。

**退出条件：** Workload、Formats、Camera、Shaders、Timing Boundaries 和 Golden Images 冻结；之后任何改变都需要 Protocol Version 更新。

### 9.2 M6 Multi-pass Research Baseline

- Geometry Pass 写 G-buffer；
- 显式 Barrier/Layout Transition；
- 独立 Lighting Pass 以 Sampled Images 读取；
- 公共 Contour/Composite；
- GPU Core-local、Full-frame 和 CPU Recording 独立计时；
- CSV/JSON、Metadata、Completion Marker 和 Checksum；
- Validation 与 Golden Image 通过。

### 9.3 M7 Vulkan Subpass Path

- 一个 Render Pass 内 Geometry Subpass 与 Lighting Subpass；
- Input Attachments、BY_REGION Dependencies、Preserve/Store 规则；
- 与 Multi-pass 相同输出和计时区段；
- RenderDoc 证明 Attachment、Subpass 和 Dependency 行为；
- Cross-path Image Diff 通过。

### 9.4 M8 DRLR Path

- Vulkan 1.4 Core 或 `VK_KHR_dynamic_rendering_local_read` Feature Probe；
- Dynamic Rendering Attachment Location/Input Index Mapping；
- Local-read Layout、Self-dependency 和 Shader Binding；
- Unsupported 设备不创建路径，并在 UI/Metadata 中标记 `NA`；
- 与 Multi-pass/Subpass Golden Image 等价；
- 保存 Validation 与 Capture 证据。

### 9.5 M9 Android Platform

- GameActivity/ANativeWindow 最小 Shell；
- 与 Desktop 共享 Renderer Core、Shaders、Workload 和数据格式；
- Android Asset Provider、Lifecycle、Pause/Resume、Surface Recreate；
- 首先运行 Multi-pass，再根据能力运行 Subpass/DRLR；
- AGI/Snapdragon Profiler 可用性记录；
- 温控、电源、刷新率和后台进程 SOP；
- 如果 Xiaomi 14 不支持 DRLR，按第 13 节决策树处理，不伪造支持。

---

## 10. M10：实验、数据与统计

### 10.1 正式实验矩阵

| 因素 | 水平 |
|---|---|
| Rendering Path | Multi-pass / Subpass / DRLR（Unsupported = NA） |
| Platform | RTX 4060 Laptop / Xiaomi 14 或经批准的替代设备 |
| Scene Complexity | Low / High |
| Resolution | 1280×720 / 1920×1080 |
| Repeats | 默认每 Cell 10 次；Pilot 后冻结 |
| Frames | 300 Warm-up + 1000 Sample |

### 10.2 指标

- GPU Core-local Time；
- GPU Full-frame Time；
- CPU Command Recording Time；
- Wall-frame Median、P95、P99、SD、CV、1% Low；
- Estimated Attachment Transactions；
- 平台内可解释的 Hardware Counters；
- Implementation Cost：LOC、文件、复杂度、缺陷和开发边界。

### 10.3 Pilot Gate

- 自动 Runner 可以中断恢复且不覆盖数据；
- Path 顺序平衡随机化；
- Warm-up 足以稳定 Pipeline/Shader Cache；
- Timestamp Period、Valid Bits 和 Query Readback 正确；
- Thermal Drift、Periodic Hitch 和 Outlier 规则已记录；
- CSV/JSON Schema 与统计脚本冻结；
- 所有 Supported Path 视觉等价、Validation clean。

### 10.4 正式数据规则

- Raw 数据 Append-only，禁止手改；
- 无效 Run 保留但标记 Invalid；
- Profiler 注入 Run 与普通 Timing Run 分离；
- 报告绝对差值、百分比效应和 95% Bootstrap CI；
- CI 跨 0 或差异小于噪声时写“未观察到可靠差异”；
- NVIDIA 与 Qualcomm 的不同 Counter 不直接放在同一数值轴；
- Small/No Difference 仍是有效研究结果。

---

## 11. M11：论文、演示与最终交付

### 11.1 Thesis Evidence Map

| 章节 | 必须已有的工程证据 |
|---|---|
| Introduction / Gap | Proposal、三路径问题、Capability Truth Table。 |
| Related Work | Vulkan Render Models、Subpass/DRLR、NPR、Profiling 限制。 |
| Methodology | Matrix、Controls、Protocol、Metrics、Statistics、Ethics/License。 |
| System Design | Architecture、G-buffer、Pass Diagram、Platform Separation。 |
| Implementation | Layout/Barrier、Subpass Dependency、DRLR Mapping、Android 差异。 |
| Results | Raw Data、Distribution、Effect Size、Estimated Transactions、Counters。 |
| Discussion | 平台差异、实现成本、无显著差异、Threats to Validity。 |
| Conclusion | 逐项回答 H1–H3，不超出设备和 Workload 范围。 |

### 11.2 最终提交包

- Source Release、Pinned Dependencies、Build Instructions；
- Windows Executable、Android APK（若允许）；
- Protocol、Capabilities、Sample Raw Data、Analysis Scripts、Figures；
- Thesis、Slides/Poster、3–5 分钟演示、20 秒以上作品集视频；
- License、Citation、Asset Boundary、Known Limitations；
- Hash Manifest、Release Tag 和离线归档。

---

## 12. 统一质量门槛

### 12.1 “效果完成”视觉门槛

一个效果只有同时满足以下条件才可标记 Complete：

- 在固定目标机位中肉眼可辨；
- 有 Isolation View；
- 有 Enabled/Disabled A/B；
- 变化只出现在合理区域；
- 在至少一个 Lighting Sweep 中连续稳定；
- 不引入遮挡、Alpha、Depth、蒙皮或 Temporal Artifact；
- 参数、数据来源和限制已记录；
- 用户确认它改善了最终画面。

代码中存在公式、Descriptor、Texture 或参数，不构成完成证据。

### 12.2 Vulkan 技术门槛

- Debug/Release 构建；
- 公共资产 Smoke 与私有资产视觉回归；
- Validation 无 Error；
- Resize/Minimize/Restore 与 Swapchain Recreate；
- Screenshot/Capture、Alpha、Manifest、Hash；
- 资源销毁、长运行和多次启动稳定；
- RenderDoc/Debug Label 能定位 Pass 与 Attachment。

### 12.3 作品集门槛

- 画面质量优先于功能数量；
- 至少一张 Face、Hair、Material、Back Emissive 近景；
- 不使用 Debug UI 遮挡最终图；
- 视频同时展示最终结果与技术证据；
- 所有贡献、参考和第三方资产边界准确；
- 公开版本可以在无私有资产条件下运行。

### 12.4 研究门槛

- Supported Path 视觉等价；
- Workload 与 Protocol 冻结；
- 计时不包含 Shader 编译、PNG 编码、UI 或 Validation；
- Raw Data、Metadata、Hash、统计脚本齐全；
- 结论不超出设备、驱动、Workload 和测量工具。

---

## 13. 决策登记

| ID | 决策 | 当前建议/决定 | 触发点 | 状态 |
|---|---|---|---|---|
| D01 | 第一完整渲染路径 | Windows Desktop Portfolio Renderer；研究路径延后。 | 已生效 | Frozen |
| D02 | 美术方向 | 类《终末地》角色 + 冷色工业科幻场景，但使用 AzureRender 自有参数和表现。 | 已生效 | Frozen |
| D03 | 视觉参考使用 | 图 2 Material Check、图 3 Ramp/Face、图 4 Specular/Rim、图 5 Back Emissive。 | M1 | Adopted |
| D04 | Face SDF | 先验证公共女性 SDF UV；不兼容则制作项目自有 SDF。 | CQ-3 | Recommended |
| D05 | 材质参数格式 | 版本化 JSON + 明确 Manifest 的 PNG/LUT，提供中性 Fallback。 | CQ-1 | Recommended |
| D06 | Hair KK Ramp | 导出真实 `CB_LWT_KK_Ramp_01`，不继续以解析近似作为最终方案。 | CQ-4 | Recommended |
| D07 | 场景范围 | 模块化 Hero Stage，不做开放世界。 | M3 | Recommended |
| D08 | 私有资产公开 | 代码和媒体可公开；原始角色模型/贴图默认不随仓库发布。 | 发布前 | Frozen |
| D09 | Benchmark Workload | Portfolio 通过后提取固定 Deferred NPR 子集。 | M5 | Frozen |
| D10 | DRLR 设备策略 | Probe 后决定；Unsupported = NA，不模拟支持。 | M8/M9 | Frozen |
| D11 | 仓库结构 | 当前单工程共享 Core；Portfolio Asset、Benchmark Data 与 Public Package 分离。 | M4/M5 | Adopted |
| D12 | Tone Mapping | S36.2 ACES fitted 作为当前基线；最终曝光/曲线在 M2 冻结。 | CQ-6 | Open |

### 13.1 DRLR / Mobile 决策树

| 情况 | 决定 |
|---|---|
| 两平台都支持 DRLR | 执行原始三路径 × 两平台矩阵。 |
| 仅 Desktop 支持 | Desktop 三路径为主实验；Mobile 只做 Multi-pass/Subpass 补充，并调整 H2 表述。 |
| 两平台都不支持 | 优先获得兼容设备；否则向 Supervisor 提交 Scope Change，转为可行性 + Subpass Benchmark。 |
| Android 基础移植失败 | 先修复最小 Platform Shell；不在移动端加入 GUI、复杂资产和新美术效果。 |

---

## 14. 风险登记与停止规则

| ID | 风险 | 影响 | 预防 | 应急 |
|---|---|---|---|---|
| R01 | 继续把 Prototype 当成美术完成 | 作品集质量失败 | 统一 Isolation/A/B/User Approval Gate | 回退到 CQ-0，取消错误完成状态。 |
| R02 | 功能扩张导致角色质量失焦 | 无法形成 Hero Result | M1–M4 前禁止研究路径和大型新系统 | Must/Should/Could 清理 Backlog。 |
| R03 | Face SDF/Hair 数据语义错误 | 画面错误且难调 | 导出 Manifest、Debug View、UV/通道验证 | 制作项目自有数据，不伪造原作还原。 |
| R04 | 资产许可不清 | 无法公开作品集或仓库 | 私有资产隔离、公开 Fallback、许可记录 | 替换媒体或资产，不发布原文件。 |
| R05 | Portfolio 与 Benchmark 相互污染 | 实验不公平 | M5 冻结独立 Workload/Protocol | 提升 Protocol Version，废弃旧结果。 |
| R06 | DRLR 不受设备支持 | 研究矩阵缺失 | 提前 Capability Probe | NA、替代设备或批准 Scope Change。 |
| R07 | Android 移植晚期失败 | 跨平台结果缺失 | 最小 Shell 和 Lifecycle Smoke 前置 | Desktop 主结果 + Mobile 限制。 |
| R08 | GPU Timing 噪声/热节流 | 结论无效 | Warm-up、随机顺序、温控、重复 | 重跑、缩减矩阵但保留统计效力。 |
| R09 | 三路径视觉不等价 | 无法比较 | 共享 Shader/Config、Golden Attachment Diff | 最小场景逐 Pass 定位。 |
| R10 | 论文写作堆积 | 最终无法整合 | 每个 M5–M10 节点同步 Evidence Map | M11 只整合，不从零回忆。 |
| R11 | 数据误删或不可复现 | 研究证据丢失 | Append-only、Checksum、双份归档 | 从 Runner Metadata 重跑 Cell。 |
| R12 | 重构破坏稳定基线 | 反复返工 | 小 Slice、先测试、保留 Golden/Tag | 回到最后验证 Tag，缩小变更。 |

停止规则：

- M2 未通过，不开始 M3 大规模场景制作；
- M4 未通过，不开始 M5 Research Refactor；
- Capability 未确认，不假设 DRLR 或 Mobile Path 可用；
- Cross-path Visual Equivalence 未通过，不采集正式性能数据；
- Pilot 数据不稳定，不开始正式 Runs；
- 原始数据和 Evidence 不完整，不撰写对应结论。

---

## 15. Must / Should / Could Backlog 边界

### Must

- 角色 Face SDF、Hair KK、Material-aware Ramp、Rim、Material Separation、Outline 与 Emissive/Bloom；
- 固定视觉 QA、Industrial Hero Scene、Windows Portfolio Release；
- Deferred Benchmark Workload、Multi-pass/Subpass/DRLR；
- Android 或经批准的受限跨平台方案；
- Runner、Pilot、正式实验、统计、Thesis 和最终提交。

### Should

- AR-0/AR-1 设置与 Renderer Core 边界；
- 可调试 HUD/ImGui、Material Preset 保存、轻量 Scene Outliner 与 Asset Browser；
- 轻量 Bloom、Color Grade、场景模块化和 LOD；
- RenderDoc Capture 索引、自动 Contact Sheet、作品集网页集成；
- 公共自制替代角色/场景资产。

### Could

- TAA、复杂后处理、额外 Tone Mapper；
- 多角色、复杂动画状态机；
- 粒子、天气、体积效果；
- 完整 ECS、通用 Editor、Hot Reload、脚本系统、动态插件 ABI；
- Ray Tracing、SSR、GI 实验。

Could 项只有在所有 Must 项完成且不会影响实验冻结时才可开始。

---

## 16. 证据与文件管理

### 16.1 每个 Vertical Slice 必须留下

- Goal、Scope、Acceptance、Risks；
- Build/Run 命令；
- Validation 结果；
- 当前/对照/Isolation Screenshot 或 Capture；
- Shader/Asset/Config Hash；
- 开发日志记录；
- 如果是里程碑退出：Release Tag 或可恢复备份。

### 16.2 数据结构

```text
data/
  raw/<run_id>/frames.csv
  raw/<run_id>/metadata.json
  raw/<run_id>/capabilities.json
  captures/<run_id>/
  derived/summary.csv
  figures/
  protocol/protocol-v1.0.md
```

### 16.3 文档更新职责

- Master Plan：只更新里程碑状态、决策、范围和退出门槛；
- Development Log：记录实际修改、验证数字和 Artifact 路径；
- Handoff：记录当前可运行入口、基准和恢复步骤；
- 专项设计：解释当前节点的算法、资源和实现细节；
- Thesis Evidence Map：M5 起每个节点同步填充。

---

## 17. 当前执行锚点

### 当前唯一 Active 里程碑

**M1 — Character Rendering Quality Foundation**

### 当前 Work Package

**CQ-3 — Face SDF 与脸部 Overlay**

### CQ-0 之后的固定顺序

```text
CQ-0 Visual QA
-> CQ-1 Material Classes/Data
-> CQ-2 Toon Ramp
-> CQ-3 Face SDF/Overlays
-> CQ-4 Hair KK
-> CQ-5 Rim/Specular/Emissive/Bloom
-> CQ-6 Outline/Lighting/Grade
-> M2 Character Hero Quality Gate
```

在 M2 通过以前，S36.3 Exposure Interaction、正式 GPU Performance、Subpass、DRLR、Android 和大型场景均为 Pending/Deferred，不得成为主要开发节点。

### 下一次开发的明确目标

兼容性审计已确认当前莱万汀资产没有显式 Face SDF 输入。下一步制作 AzureRender 自有 Face SDF，验证 `Bip001_Head`、`Face_Head` 等候选并冻结唯一 Head Basis；随后接入 Head-local 光照方向、左右翻转/阈值/Softness/Shadow Color，并重建 Hair Shadow 与 Eye Shadow Overlay 的 Depth、Opacity、Color 和偏移行为。该节点不实现最终 Hair KK、Bloom 或最终调色。

---

## 18. 计划维护检查表

每次结束前检查：

- [ ] 本次工作属于 Master Plan 的明确节点；
- [ ] 没有开始依赖尚未通过的后续节点；
- [ ] “完成”由画面/测试/数据证明，不由代码量证明；
- [ ] Build、Validation、视觉或性能证据齐全；
- [ ] Development Log 与 Handoff 已按需更新；
- [ ] Master Plan 只更新事实状态，没有临时扩张范围；
- [ ] 新想法进入 Backlog，而不是打断 Active 节点；
- [ ] 下一次开发从固定依赖顺序领取任务。

本计划从 Version 2.0 起作为 AzureRender FYP 的长期方向控制文件。旧版 `FYP_Development_Plan_v1.3.docx` 保留为历史规划依据，不再作为当前执行入口。
