# 引擎基础与操作契约实施步骤

> 文档类型：实施子计划
> 状态：已采纳，执行状态见总计划
> 更新日期：2026-10-06
> 执行方式：按 F4、G8、U3 顺序实施与验收

## 目标与公共约束

本计划建立模块、类型、机制和编辑操作的公共边界。设计依据与门禁见[主实施计划](2026-10-06-engine-evolution.md)。下列接口是实施目标。文件路径相对于 `Project/AzureRender`。

现有类型、项目和文件版本保持其兼容规则。新增绑定复用反射元数据。具体玩法通过应用装配。阶段提交采用主计划的独立验收规则。

## F4 模块与类型契约

借鉴 A1、A2。前置为 P2。交付依赖检查、服务装配、组件注册和属性描述。核心与编辑器分别具有最小运行测试。

### F4.1 单向依赖与服务生命周期

**文件：** 修改 `CMakeLists.txt`、`src/runtime/RuntimeLifecycle.hpp` 和 `src/extensions/ExtensionRegistry.hpp`。新增 `src/runtime/ModuleAssembly.hpp`、`src/runtime/ModuleAssembly.cpp`。新增 `tools/check_module_boundaries.py`、`tests/ModuleAssemblyTests.cpp` 和 `tools/test_module_boundaries.py`。

**消费：** 现有 `ExtensionDescriptor`、`RuntimeLifecycle` 和模块构建目标。依赖检查读取 CMake 目标关系与第一方 include。生成文件和第三方头文件分别分类。

**输出：** `ModuleAssembly::add(ExtensionDescriptor, Start, Stop)` 注册服务。`Start` 为 `std::function<void(RuntimeLifecycle&)>`。`Stop` 为 `std::function<void()>`。`start(RuntimeLifecycle&)` 按依赖启动，`stop() noexcept` 逆序关闭。

注册检查空 ID、重复 ID、版本和依赖环。启动失败关闭本次已启动服务。重复关闭保持有效。生命周期由装配对象拥有，工作在主线程。

启动回调通过异常报告失败并附模块诊断。关闭回调异常转为诊断，后续服务继续关闭。安装版本与缺失依赖在启动前检查。

- [x] 记录实际目标依赖与 include 关系，标注核心、可选实现和应用。
- [x] 增加 `reject_cycle`、`rollback_start_failure` 和 `stop_once` 用例。构造三模块链，确认启动正序与关闭逆序。
- [x] 增加重复 ID、缺失依赖、版本拒绝与关闭回调异常用例。
- [x] 增加边界检查夹具，分别含合法依赖与 Runtime 引用 Editor。
- [x] 运行夹具与装配测试，确认违例得到确定失败。
- [x] 实现装配与依赖检查，限定公共头文件暴露范围。
- [x] 将核心、编辑宿主和 Player 的启动关闭接入装配契约。
- [x] 验证关闭可选服务后的最小 Player 与独立检视宿主夹具。
- [x] 同步模块图、所有权、线程与启动失败恢复说明。

**计划测试：** 注册 `AzureEngine.ModuleAssembly`。检查脚本返回零表示无违例，合法例通过，非法例拒绝。

```powershell
python tools/test_module_boundaries.py
python tools/check_module_boundaries.py --source . --build-dir build/ninja-msvc-debug
ctest --test-dir build/ninja-msvc-debug -R '^AzureEngine.ModuleAssembly$' --output-on-failure
```

### F4.2 组件注册与属性描述

**文件：** 修改 `src/reflection/Registry.hpp`、`Registry.cpp`、`tools/metagen/main.cpp` 和 `src/runtime/ComponentCodec.hpp`。新增 `src/runtime/ComponentRegistry.hpp`、`ComponentRegistry.cpp`。扩展 `tests/ReflectionTests.cpp`、`tests/AssetLevelTests.cpp`。新增 `tests/ComponentRegistryTests.cpp`。

**消费：** 当前 `reflection::Type`、迁移、稳定类型标识和 `ecs::World`。`ComponentRegistry` 接收反射注册表。注册绑定提供安装、编码和移除回调。

**输出：** `add(ComponentBinding)` 登记类型能力。`install(const std::string&, ecs::World&, ecs::Entity, const Json&)` 安装组件。`encode(const std::string&, const ecs::World&, ecs::Entity) const` 返回版本化 JSON。`describe(const std::string&) const` 输出机器可读属性契约。

`Json` 统一为 `nlohmann::json`。`ComponentBinding` 包含类型名及三项回调。属性补充分类、提示、只读和工具暴露标记。类型、范围、字段和迁移仍由反射规则负责。

- [x] 添加外部注册组件夹具，以第二个组件证明安装与序列化。
- [x] 添加 `unknown_type`、`unknown_field`、`non_finite_value` 和 `version_migration` 用例。
- [x] 添加只读字段与工具隐藏字段的描述和写入限制用例。
- [x] 运行新增测试，确认缺少注册与权限校验时失败。
- [x] 实现注册绑定及元数据扩展，接入生成与迁移契约。
- [x] 让加载、编辑和脚本的组件操作消费注册表。
- [x] 验证新增组件进入面板、安装、往返和包加载。
- [x] 核对既有组件版本与合法文件，记录字段兼容策略。

**计划测试：** 注册 `AzureEngine.ComponentRegistry`。扩展 Reflection、AssetLevel 与 ScriptEntities 的用例。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureEngine.(ComponentRegistry|Reflection|AssetLevel|ScriptEntities)$' --output-on-failure
```

**F4 完成门禁：** 两种使用方式证明组件与服务可注册。依赖检查和完整两配置回归通过。服务关闭、安装移动和 Player 独立性通过。同步文档并提交 `feat(f4): 完成模块装配与类型契约验收`。

F4 的第二种使用方式采用独立宿主夹具。G8 将其扩展为正式场景检视工具。夹具使用不同服务组合与组件操作，验证公共契约。

## G8 共享机制与应用分层

借鉴 B8，落实 A1 的可组合性。前置为 F4。交付通用运行调度、可选机制和应用策略。探索项目与检视工具采用不同系统组合。

### G8.1 系统装配与可配置控制

**文件：** 修改 `src/runtime/GameRuntime.hpp`、`GameRuntime.cpp`、`InputActions.hpp`、`GameComponents.hpp` 和 `CMakeLists.txt`。新增 `src/runtime/IRuntimeSystem.hpp`、`SystemRegistry.hpp`、`SystemRegistry.cpp`。新增 `src/gameplay/CharacterMovementSystem.cpp`、`CameraFollowSystem.cpp` 和 `LocomotionSystem.cpp`。

**测试：** 新增 `tests/SystemCompositionTests.cpp`。扩展 `tests/ThirdPersonTests.cpp`、`tests/PhysicsInputTests.cpp` 与角色动画测试。新增 `assets_public/scene_inspector/project.azureproject` 及其场景配置。

**消费：** F4 注册和生命周期。`RuntimeSystemContext` 提供 `RuntimeLifecycle&`、`InputActions&` 与固定步长。`IRuntimeSystem` 定义初始化、固定步、场景变化与关闭。

**输出：** `SystemRegistry::add(const std::string&, Factory)` 注册系统工厂。`create(const std::string&, const Json&) const` 返回系统实例。`GameRuntime` 按项目描述调用系统。`InputActions::configure(const Json&)` 读取动作绑定。

输入配置声明动作名与键码列表。角色与动画驱动通过组件参数选择动作与状态。默认探索配置由应用拥有。场景检视工具配置独立相机，按需装配物理与角色系统。

- [x] 记录固定步、脚本、物理、相机与渲染快照的调用顺序。
- [x] 添加 `different_system_sets` 和 `register_without_loop_edit` 用例。
- [x] 添加重绑定、焦点释放、双 Shift、对角移动与场景重开用例。
- [x] 添加不同动画状态名夹具，验证驱动读取配置。
- [x] 确认新增行为在现有固定系统组合上失败。
- [x] 实现系统注册与装配，将角色机制置于共享机制模块。
- [x] 以应用配置保存探索默认动作、相机和动画驱动规则。
- [x] 验证 60 Hz 固定步、暂停单步与插值保持其契约。
- [x] 用场景检视工具证明核心调度支持第二种工作流。

**计划测试：** 注册 `AzureEngine.SystemComposition`。两种组合加载后，核心调度执行次数正确。未知系统与非法配置明确拒绝。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureEngine.(SystemComposition|ThirdPerson|PhysicsInput|Locomotion)$' --output-on-failure
```

### G8.2 通用交互与应用任务策略

**文件：** 修改 `src/runtime/InteractionRuntime.hpp`、`GameRuntime.cpp` 和探索项目脚本。新增 `src/gameplay/InteractionPolicy.hpp`。扩展 `tests/InteractionTests.cpp` 与 `tests/PlayableQuestTests.cpp`。更新 `tests/SystemCompositionTests.cpp`。

**消费：** 通用目标、实体身份、场景版本与物理查询。`InteractionPolicy` 为只读可用性回调。参数为运行时、发起实体和目标实体，返回 `bool`。

**输出：** `InteractionRuntime::setPolicy(InteractionPolicy)` 配置筛选。距离、遮挡、稳定选择与输入分派由通用机制负责。收集状态、门锁和任务进度由应用策略与脚本处理。

- [x] 添加只有通用可交互组件的目标夹具。
- [x] 添加 `application_policy_filters` 与稳定距离平局用例。
- [x] 添加已删除目标、场景切换、策略缺失和重复交互用例。
- [x] 确认核心对门和收集物的专用判断不能满足通用夹具。
- [x] 将任务相关判断与反馈接入应用策略和事件。
- [x] 验证探索的收集、开门、检查点、重开与完整任务。
- [x] 验证检视工具使用相同交互机制选择普通资产。
- [x] 同步组件归属、策略默认值与事件顺序说明。

**G8 完成门禁：** 两种工作流无需各自修改核心循环。既有 3C、动画与任务回归通过。完整两配置、视觉和九轮性能通过。提交 `feat(g8): 完成共享机制与应用玩法分层验收`。

## U3 编辑操作与文档事务

借鉴 A3，并建立 A6 的基础版本契约。前置为 G8。交付公共操作描述、生产执行器、文档版本与失败恢复。

### U3.1 统一操作描述与入口

**文件：** 修改 `src/editor/EditorSession.hpp`、`EditorSession.cpp`、`EditorAutomation.cpp`、`EditorToolbar.cpp` 和面板操作入口。新增 `src/editor/commands/EditContracts.hpp`、`EditRegistry.hpp`、`EditRegistry.cpp`、`EditService.hpp` 和 `EditService.cpp`。新增 `tests/EditServiceTests.cpp`。

**消费：** F4 属性描述、G8 场景与现有编辑上下文。操作注册表声明 ID、参数模式、启用条件与修改属性。

**输出：** `EditRegistry::add(EditDescriptor, Handler)` 注册操作。`describe() const -> Json` 提供发现入口。`EditService::execute(const EditRequest&) -> EditResult` 执行生产操作。

`EditRequest` 包含请求 ID、命令 ID、参数和基础版本。`EditResult` 包含状态、结果版本、差异与诊断。状态为 Applied、Rejected、Stale 或 Failed。操作 ID 保持稳定且具有描述版本。

- [x] 盘点现有菜单、快捷键、脚本与自动命令的操作清单。
- [x] 添加 `all_frontends_same_document` 用例，比较相同操作结果。
- [x] 添加未知操作、非法参数、运行中编辑和构建中编辑用例。
- [x] 确认当前独立分派入口缺少统一描述时测试失败。
- [x] 实现操作注册、参数校验和生产执行器。
- [x] 接入现有 UI、快捷键和自动命令，保留输入回放独立路径。
- [x] 验证创建、导入、组件编辑、选择、保存与构建。
- [x] 同步操作参考文档、错误码和授权范围处理规则。

**计划测试：** 注册 `AzureEditor.EditService`。已有 EditorSession、EditorWorkflow 与 Workspace 回归继续执行。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^Azure(Editor.EditService|Engine.EditorWorkflow|Render.EditorSession|Editor.Workspace)$' --output-on-failure
```

### U3.2 文档版本、批量修改与历史

**文件：** 新增 `src/editor/commands/DocumentVersion.hpp`、`EditTransaction.hpp` 和 `EditTransaction.cpp`。修改 `src/editor/EditorContext.hpp`、`EditorContext.cpp` 和 U3.1 的执行器。新增 `tests/EditTransactionTests.cpp`，扩展会话与工作流测试。

**消费：** U3.1 请求与结果。`DocumentVersion` 包含文档 ID、单调修订号和规范内容哈希。版本比较同时检查三者。

**输出：** `EditService::version() const -> DocumentVersion`。`executeBatch(const std::vector<EditRequest>&) -> EditResult` 形成一个事务。`undo()`、`redo()` 返回结果，并更新修订号。`EditTransaction` 保存候选文档和提交前状态。

事务先校验候选，再应用整体修改。失败保持有效文档与历史。合并键标识同一连续编辑。保存重开与撤销都能使既有异步提案过期。

- [x] 添加 `second_operation_failure_rolls_back`，使第二项故意失败。
- [x] 验证失败前后的文档、选择、脏状态与历史完全一致。
- [x] 添加合并拖动、单次批量撤销和重做用例。
- [x] 添加撤销后旧提案、文档重开和目标替换的 Stale 用例。
- [x] 运行测试，确认现有快照历史缺少完整事务契约时失败。
- [x] 实现候选、提交、恢复与版本比较。
- [x] 验证 Play/Stop 保持编辑态，自动工具消费同一版本。
- [x] 在编辑教程中记录批量操作和失败恢复结果。

**计划测试：** 注册 `AzureEditor.EditTransaction`。成功批量产生一个历史单元。非法提案保持原文档，撤销后修订号仍递增。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureEditor.(EditService|EditTransaction)$' --output-on-failure
```

**U3 完成门禁：** 菜单、快捷键和自动操作的文档结果一致。事务、撤销、过期拒绝与预览恢复通过。完整回归、制作流程和性能通过。提交 `feat(u3): 完成统一编辑操作与文档事务验收`。
