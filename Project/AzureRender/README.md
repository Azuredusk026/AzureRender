# AzureRender

AzureRender 是一个用 C++17 和原生 Vulkan API 编写的实时渲染器。风格化角色和黑洞模拟共用同一个宿主。项目还提供 ImGui 编辑器、确定性截图、诊断视图、GPU 计时和视觉回归工具。新场景可以通过进程内 Renderer SDK 接入。

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

项目需要 Windows 10/11 或 Ubuntu 24.04，并需要 Vulkan SDK、CMake 3.20+、Ninja 和 vcpkg。Windows 验证环境使用 Vulkan SDK 1.4、MinGW GCC 13 和 Ninja 1.13。

```powershell
# 将该路径替换为本机 vcpkg 工作目录
$env:VCPKG_ROOT = (Resolve-Path "<vcpkg 目录>").Path

.\tools\configure_windows.ps1 -Config Debug
cmake --build .\build\ninja-debug
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
```

运行黑洞：

```powershell
.\build\ninja-release\AzureRender.exe `
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
- [开发、测试与发布](docs/development-and-release.md)
- [参数与接口参考](docs/reference.md)

## 验证

```powershell
.\tools\configure_windows.ps1 -Config Release
cmake --build .\build\ninja-release
ctest --test-dir .\build\ninja-debug --output-on-failure
cmake -DBUILD_DIR="$PWD/build/ninja-release" `
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
- [RHI 同步与窗口生命周期](docs/runtime/rhi-synchronization.md)
- [桌面同步验收记录](docs/acceptance/r1/2026-09-26.md)
