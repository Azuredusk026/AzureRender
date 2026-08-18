# AzureRender 文档导航

这份索引是所有维护者的阅读入口。活动文档控制在少量、职责互斥的文件中；历史资料统一归档。

## 首次接手

按顺序阅读：

1. [项目概览](PROJECT_OVERVIEW_CN.md)
2. [当前开发计划](ACTIVE_DEVELOPMENT_PLAN_CN.md)
3. [开发指南](DEVELOPMENT_GUIDE_CN.md)
4. 与当前任务相关的专题规范

通常不需要阅读 `archive/`。

## 活动文档

| 文档 | 唯一职责 | 何时更新 |
|---|---|---|
| [项目概览](PROJECT_OVERVIEW_CN.md) | 当前事实、架构、能力、限制 | 架构或能力发生变化 |
| [当前开发计划](ACTIVE_DEVELOPMENT_PLAN_CN.md) | 唯一任务队列、范围、验收 | 任务开始、完成或改序 |
| [开发指南](DEVELOPMENT_GUIDE_CN.md) | 环境、构建、测试、工作流 | 工具链或门禁变化 |
| [架构规范](ARCHITECTURE_CN.md) | 模块所有权、场景渲染器契约 | 公共边界变化 |
| [资产与视觉 QA](ASSET_AND_VISUAL_QA_CN.md) | 资产边界、导入与视觉验收 | 资产契约或 QA 流程变化 |
| [发布与验收](RELEASE_AND_ACCEPTANCE_CN.md) | RC 打包、证据和发布规则 | 发布流程变化 |
| [应用快速使用](../README.md) | 用户运行命令和控制 | CLI/控制变化 |

## 事实来源优先级

发生冲突时按以下顺序处理：

1. 源码、schema、CMake 和自动化测试。
2. `ACTIVE_DEVELOPMENT_PLAN_CN.md` 中的当前任务状态。
3. `PROJECT_OVERVIEW_CN.md` 中的当前工程事实。
4. 其他活动专题文档。
5. `archive/`、`.docx` 和 Git 历史。

归档文档中的计划、状态、路径、测试数量和“下一步”均不具备执行效力。

## 归档

```text
archive/history/     长开发日志和资产接入流水
archive/milestones/  已完成阶段的设计、审计与验收快照
archive/legacy/      被新文档替代的交接、总结、路线和环境说明
```

归档文件原则上不再编辑。需要纠正当前事实时修改活动文档；需要追溯历史时使用 Git。

提案和早期规划 `.docx` 位于 `archive/source-documents/`。它们是原始参考材料，不是当前开发计划；旧版计划生成脚本也只向该目录输出。

## 目录内说明

以下短文档只解释所在目录，不参与项目状态管理：

- `assets_public/README.md`：公共测试资产来源和用途。
- `assets_private/README.md`：受限资产边界。
- `assets_placeholder/README.md`：占位资产准入规则。
- `portfolio/README_CN.md`：作品集展示与重新生成。
- `THIRD_PARTY_NOTICES.md`：第三方依赖许可入口。

## 文档维护规则

- 不在专题文档重复当前任务状态，只链接到开发计划。
- 不把逐提交流水追加到活动文档；提交本身和归档日志负责历史追溯。
- 命令必须可直接执行，路径统一相对 `Project/AzureRender`。
- 数量、版本和哈希必须来自本次验证，不复制旧快照。
- 新增文档前先确认不能加入现有七份活动文档。
- 每次任务完成只需更新：当前计划、项目概览，以及真正受影响的专题文档。
