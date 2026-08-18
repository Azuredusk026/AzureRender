# AzureRender 文档导航

活动文档以交接和发布为目标。历史阶段、审计和原始 DOCX 统一留在 `archive/`，不参与当前决策。

## 三个主入口

1. [当前架构](ARCHITECTURE_CN.md)：系统边界、目录、数据流、场景插件和已知限制。
2. [使用手册](USER_GUIDE_CN.md)：运行场景、控制、编辑器、截图、QA 和排错。
3. [未来开发路线](DEVELOPMENT_ROADMAP_CN.md)：下一优先级、候选能力、Deferred 和阶段门禁。

新维护者应按以上顺序阅读，不需要先阅读 `archive/`。

## 辅助规范

| 文档 | 职责 |
|---|---|
| [开发指南](DEVELOPMENT_GUIDE_CN.md) | 工具链、构建、测试和贡献循环 |
| [Renderer SDK](RENDERER_SDK_CN.md) | 新场景接入、生命周期、所有权和 catalog 契约 |
| [Blackhole 质量](BLACKHOLE_QUALITY_CN.md) | 质量档位、相机、history reset、图像回归和 timing |
| [资产与视觉 QA](ASSET_AND_VISUAL_QA_CN.md) | 资产许可、截图命名和视觉基准 |
| [Character Look 与美术验收](CHARACTER_LOOKS_CN.md) | 角色外观数据、编辑器控制和授权边界 |
| [发布与验收](RELEASE_AND_ACCEPTANCE_CN.md) | 安装包内容、发布门禁和证据 |
| [应用 README](../README.md) | 最短构建和运行入口 |
| [公开视觉证据](../portfolio/README_CN.md) | 精选截图、manifest 和哈希校验 |

## 事实来源

发生冲突时按以下顺序处理：

1. 源码、schema、CMake 和自动化测试。
2. `ARCHITECTURE_CN.md` 的当前系统事实。
3. `DEVELOPMENT_ROADMAP_CN.md` 的未来优先级与状态。
4. 其他活动规范。
5. `archive/` 与 Git 历史。

## 维护规则

- 当前能力只写入架构文档，不在路线中重复。
- 未实现内容只写入路线，不在 README 中宣传。
- 用户命令变更同步更新使用手册和应用 README。
- 公共接口变更更新架构；视觉契约变更更新 QA；包内容变更更新发布规范。
- 新截图先进入忽略的 `captures/`，验收后按语义命名精选到 `portfolio/`。
- 不恢复逐提交开发日志；完成阶段移入 `archive/milestones/`。
