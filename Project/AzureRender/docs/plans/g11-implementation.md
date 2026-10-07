# G11 脚本服务实施步骤

> 文档类型：阶段实施计划
> 状态：验收通过
> 日期：2026-10-07
> 执行方式：按已授权的自主实施模式连续验收

## 目标与边界

阶段依据为[专项实施步骤](2026-10-06-engine-specialized-steps.md)。
脚本后端消费同一个服务与宿主操作契约。
Lua 玩法、编辑预览与独立发布保持兼容。
托管后端交付真实生命周期原型与采用结论。

运行时核心提供脚本服务接口。
公共绑定宿主负责对象守卫和组件事务。
后端适配负责语言值、调用和模块加载。
应用装配负责注册与项目选择。

技术栈包括 C++17、sol2 与 .NET 9。
CoreCLR 使用 hostfxr 加载真实托管入口。
NativeAOT 使用实际发布的原生共享库。
文档与默认值按实现范围描述。

## 验证重点

实体删除和同名对象重建使旧句柄失效。
世界交换与场景切换保持对象身份语义。
初始化失败保持有效组件与脚本。
关闭会话使保存的宿主回调令牌失效。

生成检查拒绝版本、类型与输出漂移。
只读节点标识通过真实绑定执行校验。
字符串和数组具有长度、深度与所有权规则。
异常在语言边界转换为有界诊断。

CoreCLR 的可回收上下文提供卸载证据。
NativeAOT 的重载能力由实际能力描述决定。
托管模块关闭时，Lua 项目仍能构建和运行。
独立 Player 与预览消费相同装配入口。

## 步骤一：对象与公共绑定宿主

文件为 `src/ecs/World.hpp`。
公共宿主位于 `src/scripting/ScriptBindingHost.hpp`。
实现位于同目录的 `ScriptBindingHost.cpp`。
测试位于 `tests/ScriptBackendContractTests.cpp`。

输出包括 `ecs::EntityHandle` 与身份查询。
`ScriptObject` 保存实体、节点与场景版本。
`ScriptBindingHost::invoke` 校验并执行操作。
初始化事务管理属性与延迟效果。

- [x] 添加编号复用、清空与世界交换用例。
- [x] 添加回调权限、参数和跨实体事务用例。
- [x] 运行用例，确认缺失契约导致失败。
- [x] 实现公共宿主并验证失败恢复。

## 步骤二：唯一描述与 Lua 服务

服务接口为 `src/runtime/IScriptRuntime.hpp`。
描述为 `schemas/script_bindings.json`。
生成工具为 `tools/generate_script_bindings.py`。
Lua 实现归属于独立脚本构建目标。

服务声明初始化、固定步、事件与重载。
服务还声明关闭、诊断和宿主回调。
生成结果包含 C++ 描述与 C# 包装。
Lua 安装与托管调用使用同一描述。

- [x] 添加非法版本、漂移与只读标识用例。
- [x] 运行用例，确认真实拒绝路径。
- [x] 接入公共宿主与可替换服务。
- [x] 验证 Lua 任务、重载和 Play/Stop。

## 步骤三：真实托管后端

原生适配位于 `src/scripting/dotnet/`。
托管 API 位于 `managed/EngineBindings/`。
公开脚本夹具位于 `managed/SampleScripts/`。
构建工具维护模块与类型注册清单。

版本化 ABI 声明尺寸、字节长度和所有权。
原生回调通过可失效会话令牌访问公共宿主。
两种托管后端消费同一 API 和测试输入。
CoreCLR 载入程序集，NativeAOT 使用发布期注册。

- [x] 添加握手、中文字符串和数组用例。
- [x] 添加异常、初始化失败和关闭用例。
- [x] 运行真实入口，保存失败证据。
- [x] 实现并验证重载、卸载与资源释放。
- [x] 比较三后端结果及调用成本。

## 步骤四：宿主装配与交付

应用装配位于 `src/app/ProjectRuntimeAssembly.*`。
Player 和编辑预览持有 `IScriptRuntime`。
项目配置声明后端及其模块资源。
资产和发布工具维护清单依赖。

- [x] 验证探索控制器与独立标注工作流。
- [x] 验证可选模块关闭组合与既有 Player。
- [x] 记录构建、体积、启动和调用成本。
- [x] 完成两配置、设备、预算与移动包验收。
- [x] 同步设计、教程、清单和采用结论。
- [x] 核对暂存与证据后创建阶段提交。

专项命令使用正式注册的测试入口。

```powershell
python tools/generate_script_bindings.py --check
ctest --test-dir build/ninja-msvc-debug -C Debug -R '^AzureEngine.(ScriptBackendContract|ScriptRuntime|ScriptEntities|ModuleAssembly)$' --output-on-failure
```

完整回归和发布门禁均通过后进入 P3。
提交标题为 `feat(g11): 完成脚本服务与生成绑定原型验收`。
