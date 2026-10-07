# 引擎专项与交付实施步骤

> 文档类型：实施子计划
> 状态：已采纳，执行状态见总计划
> 更新日期：2026-10-06
> 执行方式：按 G10、R8、R9、G11、P3 顺序验收

## 目标与专项采用规则

本计划覆盖全部 C 类参考和最终交付。设计依据见[主实施计划](2026-10-06-engine-evolution.md)。路径相对于 `Project/AzureRender`。专项分别交付可运行原型、对照证据和采用结论。

原型先在独立目标或可选模块中运行。正式采用要求收益、视觉、设备和生命周期通过门禁。适配采用保留公共设计并说明实现范围。原型保留提供已验证产物与适用边界。

阶段结论不能用缺少证据替代。每项内容都须完成实现或原型验证任务。支持设备、项目格式和正式预算保持有效。工具链缺失按实际阻塞处理。

## G10 程序化内容模块

借鉴 C1，消费 F5 文本与来源能力、G9 领域验证。前置为 R7。交付参数化几何、受限源契约和文本刚体动画。

### G10.1 参数化几何与生成器适配

**文件：** 新增 `src/assets/generators/ProceduralContracts.hpp`、`ScadGenerator.cpp` 与 `GeometryCompilerAdapter.cpp`。新增 `assets_public/procedural/` 的参数化内容。扩展 `GeneratorRegistry` 与 `GenerationManifest`。新增 `tests/ProceduralGeometryTests.cpp` 和图像夹具。

**消费：** F5 生成请求、来源与许可清单。参数文件版本为 1。资源引用使用项目挂载路径。

**输出：** `ProceduralGeometryRequest` 声明源、参数和随机种子。生成器返回网格、材质和节点候选。支持基础几何、模块、参数与变换。组合覆盖 union、difference 和 intersection。

源码契约明确支持的 SCAD 子集或等价文本表达。编译适配先评估现有实现和离线编译器。布尔几何优先使用成熟库。完整语言语义作为独立采用范围记录。

- [x] 建立相同源生成不同建筑和道具的两项目夹具。
- [x] 添加包围盒、坐标转换、固定种子与重复输出用例。
- [x] 添加非法语法、越界引用、依赖环和复杂度超限用例。
- [x] 添加布尔运算、非闭合输入与退化几何夹具。
- [x] 确认现有生成器缺少该契约时测试失败。
- [x] 实现编译适配和受限契约，记录每项语言语义。
- [x] 将校验候选接入 F5 生成、预览与资源安装。
- [x] 验证取消和失败保持有效缓存与文档。
- [x] 比较直接导入与生成后的几何、耗时和缓存体积。

初始预算为 2 MiB 源文本、64 层展开深度和 100 万三角形。单次生成期限为 60000 ms。预算进入版本化配置及超限测试。预算变更具有单独证据。

### G10.2 文本刚体骨骼与关键帧

**文件：** 新增 `src/assets/generators/RigTextContracts.hpp` 和 `RigTextGenerator.cpp`。新增 `assets_public/procedural/rig/` 的公开动作夹具。扩展实体动画与 G9 领域适配器。新增 `tests/RigTextTests.cpp`。

**消费：** 节点、骨骼层级与已有动画数据。文本描述部件绑定、初始枢轴和关键帧。ScadRig 的 bone_、anim_ 约定进入参考适配测试。

**输出：** `RigTextRequest` 声明源与缩放。`RigTextGenerator::generate(const RigTextRequest&)` 输出注册资产。通道类型包含位移、旋转与缩放。夹具明确循环与非循环动作。

- [x] 添加父子骨骼、刚体部件和两个动作夹具。
- [x] 添加未知骨骼、层级环、非法时间和非正缩放用例。
- [x] 验证模型坐标与引擎坐标转换一致。
- [x] 确认受限刚体资产通过专门契约加载。
- [x] 实现文本动作适配，保存来源与诊断。
- [x] 验证多个实例独立播放、切换和释放。
- [x] 验证现有 glTF 蒙皮和私有动画保持兼容。
- [x] 通过 G9 固定响应生成参数或动作候选并校验。

**计划测试：** 注册 `AzureAssets.ProceduralGeometry` 和 `AzureAssets.RigText`。来源清单记录编译器、几何库、坐标与许可。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureAssets.(ProceduralGeometry|RigText|GenerationManifest)$' --output-on-failure
```

**G10 完成门禁：** 两种内容与两个项目证明参数化复用。源语义、复杂度、动画与现有资产回归通过。完整两配置和公开包通过。提交 `feat(g10): 完成程序化内容与文本动画原型验收`。

## R8 着色模块与算法组合

借鉴 C2。前置为 G10。交付共享类型、等价 Slang Pass 和算法组合原型。采用判断基于同机同负载对照。

### R8.1 工具链、共享类型与等价 Pass

**文件：** 扩展 `CMakeLists.txt` 的着色编译入口。新增 `shaders/modules/` 与 `tools/compile_shader_module.py`。新增 `tests/ShaderModuleTests.cpp` 与构建夹具。复用 `RenderGraph`、帧快照和现有 Compute Pass。

**消费：** 当前 GLSL/SPIR-V、Vulkan 1.3 和稳定视觉基线。Slang 编译器版本在来源记录中固定。编译步骤与运行资源路径保持一致。

**输出：** `ShaderCompileRequest` 声明源、入口、目标和依赖。`ShaderCompileResult` 声明 SPIR-V、类型布局与诊断。共享类型声明由受校验描述生成或核对。

- [x] 选择一个现有 Compute Pass，冻结输入与输出基线。
- [x] 添加缺编译器、模块依赖、布局错位与非法入口用例。
- [x] 添加常量、成员偏移和缓冲跨度一致性用例。
- [x] 确认不一致布局被构建或契约测试拒绝。
- [x] 实现独立编译适配与等价 Pass。
- [x] 对照 GLSL 输出，检查设备 Validation。
- [x] 比较完整编译、增量编译与运行 CPU/GPU 时间。
- [x] 在 RTX 和 Intel 验证相同支持范围。

### R8.2 策略组合与采用结论

**文件：** 新增 `shaders/modules/interfaces/` 和 `shaders/modules/strategies/`。新增 `tests/ShaderCompositionTests.cpp` 与专项基准脚本。更新着色构建、来源清单和运行时设计。

**消费：** R8.1 类型与编译契约。参考的追踪、直接光照与缓存接口用作分解依据。原型先组合本项目已有算法。

**输出：** 公共算法通过满足同一契约的两种策略调用。入口只负责选择实现与调度。材质处理、资源访问和算法内部职责清楚。

- [x] 添加两种策略共享同一算法的输出夹具。
- [x] 添加接口不匹配、材质类型与无缓存策略用例。
- [x] 确认替换策略能通过同一调用契约。
- [x] 实现模块与策略组合，记录生成变体数量。
- [x] 测量维护入口、编译成本与性能回归。
- [x] 按完整证据记录正式采用、适配采用或原型保留。
- [x] 验证现有三场景和安装着色器清单。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureRender.(ShaderModule|ShaderComposition|ComputePass|RenderGraph)$' --output-on-failure
```

**R8 完成门禁：** 等价 Pass 与策略组合均可运行。布局、图像、构建和设备证据完整。九轮正式性能满足预算。提交 `feat(r8): 完成着色模块与算法组合原型验收`。

## R9 资源索引与 GPU 可见性

借鉴 C3。前置为 R8。交付设备能力、规模对照与 GPU 可见性原型。复用现有 Bindless、GPU 场景与间接提交基础。

### R9.1 资源访问与设备契约

**文件：** 修改现有 RHI 能力报告、GPU 场景和资源表。新增 `src/render/ResourceAccessProfile.hpp`、`ResourceAccessProfile.cpp`。扩展 `tests/GpuCapabilityReportTests.cpp`、NullRHI 与资源释放测试。

**消费：** 当前固定描述符与 Bindless 路径。能力包括描述符索引、地址、容量和更新规则。参数来自实际设备查询。

**输出：** `ResourceAccessProfile::select(const DeviceCapabilities&)` 返回合法资源访问方式。配置声明所需功能与容量。逻辑资源句柄和在途销毁具有明确生命周期。

- [x] 核对当前 Bindless 和地址能力的已有实现与缺口。
- [x] 添加容量不足、功能缺失、过期句柄和资源释放夹具。
- [x] 添加固定描述符与索引访问的等价资源用例。
- [x] 确认仅按设备名称判断能力的实现不能通过测试。
- [x] 实现能力档位、资源表与明确诊断。
- [x] 验证 GPU 同步、帧中更新与卸载释放。
- [x] 记录 Intel 与 RTX 实际选择的访问方式。

### R9.2 可见性、表面数据与规模基准

**文件：** 扩展现有裁剪、间接提交和 RenderGraph。新增 `src/render/VisibilityPrototype.hpp`、`VisibilityPrototype.cpp`。新增 `tests/VisibilityPrototypeTests.cpp`、`tools/run_visibility_scale_benchmark.py` 与公开场景夹具。

**消费：** 当前帧快照、动画实例和资源访问档位。剔除输出间接命令或可见表面数据。材质着色消费稳定的实例与表面标识。

**输出：** 相同场景能切换已有路径与原型路径。查询记录总实例、可见实例、提交和表面重建成本。主线程读取结果与视口拾取保持一致语义。

- [x] 建立 100、500、10000 实例的独立基准夹具。
- [x] 添加镜像、边界、蒙皮、透明、阴影和拾取用例。
- [x] 添加历史遮挡失效、相机跳变与场景切换用例。
- [x] 确认错误剔除和标识不一致产生可判定失败。
- [x] 实现 GPU 可见性原型及可切换正式路径。
- [x] 对照默认图像、逐 Pass 时间与 CPU 提交成本。
- [x] 验证固定描述符设备和资源容量边界。
- [x] 记录收益规模、限制与采用范围。

新增万实例负载使用独立实验预算。标准与压力负载继续使用主计划预算。实验指标必须记录全部质量设置。现有设备范围与正式路径继续通过验收。

```powershell
ctest --test-dir build/ninja-msvc-debug -R '^AzureRender.(VisibilityPrototype|SceneVisibility|GpuCapabilityReport|NullRhi)' --output-on-failure
python tools/run_visibility_scale_benchmark.py --output build/evolution/r9/scale
```

**R9 完成门禁：** 能力选择、视觉一致与规模对照完整。GPU 与 CPU 收益使用同机证据。正式预算与生命周期通过。提交 `feat(r9): 完成资源索引与可见性原型验收`。

## G11 脚本后端与生成绑定

借鉴 C4。前置为 R9。交付脚本服务边界、单一绑定描述与第二后端原型。Lua 玩法和预览恢复保持其契约。

### G11.1 Lua 服务与绑定描述

**文件：** 新增 `src/runtime/IScriptRuntime.hpp`、`src/scripting/BindingDescriptor.hpp`。重构 `src/runtime/ScriptRuntime.hpp`、`ScriptRuntime.cpp` 的后端归属。新增 `tools/generate_script_bindings.py`。扩展 ScriptRuntime、ScriptEntities 与模块装配测试。

**消费：** F4 类型、G8 生命周期和已有 sol2 调用。绑定描述使用稳定 API 名、类型、版本和权限。

**输出：** `IScriptRuntime` 声明启动、固定步、事件、重载和关闭。宿主接收运行时实现。生成器输出 Lua 适配描述与托管绑定，检查模式验证生成结果一致。

- [x] 添加对象删除、场景切换、回调异常和重载用例。
- [x] 添加非法绑定版本、只读字段与失效实体句柄用例。
- [x] 确认多个手工绑定面漂移被生成检查拒绝。
- [x] 建立唯一绑定描述与可替换脚本服务。
- [x] 验证 Lua 的角色、触发器、切关与错误恢复。
- [x] 验证服务关闭和 Play/Stop 编辑态恢复。

### G11.2 托管后端与生命周期原型

**文件：** 新增 `src/scripting/dotnet/`、`managed/EngineBindings/` 和独立后端探针。新增 `tests/ScriptBackendContractTests.cpp` 与构建、运行夹具。更新工具、安装和许可清单。

**消费：** G11.1 服务与同一绑定描述。原型评估 CoreCLR 热重载与 NativeAOT 发布。两者共享托管 API 与测试输入。

**输出：** 独立探针验证初始化、组件访问、事件与关闭。构建声明可用能力和工具链。正式项目通过可选装配选择后端。

- [x] 核对 .NET 工具链、运行库、分发条件与许可。
- [x] 添加握手版本、字符串与数组、句柄失效和异常夹具。
- [x] 使用同一测试比较 Lua 与托管后端结果。
- [x] 验证 CoreCLR 重载、NativeAOT 退出与正常释放。
- [x] 测量调用成本、构建体积与启动耗时。
- [x] 记录平台和后端能力矩阵及采用结论。
- [x] 验证未启用托管模块的既有 Player。

**计划测试：** 注册 `AzureEngine.ScriptBackendContract`。两种后端消费同一契约夹具。已有 ScriptRuntime、ScriptEntities 和 F4 装配继续回归。

```powershell
python tools/generate_script_bindings.py --check
ctest --test-dir build/ninja-msvc-debug -R '^AzureEngine.(ScriptBackendContract|ScriptRuntime|ScriptEntities|ModuleAssembly)$' --output-on-failure
```

**G11 完成门禁：** Lua 契约和生成绑定回归通过。第二后端的生命周期与能力证据完整。项目、预览和发布兼容通过。提交 `feat(g11): 完成脚本服务与生成绑定原型验收`。

## P3 引擎复用与交付验收

前置为 G11。汇总全部 A、B、C 项，完成真正的引擎交付验证。

### P3.1 双工作流与全部门禁

**文件：** 更新 README、教程、运行时设计和 `CHANGELOG.md`。扩展现有发布、长跑、来源和安装检查。新增 `tools/verify_evolution_coverage.py`、阶段证据与最终采用清单。

**消费：** 所有阶段接口、18 项来源与采用结果。构建产物、测试发现列表和原始测量共同形成发布依据。

**输出：** 探索游戏包和场景检视工具包。二者可独立移动并运行。引擎安装包包含文档、许可、接口与来源清单。

- [ ] 将每项借鉴关联实际接口、任务、测试、提交和结论。
- [ ] 验证 18 项均有产物，所有风险均有测试或明确限制。
- [ ] 核对测试发现列表，执行完整 Debug 与 Release。
- [ ] 执行发布门禁、三场景视觉与九轮性能。
- [ ] 在 RTX 与 Intel 验证正式范围与模块关闭组合。
- [ ] 从空项目完成制作、撤销、保存重开和独立构建。
- [ ] 将两个包移到含空格路径，隔离 PATH 并隐藏源项目。
- [ ] 验证完整任务、20 次切关、20 次重开与篡改拒绝。
- [ ] 运行真实 1800 秒长跑，验证窗口恢复和正常释放。
- [ ] 核对源码、媒体、原始记录、许可与包哈希。
- [ ] 记录实体键鼠复核的实际执行范围。
- [ ] 完成差异、暂存和文档检查后提交阶段。

**计划测试：** 覆盖检查要求所有源项都有证据。完整回归中任一失败均阻止交付。冻结源码后采集最终性能和长跑。

```powershell
python tools/verify_evolution_coverage.py --manifest docs/plans/engine-evolution-manifest.json --evidence docs/acceptance/p3
ctest --test-dir build/ninja-msvc-debug --output-on-failure
ctest --test-dir build/ninja-msvc-release --output-on-failure
cmake -DBUILD_DIR=build/ninja-msvc-release -DCONFIG=Release -P tools/run_release_gate.cmake
```

性能与长跑沿用现有脚本的实际参数。执行前核对帮助、包路径和隔离环境。记录真实耗时和切换次数。采样失败完成原因定位与同条件复测。

**P3 完成门禁：** 全部借鉴项与阶段闭环。两种独立工作流证明公共能力可复用。完整回归、设备、预算和交付证据通过。提交 `feat(p3): 完成引擎复用与独立交付验收`。
