# AzureRender 近期开发执行计划

> 计划版本：2026-08-14 v2
> 当前节点：AR-5.2 Ready
> 适用范围：RC1 发布验证、错误契约、持久化与诊断收口

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
7. 未经用户重新授权，不执行 M3、SC、对象拾取或工业场景内容。

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
| 3 | AR-5.2 | Ready | `.azscene` 原子保存、临时文件清理和恢复测试 | AR-5.1 | `完成 AR-5.2 场景原子保存` |
| 4 | AR-5.3 | Backlog | Renderer/Loader/Validation 日志汇入统一诊断源 | AR-5.2 | `完成 AR-5.3 统一运行日志` |
| 5 | AR-5.4 | Backlog | GPU 能力报告 JSON 安全、Schema 与格式能力测试 | AR-5.3 | `完成 AR-5.4 GPU 报告契约` |
| 6 | AR-5.5 | Backlog | 第三方许可证正文、包内容清单与可复现归档校验 | AR-5.4 | `完成 AR-5.5 发布合规清单` |
| 7 | AR-5.6 | Backlog | RC1 版本冻结、双平台证据汇总与发布审计 | AR-5.5 | `完成 AR-5.6 RC1 发布审计` |

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

## 4. 任务退出条件

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

- M3 与全部 SC 工业场景任务；
- 对象拾取、Transform Gizmo、完整 Scene Graph/ECS；
- 文件导入、缩略图生成、资源依赖图和资源热重载；
- 动态库插件、跨 DLL C++ ABI、脚本系统；
- Android、Traditional Multi-pass/Subpass/DRLR 三路径和论文实验；
- HDR IBL、OIT、Morph Target 等新渲染特性。

候选项只有在 AR-4.5 完成或用户明确调整优先级后，才能进入新的版本化执行计划。
