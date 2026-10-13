# AzureRender 工程

下一项任务为 ED6。
开发任务见[编辑器实施计划](docs/plans/2026-10-14-editor-generalization.md)。
全部开发交付后执行 P8 集中验收。

阶段状态见[当前状态](docs/plans/current-stage-status.md)。
阶段收尾与验收顺序见[执行说明](docs/acceptance/phase-closure.md)。

工作区迁移与本轮检查见[交付结果](docs/acceptance/ui8/2026-10-12.md)。
Debug 与 Release 各 187 项 CPU 回归通过。

相机、变换与场景视口见[视口指南](docs/tutorials/viewport-workspace.md)。

对象选择与属性操作见[编辑指南](docs/tutorials/selection-properties.md)。

资源查找与日志操作见[面板指南](docs/tutorials/browser-console.md)。
资源与日志迁移结果见[阶段验证](docs/acceptance/ui5/2026-10-12.md)。

安装版入口、UI 与发布能力见[产品化状态](docs/ui-productization-status.md)。
自动工作流与剩余复核见[交付验证](docs/acceptance/ui4/2026-10-12.md)。

AzureRender 是使用 C++17 与原生 Vulkan 的 Windows 引擎。编辑器、运行时和独立 Player 提供内容制作、游戏运行与发布能力。公开探索项目用于验证这些公共能力。

## 从哪里开始

| 想了解什么 | 阅读入口 |
| --- | --- |
| 现在完成了什么，还有什么待验 | [项目现状](docs/project-status.md) |
| 接下来按什么顺序开发 | [开发总计划](docs/plans/azure-engine-plan.md) |
| 自动执行怎样推进，检查怎样安排 | [协作与验收](docs/development-workflow.md) |
| 构建并运行项目 | [构建与使用](docs/getting-started.md) |
| 从空项目制作并发布游戏 | [制作教程](docs/tutorials/editor-first-game.md) |
| 从安装版 GUI 创建、重开与发布项目 | [项目中心使用指南](docs/tutorials/project-center.md) |

## 当前可用能力

双击安装目录中的 `bin/AzureRender.exe` 进入项目中心。
新建或打开工程后进入编辑器，项目目录独立保存。
GUI 发布产生项目命名的 exe 与完整游戏目录。
启动和 UI 分层见[应用架构](docs/architecture-ui-productization.md)。

编辑器支持场景层级、属性编辑、选择变换、内容导入、撤销重做和运行预览。运行时提供 ECS、物理、脚本、动画、声音及游戏界面。独立 Player 按项目数据运行，并通过 Windows 游戏包交付。

最近一轮实现了类型化材质画布、像素坐标二维文档及 GIF、PSD、SVG、ASCII USD 导入。格式注册覆盖 29 种输入、20 种输出，各格式有明确支持边界。二维内容和材质图的范围见[项目现状](docs/project-status.md)。

Debug 与 Release 各 186 项 CPU 全回归通过，最后的路径修复各四项相关检查通过。可见回放、二维移动包和生命周期短检已有证据。正式热状态、原始长跑竞态和人工体验仍待集中复核。

| 制作任务 | 操作说明 |
| --- | --- |
| 编辑三维关卡 | [编辑器交互](docs/tutorials/editor-interaction.md) |
| 使用材质画布 | [材质制作](docs/runtime/material-authoring.md) |
| 接入二维内容 | [二维内容与投影标签](docs/runtime/scene-2d.md) |
| 导入与导出资产 | [资产交换](docs/runtime/asset-exchange.md)、[扩展格式范围](docs/assets/extended-formats.md) |
| 发布独立游戏 | [Windows 游戏发布](docs/runtime/game-publishing.md) |

![编辑器工作区与公开探索关卡](docs/media/editor-workspace.png)

图中为公开探索项目。图片用于展示编辑工作区，当前检查范围以[集成验收](docs/acceptance/p9/2026-10-12.md)为准。

## 构建编辑器

Windows 工具链为 MSVC、CMake、Ninja、Vulkan SDK 和 vcpkg。本机验收使用 MSVC 14.44 与 Vulkan SDK 1.4.350。

```powershell
$env:VULKAN_SDK = "C:/VulkanSDK/1.4.350.0"
$env:VCPKG_ROOT = "C:/path/to/vcpkg"
./tools/msvc_env.bat cmake --preset msvc-release
./tools/msvc_env.bat cmake --build --preset msvc-release
./build/ninja-msvc-release/AzureRender.exe --editor-project assets_public/exploration/project.azureproject
```

将 vcpkg 路径替换为本机目录。公开示例制作流程使用可写工作副本。安装与发布以完整目录为单位，操作见 [游戏发布](docs/runtime/game-publishing.md)。

## 工程与证据

`src/` 保存模块实现，`shaders/` 保存着色器，`tests/` 保存契约测试。`tools/` 保存构建与验收工具，`docs/` 保存使用说明、计划和证据。架构入口为[引擎基础](docs/runtime/engine-foundation.md)和[渲染器架构](docs/architecture.md)。

最近一轮证据见[P9 集成验收](docs/acceptance/p9/2026-10-12.md)。延期条件见[P8 清单](docs/plans/p8-deferred-acceptance-manifest.json)。阶段交付记录见[变更记录](CHANGELOG.md)，整理前的索引见[历史快照](docs/history/2026-10-12-planning/readme-snapshot.md)。

## 资产与许可

`assets_public/` 包含公共示例、测试资产和许可。`assets_private/` 保存本机授权素材。公开包与截图使用许可明确的资源，依赖许可见[第三方声明](THIRD_PARTY_NOTICES.md)。

本批最终候选与收尾进度见[P8 执行记录](docs/acceptance/p8/2026-10-13.md)。
