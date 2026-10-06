# AI 原生工具与编辑器实施步骤

> 文档类型：实施子计划
> 状态：已采纳，执行状态见总计划
> 更新日期：2026-10-06
> 执行方式：按 F5、U4、G9、R7 顺序实施与验收

## 目标与输入契约

本计划建立可观察、可操作和可验证的开发闭环。消费 F4 的类型与模块、G8 的系统组合和 U3 的编辑操作。公共门禁见[主实施计划](2026-10-06-engine-evolution.md)。路径相对于 `Project/AzureRender`。

模型、验证通道和游戏 AI 具有独立职责。模型接入模块只处理请求与响应。领域适配器拥有产物规则。UI、工具与自动入口消费同一生产契约。

## F5 观察、内容与工具闭环

借鉴 A4、A5、B5。前置为 U3。交付状态发现、条件等待、内容来源和统一命令入口。

### F5.1 观察注册与自动验证

**文件：** 新增 `src/runtime/ObservationRegistry.hpp`、`ObservationRegistry.cpp`。新增 `src/validation/ValidationService.hpp`、`ValidationService.cpp` 和 `ValidationTransport.cpp`。修改 `src/editor/EditorAutomation.cpp`、`src/runtime/GameInputReplay.hpp` 及命令行入口。新增 `tests/ObservationTests.cpp` 和 `tools/test_validation_protocol.py`。

**消费：** U3 操作描述、文档版本与生产执行器。G8 提供运行状态。输入回放消费现有窗口与 ImGui 事件路径。

**输出：** `ObservationValue` 为布尔、整数、有限浮点或字符串。`ObservationRegistry::add(name, reader)` 注册只读查询。`names() const` 返回可发现名称。`query(name) const` 返回值或未知名称诊断。

初始查询包括 `engine.status`、`engine.frameCount` 和 `scene.nodeCount`。文档查询包括 ID、revision 与 contentHash。选择查询使用稳定节点标识。应用通过注册追加自己的命名空间。

验证脚本版本为 1。步骤含 query、assert、wait-until、wait-frames 和 screenshot。输入步骤沿用键、文本、鼠标、拖动与滚轮。edit 步骤只调用注册的生产操作。

- [x] 添加 `unknown_query`、`wait_timeout` 和 `assert_failure` 夹具。
- [x] 添加查询类型、有限值与稳定节点标识的测试。
- [x] 确认固定帧等待不能证明异步加载完成的夹具失败。
- [x] 实现观察注册与条件等待，输出每步结果和耗时。
- [x] 接入生产编辑命令与独立输入事件回放。
- [x] 实现可选本机控制通道，服务端限制回环地址。
- [x] 验证错误令牌、非回环地址与未知操作得到拒绝。
- [x] 用真实场景完成查询、拖动、拾取和截图验证。
- [x] 在报告中记录正式模式、测试模式和渲染设置差异。

**计划测试：** 注册 `AzureEngine.Observation`。协议夹具返回非零表示等待或断言失败。验证脚本按步骤保存 JSON 报告。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureEngine.Observation$' --output-on-failure
python tools/test_validation_protocol.py
```

### F5.2 文本内容与生成来源

**文件：** 修改 `src/runtime/AssetDatabase.hpp`、`AssetDatabase.cpp`、`src/editor/AssetImportJob.cpp` 和资产加载入口。新增 `src/assets/GeneratorRegistry.hpp`、`GeneratorRegistry.cpp`、`GenerationManifest.hpp`。新增 `tests/GenerationManifestTests.cpp`。扩展 `tests/AssetLevelTests.cpp` 与项目打包测试。

**消费：** F4 类型描述、现有 UUID、虚拟路径、依赖与内容哈希。输入文本覆盖关卡、Prefab、动画图和生成器参数。二进制资源继续经资产导入契约管理。

**输出：** `GeneratorRegistry::add(id, version, factory)` 注册生成器。`generate(id, const GenerationRequest&) -> GenerationResult` 输出候选资源。请求声明输入与依赖。结果声明输出、指纹和诊断。

`GenerationManifest` 包含 schemaVersion、generatorId 和 generatorVersion。还包含输入哈希、依赖哈希、输出哈希与许可来源。候选输出经校验后进入缓存。项目移动保持资源 ID 和相对引用。

- [x] 添加两个项目共享生成器的夹具。
- [x] 添加依赖变化、移动路径、输出篡改和取消用例。
- [x] 添加生成失败保持有效缓存与项目引用的用例。
- [x] 确认来源缺失或失效依赖得到明确失败。
- [x] 实现生成注册、候选提交和版本化清单。
- [x] 接入现有文本与导入流程，保存可审阅差异。
- [x] 验证确定输入重复生成结果一致。
- [x] 验证公开包的生成来源和许可证能被审计。

**计划测试：** 注册 `AzureAssets.GenerationManifest`。三种文本资源和一种二进制输入分别往返。非法路径和依赖环拒绝。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^Azure(Assets.GenerationManifest|Engine.AssetLevel)$' --output-on-failure
```

### F5.3 统一开发命令入口

**文件：** 新增 `tools/azure.py`、`tools/test_azure_cli.py`。复用现有配置、来源检查、发布和性能脚本。新增 `docs/reference/developer-cli.md`。更新 README 与构建说明。

**消费：** F5.1 验证协议和 F5.2 清单。命令行读取现有 CMake preset。目标与参数显式传递，构建进程同步完成。

**输出：** doctor、build、run、test、describe、shot 和 validate 子命令。可追加 package 与 provenance 的薄包装。成功输出结构化结果，失败返回非零状态。

- [x] 添加参数转发、非法目标、含空格路径和隔离环境用例。
- [x] 添加构建锁夹具，验证同一配置的并发请求受控。
- [x] 确认无统一配置来源时，错误目标与退出状态用例失败。
- [x] 实现薄入口，复用已有脚本的算法和门禁。
- [x] 完成 describe、query、edit、capture 的组合脚本。
- [x] 分别驱动探索项目与检视工具。
- [x] 记录工具环境、preset、目标和最终退出状态。

**计划测试：** 注册 `AzureTools.Cli`。命令帮助和模式说明纳入参考文档。

```powershell
python tools/test_azure_cli.py
python tools/azure.py doctor --json
python tools/azure.py describe --json
```

**F5 完成门禁：** 两个项目的生产操作与观察闭环通过。控制通道、超时、来源与路径用例通过。完整两配置和制作流程通过。提交 `feat(f5): 完成观察内容与开发工具闭环验收`。

## U4 UI 基础与设置体系

借鉴 B1、B2、B3。前置为 F5。交付通用编辑器 UI 基础和类型化设置注册。

### U4.1 语义样式、尺度与控件

**文件：** 新增 `src/editor/ui/ThemeTokens.hpp`、`UiMetrics.hpp`、`UiScopes.hpp`、`Widgets.hpp`、`Widgets.cpp` 与 `AppChrome.cpp`。修改 `src/editor/EditorTheme.cpp`、`EditorToolbar.cpp` 和 `ImGuiEditorLayer.cpp`。新增 `tests/UiFoundationTests.cpp`，扩展工作区 GPU 回放。

**消费：** 现有 ImGui、字体、DPI 和 sRGB UI 通道。UI 基础拥有颜色、尺度和控件状态。场景与玩法规则由应用服务提供。

**输出：** 语义颜色覆盖文本、表面、边框、强调和结果状态。`UiMetrics::fromScale(float)` 输出统一尺度。按钮选项声明变体、选中、禁用与提示。作用域对象维护样式、ID 和禁用栈。

- [x] 添加中性表面、强调状态和禁用状态的样式用例。
- [x] 添加正常、紧凑、0.75 倍及 3 倍缩放布局夹具。
- [x] 添加作用域提前退出后的 ImGui 栈平衡用例。
- [x] 确认分散控件样式无法满足统一状态夹具。
- [x] 提炼主题、尺度、按钮、属性行与应用外框。
- [x] 将菜单、工具栏、底栏和属性区接入基础组件。
- [x] 验证中文文字、窄窗口、sRGB 和截图一致性。
- [x] 用输入事件验证控件行为与文字焦点保护。

**计划测试：** 注册 `AzureEditor.UiFoundation`。截图判断布局与显示，输入回放判断操作结果。

### U4.2 面板、上下文和选择服务

**文件：** 修改 `src/editor/EditorContext.hpp`、`EditorWorkspaceUI.cpp` 与面板接口。新增 `src/editor/PanelContext.hpp`、`SelectionService.hpp` 和 `SelectionService.cpp`。新增 `src/editor/panels/` 存放面板实现。扩展工作区、拾取与编辑工作流测试。

**消费：** U3 编辑服务、F5 观察和 U4.1 控件。面板得到只读文档视图、选择服务和编辑操作接口。

**输出：** `SelectionService::set(const std::vector<std::string>&)` 使用节点标识。`selected() const` 返回稳定选择。面板描述声明 ID、标题、开关和职责。界面状态由工作区持有。

- [x] 添加独立面板注册与选择跨面板同步用例。
- [x] 添加删除选择、重开布局和损坏布局恢复用例。
- [x] 添加拖放、视口拾取与文本焦点快捷键用例。
- [x] 确认直接访问全部上下文不能满足最小面板夹具。
- [x] 拆分面板上下文与操作服务，保持面板依赖可审查。
- [x] 验证层级、属性、内容、控制台和调试面板。
- [x] 验证九面板布局、三类变换及保存重开。
- [x] 更新空项目教程与正式截图。

### U4.3 类型化设置与来源优先级

**文件：** 新增 `src/foundation/SettingRegistry.hpp`、`SettingRegistry.cpp`。新增 `src/editor/panels/SettingsPanel.cpp`。修改 `src/render/RenderSettings.hpp`、`RenderSettings.cpp`、`src/app/CommandLine.cpp` 和输入配置读取。新增 `tests/SettingRegistryTests.cpp`。

**消费：** F4 元数据和 G8 动作配置。项目文件拥有玩法配置。设置注册表管理引擎、诊断和用户偏好。

**输出：** `SettingRegistry::add(SettingDescriptor)` 声明类型和约束。`set(name, value, SettingSource) -> SettingResult` 处理写入。`describe() const -> Json` 描述来源和状态。`saveUser(path)` 保存允许持久化的字段。

来源顺序为默认值、默认文件、用户文件、项目、命令行和控制台。高优先级覆盖低优先级。标记包含只读、持久化和启动专用。修改在约定帧边界生效。

- [x] 添加每种来源覆盖与重复加载的测试。
- [x] 添加类型错误、范围错误、只读和启动后修改用例。
- [x] 添加保存重开、缺失用户文件和损坏用户配置用例。
- [x] 确认配置来源不可追踪时，优先级测试失败。
- [x] 实现注册、搜索、重置、来源诊断与帧边界应用。
- [x] 接入渲染、诊断与编辑器偏好，保持项目格式兼容。
- [x] 验证 Player 与编辑器的默认质量及正式发布配置。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^Azure(Editor.UiFoundation|Editor.Workspace|Engine.SettingRegistry|Engine.EditorWorkflow)$' --output-on-failure
python tools/test_editor_workspace.py --executable build/ninja-msvc-debug/AzureRender.exe --output build/evolution/u4/workspace
```

最后一条运行实际 GPU 工作区回放并保存产物。U4.2 的拾取与布局测试归入 Workspace。U4.3 注册 `AzureEngine.SettingRegistry`。

**U4 完成门禁：** 共用控件和最小上下文在两种工作流可用。DPI、焦点、布局、设置和教程通过。九轮性能记录实际编辑视口。提交 `feat(u4): 完成界面基础与设置体系验收`。

## G9 有界 AI 内容辅助

借鉴 A6、B4。前置为 U4。交付可选模型服务、领域提案、有限修复和可回放验证。

### G9.1 模型客户端与进程协议

**文件：** 新增 `src/ai/ModelClient.hpp`、`ModelClient.cpp`、`ModelProcess.cpp`。新增 `tools/ai_bridge.py` 与 `tools/ai/` 的 provider、profile 和 session 实现。新增 `tests/ModelProtocolTests.cpp`、`tools/test_ai_bridge.py` 和共享协议夹具。

**消费：** F4 模块装配和 U4 设置。服务只接收模型请求与响应。应用传入提示、模式和请求预算。

**输出：** `IModelTransport::request(const ModelRequest&)` 返回可等待结果。`cancel(runId)` 取消请求。协议使用版本化 NDJSON 与 JSON-RPC 2.0。方法包含 initialize、llm.chat、run.cancel 与 shutdown。

配置与会话方法支持 providers.list、profiles.list 和 session 生命周期。领域工作流可声明 workflow.run。结构化状态区分原生模式、JSON 模式与提示约束模式。运行快照采用 stateless 请求。

- [ ] 添加握手、版本拒绝、乱序事件与非法帧用例。
- [ ] 添加服务缺失、进程退出、超时和取消用例。
- [ ] 让 C++ 客户端和 Python bridge 读取同一协议夹具。
- [ ] 确认服务失效不会中断渲染与既有项目。
- [ ] 实现可选客户端、协议、配置与 provider 路由。
- [ ] 使用本机固定响应服务验证异步和流式结果。
- [ ] 有可用模型配置时另行记录真实接入证据。
- [ ] 报告区分固定响应、真实服务与模型质量验证。

**计划测试：** 注册 `AzureAI.ModelProtocol`。协议服务的凭据属于工具配置。正式包按项目需求选择是否带模型服务。

### G9.2 领域提案、校验与生产应用

**文件：** 新增 `src/editor/ai/ProposalContracts.hpp`、`ProposalController.cpp`、`ValidationPolicy.hpp`。新增 `src/editor/ai/adapters/` 存放领域适配器。新增 `tests/ProposalControllerTests.cpp`、`ProposalAdapterTests.cpp` 和固定响应夹具。

**消费：** U3 的 DocumentVersion、EditRequest 与 EditService。F4 提供合法字段，F5 提供资产来源。G9.1 只负责传输。

**输出：** `ProposalController::generate(const ProposalRequest&)` 启动请求。`cancel()` 结束当前请求。`validate()` 形成候选差异。`apply(EditService&) -> EditResult` 检查版本并执行批量操作。

提案状态为 Idle、Generating、Validating、Ready 和 Stale。结束状态为 Applied、Rejected、Cancelled 或 Error。目标、基础版本、诊断和修复次数进入报告。

初始预算为 128 项操作、2 MiB 源文本和 1 次修复。历史最多 24 条、128 KiB。单请求期限为 300000 ms，同文档最多一个生成请求。预算可通过受校验配置调整，变更需独立验收。

- [ ] 为场景装配和资产参数生成分别建立领域适配器。
- [ ] 添加非法字段、非有限值、未知引用与越界路径夹具。
- [ ] 添加恰好一次修复、修复耗尽与取消用例。
- [ ] 添加撤销、重开、目标变化后的过期结果用例。
- [ ] 确认直接使用模型输出不能通过领域验证夹具。
- [ ] 实现候选生成、语义差异、预算和版本判断。
- [ ] 经 U3 批量操作应用，验证失败恢复与撤销。
- [ ] 在两种工作流中回放生成、预览、拒绝和应用。
- [ ] 验证模型服务关闭时，人工编辑与项目运行正常。

```powershell
python tools/test_ai_bridge.py
ctest --test-dir build/ninja-msvc-debug -R '^Azure(AI.ModelProtocol|Editor.ProposalController|Editor.ProposalAdapter|Editor.EditTransaction)$' --output-on-failure
```

**G9 完成门禁：** 固定响应、有限修复、过期拒绝和事务恢复通过。领域规则与传输依赖分离。完整回归与运行性能通过。提交 `feat(g9): 完成有界内容提案与模型接入验收`。

## R7 开发期资源与预览

借鉴 B6、B7。前置为 G9。交付安全热重载、独立视图和按需资产预览。

### R7.1 热重载服务与安全替换

**文件：** 新增 `src/render/IShaderHotReloader.hpp`。新增 `src/devtools/ShaderHotReloader.cpp` 和 `ShaderCompileJob.cpp` 提供开发实现。修改模块装配、资产刷新、RenderGraph 和宿主资源替换入口。新增 `tests/HotReloadTests.cpp` 与 GPU 生命周期夹具。

**消费：** F4 装配、F5 依赖清单和 U4 设置。编译器使用构建配置发现的工具路径。发布模式按需关闭开发服务。

**输出：** `IShaderHotReloader::poll()` 处理变更。`requestRebuild()` 提交任务。`status() const` 返回编译、待提交与诊断状态。替换在 GPU 完成信号满足后提交。

- [ ] 添加编译失败保留当前有效 Pipeline 的用例。
- [ ] 添加依赖文件变化、重复请求和关闭取消用例。
- [ ] 添加在途帧替换与退出释放的 GPU 验证。
- [ ] 确认直接覆盖资源不能满足安全替换夹具。
- [ ] 实现监听、编译候选、校验与帧边界替换。
- [ ] 验证资产和 Lua 重载共享各自来源与失败诊断。
- [ ] 在开发服务关闭状态验证独立 Player。
- [ ] 记录 C++ 热编译的接口、成本与采用范围评估。

### R7.2 视图服务与资产缩略图

**文件：** 新增 `src/render/RenderViewService.hpp`、`RenderViewService.cpp`。新增 `src/editor/preview/AssetThumbnailService.hpp`、`AssetThumbnailService.cpp`。修改 `RenderGraph`、`RenderContext` 和内容浏览器。新增 `tests/RenderViewTests.cpp` 与 GPU 预览脚本。

**消费：** 当前离屏视口、场景快照与资源分配。视图拥有独立相机、尺寸、历史和生命周期。缩略图以资源 ID、指纹和预览设置作为缓存键。

**输出：** `create(const RenderViewDescriptor&) -> RenderViewHandle`。`request(RenderViewHandle)` 按需调度。`resize(handle, extent)` 更新尺寸。`release(handle)` 释放视图，失效句柄得到拒绝。

初始次视图预算为 3，计入设置与报告。实际采用容量根据完整 GPU 预算验收。静态缩略图完成后暂停更新。场景卸载使其关联历史和缓存失效。

- [ ] 添加相机独立、尺寸独立和失效句柄用例。
- [ ] 添加关闭面板后停止更新和资产变更后的缓存失效用例。
- [ ] 添加窗口恢复、多视图卸载与内存释放用例。
- [ ] 确认共用主视图历史不能通过隔离夹具。
- [ ] 实现视图句柄、资源所有权和 RenderGraph 调度。
- [ ] 接入资产缩略图、相机预览与截图服务。
- [ ] 测量开启与关闭预览的 CPU、GPU 和显存。
- [ ] 验证三场景、Intel 与 RTX 的正式范围。

**计划测试：** 注册 `AzureRender.HotReload` 和 `AzureRender.RenderView`。GPU 验证使用生产编译与资源路径。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureRender.(HotReload|RenderView|RenderGraph|SurfaceLifecycle)$' --output-on-failure
```

**R7 完成门禁：** 资源失败恢复、在途替换和视图隔离通过。完整视觉、九轮性能和释放门禁通过。预览能力用于编辑器和独立工具。提交 `feat(r7): 完成开发期重载与资产预览验收`。
