# AzureRender 近期开发执行计划

> 计划版本：2026-08-17 v7.1
> 当前节点：v6 队列进行中（BH-1/2/3、AR-10.0/10.1/10.2、BH-2.1 Complete；BH-2.2 收尾——TAA 资源已建未激活(渲染私有纹理时 trace 输出异常，回退至直渲 scene framebuffer)，lensing 修正与 bloom 待启用）
> 适用范围：可插拔场景渲染器架构 + 施瓦西黑洞渲染 Demo

## 1. 计划治理

本文是近期任务顺序的唯一事实来源。`PROJECT_HANDOFF_CN.md` 负责项目事实交接，
`RENDERER_MODULARIZATION_PLAN_CN.md` 负责阶段目标，`DEVELOPMENT_LOG_CN.md` 只记录
已经完成的工作。三者中的历史“下一步”不得覆盖本文。

执行规则：

1. 严格按“固定执行队列”从上到下工作，同时只能有一个 `Active` 任务；
2. 开始实现前先读取任务的范围、依赖、交付物和退出条件，不在运行过程中临时发明下一任务；
3. 每个任务完成后独立 Commit，使用表中冻结的简短中文标题；
4. 只有构建、测试、Validation 和必要的视觉检查全部通过后才能标记 `Complete`；
5. 新需求先进入“候选池”，不得插入当前任务；改变顺序时先单独更新本文并提交；
6. 紧急回归修复可以中断队列，但必须记录原因、复现、验证和 Commit；
7. v3 队列是用户 2026-08-16 明确授权的执行范围：HDR IBL/mipmap、Morph Target/OIT、
   对象拾取/Gizmo/Scene Graph、git 仓库健康修复与 `.workbuddy` 版本控制决策。
   M3/SC 工业场景、论文三路径、动态插件仍 Deferred。
8. v4 队列是用户 2026-08-16 二次授权的打磨范围：HDR IBL 精化（真实 equirect 资产
   导入 + specular IBL 卷积）、Gizmo 3D 视口内手柄、完整 ECS 基础。
9. v5 队列（AR-9.0～AR-9.3）已完成：编辑器闭环验证、背景采样环境贴图、论文三路径
   基准、完整 ECS 系统。
10. v6 队列是用户 2026-08-16 授权的一次性自主执行范围：将 AzureRender 演进为可插拔
    场景渲染器（`ISceneRenderer` + 注册中心 + `sceneType` 选择），并把现有角色路径
    迁移为第一个场景渲染器实现（哈希回归锚定），随后以施瓦西黑洞渲染器作为第二个
    实现验证架构，最后收口多场景切换、文档与全量回归。

状态只使用：`Backlog`、`Ready`、`Active`、`Complete`、`Deferred`、`Blocked`。

## 2. 当前基线

| 节点 | 状态 | 已交付能力 |
|---|---|---|
| M2 | Complete | Validation 发布门禁、Hero 基准冻结 |
| AR-1 v1 / AR-1.1 | Complete | Renderer 契约边界、GLFW 平台前端拆分 |
| AR-2 v1 | Complete | `.azscene v1`、资源与节点模型 |
| AR-3 v1 | Complete | Renderer-native Preview 与保存闭环 |
| AR-3.1 | Complete | `EditorContext`、`IEditorPanel` |
| AR-3.2 | Complete | Dear ImGui GLFW/Vulkan Backend 与 Docking 布局 |
| AR-3.3 | Complete | 独立离屏 Viewport 与 UI RenderPass |
| AR-3.4 | Complete | 轨道相机、平移、缩放和输入焦点隔离 |
| AR-3.5 | Complete | Viewport 尺寸驱动颜色/深度/法线/后处理目标 |
| AR-3.6 | Complete | Viewport 资源独立重建，不重建 swapchain/ImGui Context |
| AR-3.7 | Complete | `EditorSession`/Command、保存、脏状态、布局重置和错误反馈 |
| AR-4.0 | Complete | RC0 行为基线、支持平台、兼容策略和统一测试门禁 |
| AR-4.1 | Complete | 结构化日志、GPU 能力报告、错误分类与退出码 |
| AR-4.2 | Complete | `ResourceLocator`、开发/安装树和显式资源根覆盖 |
| AR-4.3 | Complete | Feature、Importer、Panel 进程内扩展注册中心 |
| AR-4.4 | Complete | Windows/Linux CI、CTest、公共 smoke 和文档检查 |
| AR-4.5 | Complete | 可移动 RC 包、公共 Demo、许可证和干净目录验收 |
| M3 / SC | Deferred | 按当前决策暂不执行场景与工业场景工作包 |

这里的 AR-1 `Complete` 指 v1 契约边界完成，不代表 Vulkan 资源所有权已经完全移出
`AzureRenderApp`；剩余所有权拆分纳入下列 AR-3.6 与 AR-4 工作包。

## 3. v1 已完成队列

| 顺序 | 任务 | 状态 | 目标 | 依赖 | 预定 Commit 标题 |
|---:|---|---|---|---|---|
| 1 | AR-3.6 | Complete | Viewport RenderTarget 独立重建，不再重建交换链或 ImGui Context | AR-3.5 | `完成 AR-3.6 视口资源独立重建` |
| 2 | AR-3.7 | Complete | 编辑器 Session 与命令层：保存、脏状态、布局重置、错误反馈 | AR-3.6 | `完成 AR-3.7 编辑器会话闭环` |
| 3 | AR-4.0 | Complete | 冻结 RC0 行为基线、测试矩阵和支持平台 | AR-3.7 | `完成 AR-4.0 RC0 基线冻结` |
| 4 | AR-4.1 | Complete | 结构化日志、GPU 能力报告、错误分类与退出码 | AR-4.0 | `完成 AR-4.1 运行诊断基础` |
| 5 | AR-4.2 | Complete | `ResourceLocator` 与安装目录，移除运行时源码绝对路径依赖 | AR-4.1 | `完成 AR-4.2 运行资源定位` |
| 6 | AR-4.3 | Complete | `IRenderFeature`、`IAssetImporter`、`IEditorPanel` 进程内 Registry | AR-4.2 | `完成 AR-4.3 扩展注册中心` |
| 7 | AR-4.4 | Complete | Windows/Linux CI：构建、CTest、公共资产 smoke 与文档检查 | AR-4.3 | `完成 AR-4.4 跨平台持续集成` |
| 8 | AR-4.5 | Complete | 可安装 RC 包、公共 Demo、许可证与干净环境验收 | AR-4.4 | `完成 AR-4.5 RC 发布包` |

v1 固定队列已全部完成。

## 3.1 v2 固定执行队列

| 顺序 | 任务 | 状态 | 目标 | 依赖 | 预定 Commit 标题 |
|---:|---|---|---|---|---|
| 1 | AR-5.0 | Complete | 可重复的本地/CI 发布门禁编排与结果摘要 | AR-4.5 | `完成 AR-5.0 发布门禁编排` |
| 2 | AR-5.1 | Complete | 类型化 CLI 解析、稳定错误类型与参数契约测试 | AR-5.0 | `完成 AR-5.1 命令行契约` |
| 3 | AR-5.2 | Complete | `.azscene` 原子保存、临时文件清理和恢复测试 | AR-5.1 | `完成 AR-5.2 场景原子保存` |
| 4 | AR-5.3 | Complete | Renderer/Loader/Validation 日志汇入统一诊断源 | AR-5.2 | `完成 AR-5.3 统一运行日志` |
| 5 | AR-5.4 | Complete | GPU 能力报告 JSON 安全、Schema 与格式能力测试 | AR-5.3 | `完成 AR-5.4 GPU 报告契约` |
| 6 | AR-5.5 | Complete | 第三方许可证正文、包内容清单与可复现归档校验 | AR-5.4 | `完成 AR-5.5 发布合规清单` |
| 7 | AR-5.6 | Complete | RC1 版本冻结、双平台证据汇总与发布审计 | AR-5.5 | `完成 AR-5.6 RC1 发布审计` |

### v2 统一退出规则

- 每个节点必须新增或强化自动化测试，不能只更新文档；
- Debug、Release、ImGui Release 构建与全量 CTest 必须通过；
- 无窗口环境执行 CLI、安装树和包结构门禁；图形 smoke 由 Xvfb/lavapipe CI 执行；
- 每节点独立提交并更新本文状态；未完成当前节点不得开始下一节点；
- M3/SC、对象拾取、Gizmo、资源导入、动态插件、Android 和论文三路径继续 Deferred。

### AR-5.0 发布门禁编排

- 提供单一命令执行配置/构建、CTest、安装、移动目录资源检查、CPack 和 manifest 校验；
- 失败必须指出具体阶段并返回非零；生成机器可读结果摘要；
- CI 复用同一入口，避免本地和流水线维护两套门禁。

验收：在当前 Linux 环境完整执行；验证故意缺失构建目录时安全失败；结果摘要记录各阶段。

### AR-5.1 类型化命令行契约

- CLI 解析从 `main.cpp` 抽为不依赖窗口、Vulkan 或资产的纯 C++ 模块；
- 缺值、未知参数、非法值和非法组合使用稳定的类型化错误，并统一返回退出码 2；
- 数字采用完整无符号整数解析，拒绝负数、尾随字符、零值和类型溢出；
- 冻结 Capture、Technical Sequence、QA 与场景入口的依赖和互斥关系。

验收：独立表驱动契约测试；真实进程负向退出码测试；Debug、Release、ImGui
Release 全量 CTest；契约同步至 `docs/CLI_CONTRACT_CN.md`。

### AR-5.2 场景原子保存

- `SceneDocument::save` 改为原子写入：同目录唯一临时文件 → flush 校验 → `rename` 替换目标；
- 写入、flush 或 rename 失败时清理临时文件并保持原目标不变；
- 保存目标目录不存在时在写临时文件前失败，无副作用；
- 新增 `tests/SceneModelTests.cpp`，覆盖正常保存、覆盖保存、失败恢复和无残留验证。

验收：Debug/Release 构建；`AzureRender.SceneModel` 等全量 CTest 通过；公共资产
120 帧 Debug Validation 无 VUID；每节点独立提交并更新本文、README 与开发日志。

### AR-5.3 统一运行日志

- Renderer、Loader、Validation 日志汇入 AR-4.1 建立的 RuntimeDiagnostics 统一源；
- 消除直接 `std::cout` 输出，ImGui Console 与文件日志消费同一事件流；
- 日志路径、级别与错误码沿用既有 Schema，不新增格式。

## 3.2 v3 固定执行队列

> 用户授权范围：渲染特性升级、编辑器增强与仓库维护（2026-08-16）。

| 顺序 | 任务 | 状态 | 目标 | 依赖 | 预定 Commit 标题 |
|---:|---|---|---|---|---|
| 1 | MAINT-1 | Complete | git 对象库健康修复：fetch 补全缺失对象、gc 清理、`git fsck` 清零；`.workbuddy/` 纳入版本控制决策 | 无 | `完成 MAINT-1 仓库健康修复` |
| 2 | AR-6.1 | Complete | HDR IBL：equirectangular HDR 环境 + mipmap 生成 + prefiltered specular 反射 | MAINT-1 | `完成 AR-6.1 HDR IBL 环境` |
| 3 | AR-6.2 | Complete | Morph Target：glTF morph targets 加载与 GPU 混合 | AR-6.1 | `完成 AR-6.2 形态目标` |
| 4 | AR-6.3 | Complete | OIT：per-triangle 透明度排序（替代 primitive 级排序） | AR-6.2 | `完成 AR-6.3 逐三角形透明排序` |
| 5 | AR-7.1 | Complete | 对象拾取：Viewport 射线拾取 + 选中高亮 | AR-6.3 | `完成 AR-7.1 视口对象拾取` |
| 6 | AR-7.2 | Complete | Transform Gizmo：平移/旋转/缩放手柄与编辑 | AR-7.1 | `完成 AR-7.2 变换 Gizmo` |
| 7 | AR-7.3 | Complete | 完整 Scene Graph：层级浏览、节点属性编辑、保存/加载 | AR-7.2 | `完成 AR-7.3 场景图编辑` |

### v4 固定执行队列

| 顺序 | 任务 | 状态 | 目标 | 依赖 | 预定 Commit 标题 |
|---:|---|---|---|---|---|
| 1 | AR-8.1 | Complete | HDR IBL 精化：外部 equirect 资产导入（.hdr/.png）+ specular IBL 卷积（重要性采样 prefilter） | AR-7.3 | `完成 AR-8.1 HDR IBL 精化` |
| 2 | AR-8.2 | Complete | Gizmo 3D 视口内手柄：屏幕空间轴投影、拖拽平移/旋转/缩放 | AR-8.1 | `完成 AR-8.2 视口 Gizmo 手柄` |
| 3 | AR-8.3 | Complete | 完整 ECS：Entity/Component 存储、System 更新循环、场景桥接 | AR-8.2 | `完成 AR-8.3 ECS 基础` |

### v3 统一退出规则

- 每个节点必须新增或强化自动化测试，不能只更新文档；
- Debug、Release 构建与全量 CTest 必须通过；
- 渲染/画面变化任务执行 1280×720 截图检查与 120 帧 Debug Validation；
- 每节点独立提交并更新本文状态；未完成当前节点不得开始下一节点；
- M3/SC、论文三路径、动态插件、Android 继续 Deferred。

## 4. 任务退出条件

### MAINT-1 仓库健康修复

- 诊断并修复 `.git/refs/` 稳定性问题（此前发生过 refs 目录丢失）；
- 网络可用时 `git fetch` 补全缺失对象，`git gc` 整理仓库，`git fsck --full`
  无 missing/invalid 输出；
- 决定 `.workbuddy/` 是否纳入版本控制：若纳入则补充 `.gitignore` 规则与文档说明；
- 记录修复前后 fsck 差异与仓库状态。

验收：`git fsck --full` 干净；`git log`/`git status` 正常；提交链路无异常；
`.workbuddy/` 决策写入文档。

### AR-6.1 HDR IBL 环境

- 环境贴图从 LDR 程序生成升级为 HDR equirectangular 源（`.hdr`/`.exr` 加载或程序生成）；
- 生成 irradiance 漫反射卷积与 prefiltered specular 的 mip 链；
- PBR 反射路径使用 prefiltered 采样替代现有粗糙度模糊近似；
- 保留现有诊断视图与确定性 Capture 不变。

验收：Debug/Release 构建；CTest；1280×720 截图对比确认反射质量提升；
120 帧 Validation 无 VUID；环境资源由 ResourceLocator 定位。

### AR-6.2 Morph Target

- glTF morph targets（POSITION/NORMAL 权重混合）加载；
- GPU 端按权重混合基础网格与 morph 目标；
- 权重通过 RenderSettings/动画或编辑器参数驱动，支持 0~1 连续调节；
- 公共测试资产提供至少一个 morph 用例。

验收：加载/混合测试；动画或参数驱动的权重变化截图；Validation 无 VUID。

### AR-6.3 逐三角形透明排序

- BLEND 材质从 primitive 级排序升级为 per-triangle 视空间深度排序；
- 深度写入策略与混合顺序在排序后保持正确；
- 与现有 HDR Scene Color、内部描边和后处理合成路径兼容；
- 保持确定性 Capture 输出可复现。

验收：透明物体排列正确性截图；排序单元测试（构造三角形深度序列验证）；
Validation 无 VUID。

### AR-7.1 视口对象拾取

- Viewport 点击生成射线，与场景节点/primitive 求交；
- 拾取结果显示在 Inspector，选中节点高亮；
- 与编辑器 Session 脏状态、相机交互和离屏 Viewport 路径共存；
- 新增拾取单元测试（射线-包围盒/三角形求交）。

验收：求交测试；交互拾取 Validation；1280×720 截图确认高亮。

### AR-7.2 变换 Gizmo

- 平移/旋转/缩放三模式 Gizmo 手柄渲染与拖拽编辑；
- Gizmo 操作更新节点 TRS 并标记脏状态；
- 支持局部/世界坐标系切换（可选）与撤销基本操作；
- 新增 Gizmo 数学测试（手柄命中、拖拽增量）。

验收：Gizmo 命中/拖拽测试；编辑后保存/重载一致；Validation 无 VUID。

### AR-7.3 场景图编辑

- Outliner 支持层级树浏览、多选与节点重命名；
- Inspector 支持节点变换、可见性、父子关系编辑；
- Scene Graph 变更可保存到 `.azscene v1`（或提升 schema 版本）并重载一致；
- 完整 Scene Graph 编辑闭环不依赖未授权功能。

验收：层级编辑测试；保存/重载一致；1280×720 截图；Validation 无 VUID。

### AR-8.1 HDR IBL 精化

- 支持从文件导入真实 equirectangular 环境（.hdr 使用 stb_image float 解码，
  .png/.jpg 常规解码），替代或叠加程序化环境；
- 环境贴图按 RGBA16F/RGBA32F 上传并生成 mip 链；
- 用 GPU 卷积（compute 或逐 mip blit + 权重采样）生成 prefiltered specular
  环境，替代当前"blit 即近似"；
- CLI 参数 `--environment <path>` 与文档示例资产。

验收：环境导入单元测试（.hdr 解码往返）；截图对比程序化/外部环境；
Validation 无 VUID；Debug/Release 构建与 CTest 全绿。

### AR-8.2 视口 Gizmo 手柄

- 选中对象后在 Viewport 内投影渲染 3D 轴手柄（平移/旋转/缩放三模式可切换）；
- 鼠标悬停高亮、点击拖拽沿轴/面更新节点 TRS；
- 手柄命中与拖拽数学有单元测试（投影、逆变换、增量）；
- 保留 Inspector 数值编辑作为补充入口。

验收：手柄命中/拖拽测试；编辑后保存/重载一致；1280×720 截图；Validation 无 VUID。

### AR-8.3 ECS 基础

- 引入 Entity/Component 存储（稀疏集或 SoA 组件池）与 System 注册/更新循环；
- 场景节点桥接 ECS：Outliner/Inspector 与渲染路径至少一条通过 ECS 查询工作；
- ECS 变更不破坏 `.azscene` 保存/重载与既有测试；
- 新增 ECS 单元测试（创建/销毁实体、组件读写、系统执行顺序）。

验收：ECS 测试；Debug/Release 构建与 CTest 全绿；编辑器编辑闭环正常。

### AR-3.6 Viewport 资源独立重建

- 抽出只管理 Scene Color、Depth、Normal、最终 Viewport Color 与相关 framebuffer 的生命周期；
- Dock 分隔线调整只等待相关 frame fence，不销毁 swapchain、ImGui Context 或平台窗口；
- 连续拖动使用防抖，不产生 descriptor 泄漏、陈旧 image view 或 layout VUID；
- 普通 Renderer 路径和确定性 Capture 不改变。

验收：Debug/Release 构建；CTest；编辑器尺寸变化前后各 120 帧 Validation；普通 Renderer
120 帧 Validation；截图确认 Viewport 无拉伸、无黑边。

### AR-3.7 编辑器会话闭环

- 建立显式 Editor Session/Command 边界，面板不直接执行文件保存；
- 提供 Save、Save on Close、脏状态显示、恢复默认布局和失败信息；
- 保存失败不得清除脏状态，退出时不得静默丢失修改；
- 不扩展 Scene Graph、对象拾取、Transform Gizmo 或资源导入功能。

验收：会话单元测试；只读/非法路径失败测试；保存后重载一致；120 帧 Validation。

### AR-4.0 RC0 基线冻结

- 冻结 CLI、`.azscene`、`RenderSettings`、Capture Manifest 和编辑器最小功能面；
- 建立支持平台、非目标、兼容策略和发布阻断项清单；
- 将公共资产 smoke、相机测试、场景序列化和 Validation 定义为统一门禁。

验收：所有门禁可由文档中的命令重复执行，结果记录不依赖私有资产。

### AR-4.1 运行诊断基础

- 日志具备时间、级别、子系统和稳定错误码，可同时输出 Console 与文件；
- GPU/驱动/API/扩展/格式能力生成机器可读报告；
- CLI 参数错误、资产错误、Vulkan 初始化错误和运行错误使用不同退出码；
- ImGui Console 消费同一日志源，不复制 `std::cout` 状态。

验收：错误路径测试；能力报告 Schema 测试；日志文件不可写时安全失败。

### AR-4.2 运行资源定位

- Shader、公共资产、Ramp 和配置通过 `ResourceLocator` 查找；
- 支持开发树、安装树和显式 CLI/环境覆盖三种模式；
- CMake install 后不依赖 `AZURERENDER_*_DIR` 源码绝对路径；
- 缺失资源错误必须包含资源类型、搜索路径和修复提示。

验收：临时安装目录运行；移动安装目录后运行；缺失资源负向测试。

### AR-4.3 扩展注册中心

- 三类进程内 Registry 使用稳定 ID、版本和能力查询；
- 新增 Editor Panel 不修改主循环；Importer/Render Feature 的生命周期与 Renderer 所有权明确；
- 重复 ID、版本不兼容和依赖缺失在注册阶段失败；
- 本阶段不提供跨 DLL C++ ABI 或动态插件加载。

验收：示例 Panel 注册；重复/不兼容注册测试；现有渲染画面和 Validation 不变。

### AR-4.4 跨平台持续集成

- Windows 与 Linux 使用 vcpkg manifest 干净配置；
- Debug/Release 构建，CTest 和公共资产 smoke 自动化；
- Linux 软件 Vulkan Validation 作为门禁，Windows GPU smoke 作为平台门禁；
- CI 不访问私有资产，不提交构建产物。

验收：两个平台流水线全绿；失败日志可定位到具体门禁。

### AR-4.5 RC 发布包

- CMake install/CPack 生成可搬移包，包含可执行文件、Shader、公共 Demo 和必要许可证；
- 提供最短启动命令、系统需求、已知限制和版本信息；
- 在无源码目录的干净 Windows/Linux 环境完成启动、编辑器和 Capture smoke；
- 发布包生成 SHA-256 与机器可读 manifest。

验收：干净环境安装验收通过，形成 RC 发布报告；之后才允许规划 M3/SC 或动态插件。

## 4.1 v6 固定执行队列（可插拔场景渲染器 + 黑洞 Demo）

| 顺序 | 任务 | 状态 | 目标 | 依赖 | 预定 Commit 标题 |
|---:|---|---|---|---|---|
| 1 | AR-10.0 | Complete | 冻结 `ISceneRenderer`/`RenderContext`/`SceneFrameData` 契约，`SceneRendererRegistry` 挂入 `ExtensionRegistry`，`RenderSettings.sceneType` + `.azscene sceneRenderer` + `--scene-type` 契约 | AR-9.3 | `feat(ar10): 冻结可插拔场景渲染器接口` |
| 2 | AR-10.1 | Complete | 角色路径迁移为 `CharacterSceneRenderer`，引擎核心瘦身为调度者，S36 Beauty 哈希不变 | AR-10.0 | `feat(ar10): 迁移角色路径为场景渲染器` |
| 3 | BH-1 | Complete | `BlackholeSceneRenderer`：全屏追踪 pass + 星空背景 + 纯黑洞，`--scene-type blackhole` 可用 | AR-10.1 | `feat(bh): 黑洞渲染器基础追踪` |
| 4 | BH-2 | Complete | 吸积盘、多普勒/引力红移/射束、光子环累积、HDR 峰值，冻结新 Beauty 基准 | BH-1 | `feat(bh): 吸积盘与相对论效应` |
| 5 | BH-3 | Complete | 黑洞诊断视图、确定性 capture、GPU timing、manifest 扩展、技术序列 | BH-2 | `feat(bh): 黑洞交付链` |
| 6 | AR-10.2 | Complete | 编辑器场景类型切换、SceneRenderer Cookbook、tone mapper 可替换接口、全量回归 | BH-3 | `feat(ar10): 多场景收口与文档` |
| 7 | BH-2.1 | Complete | 吸积盘视觉重构：移植知乎实现——欧拉测地线+球对称连续步长、首步随机抖动+2x2 supersampling、体积吸积盘(Perlin 分形云/Shape 密度/温度 T⁴/多普勒/红移/黑体色/alpha 累积)、maxSteps=1800 | BH-3 | `feat(bh): 吸积盘视觉重构` |
| 8 | BH-2.2 | Backlog | TAA 激活(私有 ping-pong 纹理+blackhole_taa.frag 已建，修复 trace 写私有纹理异常后启用)+单 pass bloom+引力透镜 lensing 修正+星空蓝移 | BH-2.1 | `feat(bh): TAA 泛光与 lensing` |
v6 退出规则：每节点独立提交（本表中文标题）；Debug/Release 构建与全量 CTest 通过；
角色场景在 AR-10.1 之后必须以 `CharacterSceneRenderer` 渲染且与 S30/S36 基线逐字节
一致；黑洞场景只新增内容，不触碰角色路径。

## 5. 每任务固定门禁

除非任务明确声明不涉及运行时代码，每个任务完成前执行：

1. `git diff --check`，确认工作树只含当前任务；
2. Linux Debug、Release 和 Dear ImGui 构建；
3. `ctest --output-on-failure`；
4. 公共资产普通 Renderer 120 帧 Debug Validation；
5. 公共 `.azscene` 编辑器 120 帧 Debug Validation；
6. UI/画面变化任务执行 1280×720 截图检查；
7. 确认无 VUID、Validation warning/error；
8. 使用计划表中的中文标题提交，并同步任务状态与开发日志。

私有资产只作为可选补充证据，不能成为完成任务的必要条件。

## 6. 候选池与明确暂缓

以下内容不在固定执行队列中：

- M3 与全部 SC 工业场景任务（用户 2026-08-16 授权中未包含）；
- 文件导入、缩略图生成、资源依赖图和资源热重载；
- 动态库插件、跨 DLL C++ ABI、脚本系统；
- Android、Traditional Multi-pass/Subpass/DRLR 三路径和论文实验；
- 完整 ECS 架构、撤销/重做系统（AR-7 只做最小编辑闭环）。

候选池中的项目只有进入新的版本化执行计划（经用户授权）后才能执行。

