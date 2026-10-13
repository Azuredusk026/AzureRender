# AzureRender 文档

下一项任务为 ED3。
开发任务见[编辑器实施计划](plans/2026-10-14-editor-generalization.md)。
全部开发交付后执行 P8 集中验收。

阶段代号与状态见[统一状态表](plans/current-stage-status.md)。
下一步验收操作见[收尾流程](acceptance/phase-closure.md)。

当前工作区检查与待项见[交付结果](acceptance/ui8/2026-10-12.md)。

安装版项目制作见[项目中心指南](tutorials/project-center.md)。
入口、UI 与独立发布范围见[产品化状态](ui-productization-status.md)。
本轮证据和剩余限制见[交付验证](acceptance/ui4/2026-10-12.md)。

资源查找、日志筛选和任务处理见[资源与日志操作](tutorials/browser-console.md)。

AzureRender 提供 Windows 编辑器、原生 Vulkan 渲染核心与独立 Player。文档按了解现状、开始制作、参与开发和查询证据组织。

## 了解项目

先读[项目现状](project-status.md)，了解可用能力、限制与检查结果。再读[开发总计划](plans/azure-engine-plan.md)，查看下一批工作的顺序。自动执行和验收安排见[协作与验收](development-workflow.md)。

## 开始制作

| 任务 | 入口 |
| --- | --- |
| 构建和启动 | [构建与使用](getting-started.md) |
| 完成第一份游戏项目 | [从空关卡制作并发布](tutorials/editor-first-game.md) |
| 选择、变换和编辑内容 | [编辑器交互](tutorials/editor-interaction.md) |
| 制作材质 | [材质制作](runtime/material-authoring.md) |
| 接入二维内容 | [二维内容与投影标签](runtime/scene-2d.md) |
| 交换资产 | [资产交换](runtime/asset-exchange.md)、[扩展格式](assets/extended-formats.md) |
| 发布独立游戏 | [Windows 游戏发布](runtime/game-publishing.md) |

![编辑器工作区与公开探索关卡](media/editor-workspace.png)

图中为公开探索项目的编辑工作区。示例玩法见[探索关卡](runtime/exploration-gameplay.md)和[平台关卡](runtime/platformer-gameplay.md)。

## 参与开发

模块职责见[引擎基础](runtime/engine-foundation.md)和[渲染器架构](architecture.md)。生产操作见[编辑服务](runtime/edit-operations.md)，命令入口见[开发命令参考](reference/developer-cli.md)。具体后续任务见[后续开发路线](plans/2026-10-11-next-development.md)。

## 查询验收依据

最近的内容扩展与修复见[P9 验收](acceptance/p9/2026-10-12.md)。正式性能、长跑与人工待项见[P8 验收](acceptance/p8/2026-10-12.md)和[集中复核表](acceptance/p8/manual-review.md)。历史诊断按候选来源查询，[计划快照](history/2026-10-12-planning/azure-engine-plan.md)保留整理前的证据索引。

本批候选、检查结果与待项见[P8 执行记录](acceptance/p8/2026-10-13.md)。
