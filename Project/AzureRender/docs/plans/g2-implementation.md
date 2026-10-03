# G2 资产、关卡与 Prefab实施计划

> 文档类型：开发计划
> 状态：Complete
> 更新日期：2026-10-04
> 适用范围：Windows 编辑器与独立 Player

## 目标与依赖

依赖 G1，阶段目标为UUID 资产数据库、版本化缓存、JSON 关卡、Prefab 覆盖和热重载。执行依据为 [开发总计划](azure-engine-plan.md)。

## 接口与文件

`src/runtime/AssetDatabase.*` 管理侧车标识、内容指纹和依赖。`Level.*` 保存反射组件包与资源引用。`Prefab.*` 合并节点级覆盖并检测循环。Player 加载关卡，帧边界提交成功的替换。

## 接口约定

`AssetDatabase::refresh()` 返回变化 UUID，`readSource()` 读取有效缓存字节。`Level::load()` 和 `save()` 持有 JSON 存档，`setComponent()` 编辑原始节点或实例覆盖。`LevelSession::request()` 排队切换，`poll()` 在帧边界提交。`setPrepareHandler()` 准备 GPU 候选资源。

缓存按 UUID 和指纹分版本存放，目录资源包包含版本化清单与原目录结构。源文件和 UUID 侧车一同移动。

## 文件级任务

- [x] 写出阶段契约测试，确认缺失行为产生失败。
- [x] 实现公开接口、宿主集成和失败诊断。
- [x] 执行针对性测试、Debug 与 Release 回归。
- [x] 更新运行时说明、验收证据、安装文档和总计划。
- [x] 检查暂存内容并创建阶段提交。

## 验收矩阵

资产移动与依赖失效；缓存损坏；关卡切换失败保留当前场景；Prefab 覆盖往返和循环；Player 实际启动。

阶段证据保存在 `docs/acceptance/g2/2026-10-04.md`。测试覆盖 Windows Debug 和 Release，阶段结束运行完整 CTest。最终 G4 执行发布门禁与完整回归，GPU 预算沿用 G0 的本机温控口径。

## 成本与边界

每个功能先运行契约测试，完整套件在阶段完成时执行。渲染算法由 Render Core 管理，新增运行时功能通过场景快照提交。Android 为 Deferred，编辑器完整工作流按 U0 建设。

## 来源与风险

反射与资产代码由项目实现。Jolt 来源为 https://github.com/jrouwe/JoltPhysics，Lua 来源为 https://www.lua.org，sol2 来源为 https://github.com/ThePhD/sol2。依赖版本、许可证和实际参考文件在模块准入时记录。

跨模块重点检查资源标识、实体生命周期、失败事务和线程边界。主线程提交关卡与脚本替换，物理回调生成事件后由运行时消费。

## 完成记录

Debug 与 Release 各 45 项测试通过，Vulkan Player 的关卡与热重载通过。记录见 [G2 验收](../acceptance/g2/2026-10-04.md)。
