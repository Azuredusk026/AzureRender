# G4 脚本与玩法实施计划

> 文档类型：开发计划
> 状态：Complete
> 更新日期：2026-10-04
> 适用范围：Windows 编辑器与独立 Player

## 目标与依赖

依赖 G3，阶段目标为 Lua/sol2、事件、反射绑定、错误隔离和受控重载。执行依据为 [开发总计划](azure-engine-plan.md)。

## 接口与文件

`src/runtime/ScriptRuntime.*` 为每个实体提供隔离环境。脚本调用动作、反射属性、角色移动、销毁和关卡请求。替换脚本先编译验证，成功后在固定步边界替换。运行异常禁用当前脚本并记录诊断。

Lua 使用 5.4.8，sol2 使用 3.5.0。候选初始化暂存反射字段和副作用，成功后提交。关卡重载依据展开数据与渲染资源指纹，脚本重载保持 World。

## 文件级任务

- [x] 写出阶段契约测试，确认缺失行为产生失败。
- [x] 实现公开接口、宿主集成和失败诊断。
- [x] 执行针对性测试、Debug 与 Release 回归。
- [x] 更新运行时说明、验收证据、安装文档和总计划。
- [x] 检查暂存内容并创建阶段提交。

## 验收矩阵

脚本驱动角色、触发事件和关卡切换。语法与运行错误隔离。失败重载保留当前代码。已删除实体句柄拒绝访问。

| 场景 | 输入与期望 | Windows 验证入口 |
| --- | --- | --- |
| 反射与错误隔离 | 有效字段更新。错误与死循环只禁用对应脚本 | `AzureEngine.ScriptRuntime` |
| 初始化事务 | 候选修改属性、请求切关后失败，当前状态保留 | `AzureEngine.ScriptRuntime` |
| 生命周期 | 延迟删除在脚本对象释放后安全执行。过期句柄拒绝访问 | `AzureEngine.ScriptRuntime` |
| 脚本热重载 | Lua 源变化保留场景版本和实体，新代码生效 | `AzureEngine.ScriptRuntime` |
| 玩法闭环 | 双关卡示例移动、触发传送、切关后继续运动 | `AzureEngine.ScriptPlayer` |
| 移动安装树 | 发布目录移动后运行公开玩法项目，资源卸载为零 | `tools/run_release_gate.cmake` |

阶段证据保存在 `docs/acceptance/g4/2026-10-04.md`。测试覆盖 Windows Debug 和 Release，阶段结束运行完整 CTest。最终 G4 执行发布门禁与完整回归，GPU 预算沿用 G0 的本机温控口径。

Debug 与 Release 各 49/49 通过，Release 发布门禁通过。黑洞电影档 GPU 中位数为 18.851835 ms。阶段提交标题为 `feat(g4): 完成Lua脚本与双关卡玩法闭环`。

## 成本与边界

每个功能先运行契约测试，完整套件在阶段完成时执行。渲染算法由 Render Core 管理，新增运行时功能通过场景快照提交。Android 为 Deferred，编辑器完整工作流按 U0 建设。

## 来源与风险

反射与资产代码由项目实现。Jolt 来源为 https://github.com/jrouwe/JoltPhysics，Lua 来源为 https://www.lua.org，sol2 来源为 https://github.com/ThePhD/sol2。依赖版本、许可证和实际参考文件在模块准入时记录。

跨模块重点检查资源标识、实体生命周期、失败事务和线程边界。主线程提交关卡与脚本替换，物理回调生成事件后由运行时消费。

Lua 和 sol2 使用 MIT，通过 vcpkg 固定基线与 Lua 版本覆盖安装。接口适配与 MSVC 局部警告处理见 [脚本与玩法](../runtime/scripts-gameplay.md)。脚本使用本地可信项目，Lua 指令预算与源文件限制进入契约。
