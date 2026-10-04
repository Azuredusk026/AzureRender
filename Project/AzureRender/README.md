# AzureRender

AzureRender 是一个用 C++17 和原生 Vulkan API 编写的实时渲染器。风格化角色和黑洞模拟共用同一个宿主。项目还提供 ImGui 编辑器、确定性截图、诊断视图、GPU 计时和视觉回归工具。新场景可以通过进程内 Renderer SDK 接入。

游戏项目支持反射组件、关卡与 Prefab、Jolt 物理、Lua、动画、声音和 RmlUi。公开模板贯通创建、编辑、运行和 Windows 独立游戏发布。

第三人称项目提供独立动画实例、真实动作导入、速度驱动和跟随相机。公共制作入口与主角验收流程见[独立动画与第三人称控制](docs/runtime/third-person-animation.md)。

角色材质验收提供七视角、眉毛遮罩、主光扫描及公共混合场景。派生素材、固定输入和标注流程见[角色材质定稿与验收](docs/runtime/character-finish.md)。

> **English summary:** AzureRender is a real-time renderer written with C++17 and the native Vulkan API. One host runs both the stylized character renderer and the relativistic black-hole simulation. It also includes deterministic capture, diagnostics, GPU timing and an in-process renderer interface.

## 项目定位

项目直接实现 Vulkan 核心。这里包括设备、交换链、Render Pass、Pipeline、Descriptor 和 Buffer/Image，也包括资源上传、帧同步、GPU Query 和资源生命周期。

GLFW 与 Dear ImGui 负责窗口和界面。tinygltf、stb 和 nlohmann/json 负责解析模型、图片和 JSON。它们不接管渲染架构或 Vulkan 资源管理。

| 场景 | 主要技术 |
| --- | --- |
| Character | glTF 蒙皮动画、分类 Toon Ramp、Face SDF、Hair HN/P、双层 Kajiya-Kay、PCSS、AO、几何与屏幕空间描边、HDR 环境光 |
| Blackhole | Schwarzschild 近似光线积分、吸积盘体密度、周期噪声、多普勒与相对论增亮、引力红移、双 History TAA、HDR 合成 |
| Shared host | Vulkan 1.3、HDR Scene Color、Depth/Normal、Swapchain 合成、GPU Timing、Capture、HUD、编辑器与 Renderer Registry |

## 快速开始

项目需要 Windows 10/11 或 Ubuntu 24.04，并需要 Vulkan SDK、CMake 3.20+、Ninja 和 vcpkg。Windows 当前验收使用 MSVC 14.44、Vulkan SDK 1.4.350 和 Ninja。

```powershell
# 将该路径替换为本机 vcpkg 工作目录
$env:VCPKG_ROOT = (Resolve-Path "<vcpkg 目录>").Path

.\tools\msvc_env.bat cmake --preset msvc-debug
.\tools\msvc_env.bat cmake --build --preset msvc-debug
.\build\ninja-msvc-debug\AzureRender.exe --smoke-frames 120
```

运行黑洞：

```powershell
.\build\ninja-msvc-release\AzureRender.exe `
  --scene-type blackhole `
  --blackhole-quality cinematic
```

构建安装树后，请运行 `build/install-<config>/bin/AzureRender.exe`。不要单独复制 EXE。这样做会漏掉 GLFW 或 MinGW Runtime DLL。

## 文档

完整文档发布在 [AzureRender GitHub Pages](https://azuredusk026.github.io/AzureRender/)，仓库内也可直接阅读：

- [构建与使用](docs/getting-started.md)
- [渲染器架构与 Vulkan 实现](docs/architecture.md)
- [风格化角色渲染](docs/character-rendering.md)
- [黑洞模拟](docs/blackhole-rendering.md)
- [资产、场景与编辑器](docs/assets-and-editor.md)
- [项目编辑、运行控制与游戏界面](docs/runtime/editor-game-ui.md)
- [游戏模板、编辑器构建与 Windows 发布](docs/runtime/game-publishing.md)
- [可玩关卡性能与加载](docs/runtime/playable-performance.md)
- [开发、测试与发布](docs/development-and-release.md)
- [参数与接口参考](docs/reference.md)

## 验证

```powershell
.\tools\msvc_env.bat cmake --preset msvc-release
.\tools\msvc_env.bat cmake --build --preset msvc-release
ctest --test-dir .\build\ninja-msvc-debug --output-on-failure
cmake -DBUILD_DIR="$PWD/build/ninja-msvc-release" `
  -DCONFIG=Release `
  -P .\tools\run_release_gate.cmake
```

自动化测试覆盖 CLI、ECS、编辑器历史和场景序列化。它也检查资源定位、扩展注册和 GPU 能力报告。视觉变更还要运行公共资产捕获、图像比较和 Debug Validation。单元测试不能代替真实 GPU 验收。

## 资产与许可

- `assets_public/` 可用于 CI、发布和公开截图。
- `assets_private/` 仅供本机授权范围内的视觉检查，不进入公开仓库、安装包或作品集。
- `portfolio/` 只保存经过选择的公共视觉证据与机器可读 manifest。
- 第三方许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

项目当前版本为 `0.1.0-rc1`。功能状态和兼容性以源码、Schema、测试及 [CHANGELOG.md](CHANGELOG.md) 为准。

- [Azure Engine 开发总计划](docs/plans/azure-engine-plan.md)
- [第三人称角色与可玩关卡计划](docs/plans/third-person-playable-plan.md)
- [RHI 同步与窗口生命周期](docs/runtime/rhi-synchronization.md)
- [桌面同步验收记录](docs/acceptance/r1/2026-09-26.md)

R3 具备只读实例快照、并行录制和 GPU 间接提交。阶段为 Complete，本机验收与现行性能门禁见 [R3 阶段验收](docs/acceptance/r3/2026-10-03.md)。

Windows 渲染核心的压力场景、质量档位、长跑与窗口恢复结果见 [R4 阶段验收](docs/acceptance/r4/2026-10-03.md)。复现命令见 [Windows 验收说明](docs/runtime/windows-acceptance.md)。

黑洞电影档的 20 ms 预算、图像一致性及引擎开销核验见 [黑洞优化验收](docs/acceptance/blackhole/2026-10-03.md)。复现命令见 [黑洞性能预算](docs/runtime/blackhole-performance.md)。

## 引擎基础与 Player

独立 AzurePlayer 支持创建和加载版本化项目，资源通过虚拟挂载访问。使用与模块契约见 [引擎基础](docs/runtime/engine-foundation.md)，实施任务见 [G0 计划](docs/plans/g0-implementation.md)。

反射生成器、组件存档与属性元数据见 [反射与序列化](docs/runtime/reflection.md)。

UUID 资产、JSON 关卡、Prefab 覆盖与热重载见 [资产与关卡](docs/runtime/assets-levels.md)。

Jolt 物理、固定步长与动作输入见 [物理与输入](docs/runtime/physics-input.md)。

Lua 角色控制、反射访问、触发器和关卡切换见 [脚本与玩法](docs/runtime/scripts-gameplay.md)。公开双关卡项目位于 `assets_public/gameplay/`，复制到可写目录后用 Player 加载。

## 开发路线

Windows 主构建入口为 `msvc-debug` 与 `msvc-release`。源码和产物通过哈希记录关联，素材通过参数化报告准入，见[构建复现说明](docs/runtime/build-reproducibility.md)。

F1、R5、G5、G6、U1、F2、P1 全部完成。路线覆盖外观、真实动作、3C、交互和编辑器制作。构建复现、资产加载、性能和发布随阶段验收。交付结果见 [P1 验收](docs/acceptance/p1/2026-10-05.md)。

公开探索项目位于 `assets_public/exploration/`。玩法和本机主角项目入口见[第三人称探索关卡](docs/runtime/exploration-gameplay.md)。任务包含领取目标、三件收集物、开门、完成与重开。
