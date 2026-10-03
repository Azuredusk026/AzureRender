# G1 反射与序列化实施计划

> 文档类型：开发计划
> 状态：Complete
> 更新日期：2026-10-03
> 适用范围：Windows 编辑器与独立 Player

## 目标与依赖

依赖 G0，阶段目标为受限 C++ 注解、稳定类型标识、属性元数据与 JSON 迁移。执行依据为 [开发总计划](azure-engine-plan.md)。

## 接口与文件

`tools/metagen/main.cpp` 读取 `AZURE_TYPE` 和 `AZURE_FIELD` 注解。生成文件只在内容变化时写入，CMake 显式声明输入依赖。`src/reflection/Registry.hpp` 持有属性读写、类型检查与迁移函数，生成器注册 ECS 和玩法组件。

## 文件级任务

- [x] 写出阶段契约测试，确认缺失行为产生失败。
- [x] 实现公开接口、宿主集成和失败诊断。
- [x] 执行针对性测试、Debug 与 Release 回归。
- [x] 更新运行时说明、验收证据、安装文档和总计划。
- [x] 检查暂存内容并创建阶段提交。

## 验收矩阵

代码生成、错误定位、稳定标识与增量时间戳；属性编辑、损坏数据、未来版本和迁移往返。

阶段证据保存在 `docs/acceptance/g1/2026-10-03.md`。测试覆盖 Windows Debug 和 Release，阶段结束运行完整 CTest。最终 G4 执行发布门禁与完整回归，GPU 预算沿用 G0 的本机温控口径。

## 成本与边界

每个功能先运行契约测试，完整套件在阶段完成时执行。渲染算法由 Render Core 管理，新增运行时功能通过场景快照提交。Android 为 Deferred，编辑器完整工作流按 U0 建设。

## 来源与风险

反射与资产代码由项目实现。Jolt 来源为 https://github.com/jrouwe/JoltPhysics，Lua 来源为 https://www.lua.org，sol2 来源为 https://github.com/ThePhD/sol2。依赖版本、许可证和实际参考文件在模块准入时记录。

跨模块重点检查资源标识、实体生命周期、失败事务和线程边界。主线程提交关卡与脚本替换，物理回调生成事件后由运行时消费。

## 完成记录

Debug 与 Release 各 43 项测试通过。生成、迁移和 Inspector 元数据已接入。记录见 [G1 验收](../acceptance/g1/2026-10-03.md)。
