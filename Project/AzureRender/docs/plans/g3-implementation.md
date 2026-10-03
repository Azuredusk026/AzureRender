# G3 物理与输入实施计划

> 文档类型：开发计划
> 状态：Complete
> 更新日期：2026-10-04
> 适用范围：Windows 编辑器与独立 Player

## 目标与依赖

依赖 G2，阶段目标为Jolt 物理、固定步长、角色控制、查询、触发器与输入动作。执行依据为 [开发总计划](azure-engine-plan.md)。

## 接口与文件

`src/runtime/PhysicsWorld.*` 以独立 Jolt 世界持有刚体、角色与实体绑定。`InputActions.*` 统一键盘动作和焦点。`GameRuntime.*` 按 1/60 秒推进系统，帧边界管理实体删除和关卡替换。

## 接口约定

`PhysicsWorld::step()` 同步 World、推进物理并返回触发事件，`raycast()` 返回实体与射线比例。`InputActions` 提供动作绑定、按住与按下沿。`GameRuntime::advance()` 采用 1/60 秒步长，单帧累计上限为 0.25 秒。

物理对象以节点身份和关卡版本管理。Jolt 5.6.0 使用单线程任务执行器，相关许可证安装到发布树。

## 文件级任务

- [x] 写出阶段契约测试，确认缺失行为产生失败。
- [x] 实现公开接口、宿主集成和失败诊断。
- [x] 执行针对性测试、Debug 与 Release 回归。
- [x] 更新运行时说明、验收证据、安装文档和总计划。
- [x] 检查暂存内容并创建阶段提交。

## 验收矩阵

角色落地与移动；射线查询；触发器进入退出；删除清理与编号复用；暂停、单步和固定步长分帧一致性。

阶段证据保存在 `docs/acceptance/g3/2026-10-04.md`。测试覆盖 Windows Debug 和 Release，阶段结束运行完整 CTest。最终 G4 执行发布门禁与完整回归，GPU 预算沿用 G0 的本机温控口径。

## 成本与边界

每个功能先运行契约测试，完整套件在阶段完成时执行。渲染算法由 Render Core 管理，新增运行时功能通过场景快照提交。Android 为 Deferred，编辑器完整工作流按 U0 建设。

## 来源与风险

反射与资产代码由项目实现。Jolt 来源为 https://github.com/jrouwe/JoltPhysics，Lua 来源为 https://www.lua.org，sol2 来源为 https://github.com/ThePhD/sol2。依赖版本、许可证和实际参考文件在模块准入时记录。

跨模块重点检查资源标识、实体生命周期、失败事务和线程边界。主线程提交关卡与脚本替换，物理回调生成事件后由运行时消费。

## 完成记录

Debug 与 Release 各 47 项测试通过。Jolt 物理与输入接入 Player，记录见 [G3 验收](../acceptance/g3/2026-10-04.md)。
