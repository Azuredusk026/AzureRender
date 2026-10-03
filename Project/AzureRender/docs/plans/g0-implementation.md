# G0 引擎基础实施计划

> 文档类型：开发计划
> 状态：Complete
> 更新日期：2026-10-03
> 适用范围：Windows 游戏运行时、独立 Player 与现有渲染宿主

## 目标与依据

执行依据为 [开发总计划](azure-engine-plan.md)及 [黑洞准入验收](../acceptance/blackhole/2026-10-03.md)。G0 交付模块库、版本化项目文件、资源挂载、独立 Player、运行时生命周期与完整帧性能观测。G1 的反射和资产数据库分别按其阶段安排。

## 模块与接口

Foundation 持有诊断与资源定位。Platform 持有 GLFW 生命周期。Render Core 持有 RHI、渲染图、渲染设置、资产解析和场景渲染器。

Runtime 持有 SceneDocument、Project 与 RuntimeLifecycle。Editor 持有交互和界面。两个宿主构建目标分别服务 AzureRender 与 AzurePlayer，Player 的宿主编译排除编辑器实现和 Dear ImGui 链接。

项目文件为 `project.azureproject`，版本为 1。Project 提供创建、读取、虚拟路径解析和启动配置。项目挂载限定在项目目录，使用规范化路径检查，资源按 `挂载名:/相对路径` 访问。引擎内置资源通过 `engine:/` 访问，并由 ResourceLocator 按安装位置定位。

启动参数保留现行渲染验收选项。

RuntimeLifecycle 管理运行、暂停、单步与关闭状态，拥有 World。帧开始消费延迟操作并运行系统，渲染使用已有不可变快照。资源与任务依赖按当前 GPU 完成信号释放。

## 文件级任务

- [x] 项目与生命周期测试：新增 `tests/ProjectTests.cpp`、`tests/RuntimeLifecycleTests.cpp`，覆盖往返、版本错误、重复挂载、路径越界、暂停和单步。
- [x] 项目实现：新增 `src/runtime/Project.hpp`、`Project.cpp`、`RuntimeLifecycle.hpp`，提供版本化项目和虚拟资源入口。
- [x] 模块拆分：调整 `CMakeLists.txt`，将场景文档归入 Runtime，保持编辑器兼容入口，建立模块依赖和严格编译选项。
- [x] Player 启动：新增 `src/player/main.cpp`，支持创建项目、检查项目、运行项目和明确的 CLI 错误诊断。
- [x] 宿主生命周期：将 RuntimeLifecycle 接入普通帧，保留现行捕获与确定性模拟时序。只读节点与灯光描述进入逐帧渲染输入，NullRHI 验证变换、隐藏与删除。
- [x] 性能观测：分别记录帧槽、取图、提交、呈现和整帧 CPU 时间，补齐黑洞绘制计数。一至五个录制任务使用调用线程，较大批次通过工作池并行录制。
- [x] 发布验收：检查 Player 链接闭包、新建项目、项目迁移、隔离安装启动、三场景、固定描述符、窗口恢复和性能预算。
- [x] 文档与提交：同步总计划、README、模块说明、变更记录和证据清单，提交 `feat(g0): 完成引擎基础与独立Player`。

## 验收矩阵

| 场景 | 期望 | 验证入口 |
| --- | --- | --- |
| 项目往返与引用 | 标识稳定，项目资源与引擎资源正确解析 | ProjectTests 与 Player CLI |
| 损坏配置、未来版本、重复挂载、路径越界 | 加载失败并给出字段或路径诊断 | ProjectTests |
| 暂停、单步、延迟修改、关闭 | 系统执行次数与状态符合契约 | RuntimeLifecycleTests |
| 构建闭包 | Player 链接包含运行时模块，编辑器与 ImGui 属于 AzureRender | Ninja 链接命令与符号检查 |
| 渲染与生命周期 | 三场景和既有窗口回归通过，卸载分配为零 | 现有 CTest 与本机运行记录 |
| 正面黑洞预算 | Release、1280×720、电影档三轮均值中位数低于 20 ms | 黑洞性能工具 |
| 安装与发布 | 移动安装树后，Player 在外部目录创建和运行项目 | Release 门禁与安装清单 |

## 验证顺序与成本

契约测试先失败再实现。任务内运行针对性测试，阶段结束分别运行一次 Debug 与 Release 完整 CTest。

GPU 预算按三轮各 300 帧正式采样执行，各轮起始 GPU 温度至多 55°C，候选程序与参考程序交错运行并记录温度与频率。此起始条件依据本机空闲温度控制热积累，采样间安排冷却等待，20 ms 门禁保持生效。

结构迁移使用固定图像基线核验。失败项按实际原因修复后复验。测试设备跟随本机，保存设备和工具链元数据。

## 风险与来源

分库需要检查公共头文件的依赖方向及宿主编译变体。项目文件路径检查需覆盖规范化与符号链接。任务回调异常必须向调用线程传播。主线程拥有 World，工作线程读取帧快照。

源码依据为 `src/app/`、`src/ecs/`、`src/resources/`、`src/render/` 和 `src/rhi/`。本阶段采用现有实现和标准库，第三方许可证沿用项目发布清单。

## 完成记录

Debug 与 Release 各 41 项回归通过，项目、安装、图像、调度和 GPU 预算验收通过。阶段证据见 [G0 验收](../acceptance/g0/2026-10-03.md)。后续阶段为 G1。
