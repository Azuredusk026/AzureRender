# AzureRender

AzureRender 提供原生 Vulkan 渲染核心和第三人称游戏引擎。Windows 编辑器支持关卡制作、动画、物理、Lua 与游戏构建。独立 Player 运行公开探索关卡，完成任务、切关与重开。

![公开黑洞场景](Project/AzureRender/portfolio/images/blackhole/blackhole_temporal_beauty_v1_1280x720.png)

展示图使用公开资产，捕获来源见 [展示清单](Project/AzureRender/portfolio/portfolio_manifest.json)。

## 从这里开始

| 目标 | 入口 |
| --- | --- |
| 运行公开演示 | [第三人称探索关卡](Project/AzureRender/docs/runtime/exploration-gameplay.md) |
| 构建编辑器与 Player | [构建与使用](Project/AzureRender/docs/getting-started.md) |
| 学习关卡制作 | [编辑器操作指南](Project/AzureRender/docs/runtime/editor-game-ui.md) |
| 构建独立游戏包 | [Windows 游戏发布](Project/AzureRender/docs/runtime/game-publishing.md) |

## 当前能力

| 模块 | 已实现能力 |
| --- | --- |
| 渲染 | glTF 材质、独立蒙皮、光照、级联阴影、透明、描边与黑洞场景 |
| 运行时 | ECS、反射、UUID 资产、关卡、Prefab、Jolt、Lua、声音与 RmlUi |
| 编辑器 | 层级、属性、导入、撤销、预览、运行调试、保存与构建 |
| 发布 | 可移动 Windows x64 Player、依赖、许可与 SHA-256 清单 |

## 构建与运行

在 Windows 安装 MSVC、CMake、Ninja、Vulkan SDK 与 vcpkg。设置工具链路径后，在工程目录执行以下命令。

```powershell
cd Project/AzureRender
$env:VULKAN_SDK = "C:/VulkanSDK/1.4.350.0"
$env:VCPKG_ROOT = "C:/path/to/vcpkg"
./tools/msvc_env.bat cmake --preset msvc-release
./tools/msvc_env.bat cmake --build --preset msvc-release
./build/ninja-msvc-release/AzureRender.exe --editor-project assets_public/exploration/project.azureproject
```

将 vcpkg 路径替换为本机安装目录。制作项目时将公开示例复制到可写工作目录。完整参数和环境要求见 [构建与使用](Project/AzureRender/docs/getting-started.md)。

## 验证范围

| 环境 | 已验证范围 |
| --- | --- |
| Windows x64、RTX 4060 Laptop | 1080p 标准负载、完整任务、独立发布与长跑 |
| Windows x64、Intel 核显 | 540p 完整任务、两次重开与退出释放 |
| Ubuntu 24.04 CI | 构建、公共软件渲染回归与文档检查 |

本机交付证据见 [P1 验收](Project/AzureRender/docs/acceptance/p1/2026-10-05.md)。设备与质量配置共同定义兼容范围。

## 仓库结构

```text
Project/AzureRender/src/          引擎、编辑器与 Player
Project/AzureRender/shaders/      Vulkan 着色器
Project/AzureRender/assets_public/ 公共模型、示例和许可
Project/AzureRender/tests/        契约与回归测试
Project/AzureRender/tools/        构建、导入、审计与验收工具
Project/AzureRender/docs/         文档站与阶段证据
Project/AzureRender/portfolio/    精选公共展示及来源清单
.github/workflows/               构建与文档工作流
```

## 开发路线与文档

F1 至 P1 的可玩关卡路线已完成。F3 仓库与文档基础已完成。当前执行 R6 可见性，后续为 G7 方向与冲刺、U2 工作区、P2 最终交付。

[开发总计划](Project/AzureRender/docs/plans/azure-engine-plan.md)管理阶段状态。[优化实施计划](Project/AzureRender/docs/plans/2026-10-05-quality-round.md)列出任务和验收。[文档站](https://azuredusk026.github.io/AzureRender/)提供主题说明。

## 资产与许可

公开资产、示例和展示媒体使用其随附许可。私有主角及派生素材按本机授权范围保存。第三方许可见 [THIRD_PARTY_NOTICES.md](Project/AzureRender/THIRD_PARTY_NOTICES.md)。
