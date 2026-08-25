# AzureRender

AzureRender 是一个基于原生 Vulkan API 和 C++17 构建的可扩展实时渲染器。统一宿主目前承载两条差异显著的渲染路径：风格化角色渲染，以及带相对论视觉效应的黑洞模拟。项目同时提供 ImGui 编辑器、确定性截图、诊断视图、GPU Pass Timing、视觉回归工具和进程内 Renderer SDK。

> **English summary:** AzureRender is an extensible real-time renderer built directly on Vulkan. It combines a stylized character pipeline and a relativistic black-hole renderer under one host, with deterministic capture, diagnostics, GPU timing and an in-process scene-renderer interface.

## 项目定位

Vulkan 核心由项目直接实现，包括设备与交换链、Render Pass、Pipeline、Descriptor、Buffer/Image、资源上传、帧同步、GPU Query 和资源生命周期。GLFW、Dear ImGui、tinygltf、stb 和 nlohmann/json 分别承担窗口界面、模型图片解析和 JSON 支持；它们不替代渲染架构或 Vulkan 资源管理。

| 场景 | 主要技术 |
| --- | --- |
| Character | glTF 蒙皮动画、分类 Toon Ramp、Face SDF、Hair HN/P、双层 Kajiya-Kay、PCSS、AO、几何与屏幕空间描边、HDR 环境光 |
| Blackhole | Schwarzschild 近似光线积分、吸积盘体密度、周期噪声、多普勒与相对论增亮、引力红移、双 History TAA、HDR 合成 |
| Shared host | Vulkan 1.3、HDR Scene Color、Depth/Normal、Swapchain 合成、GPU Timing、Capture、HUD、编辑器与 Renderer Registry |

## 快速开始

要求 Windows 10/11 或 Ubuntu 24.04、Vulkan SDK、CMake 3.20+、Ninja 和 vcpkg。Windows 已验证 Vulkan SDK 1.4.350.0、MinGW GCC 13.1 与 Ninja 1.13.2。

```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

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

构建安装树后应从 `build/install-<config>/bin/AzureRender.exe` 运行；不要只复制 EXE，否则 Windows 会缺少 GLFW 或 MinGW Runtime DLL。

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
ctest --test-dir .\build\ninja-debug --output-on-failure
cmake -DBUILD_DIR="$PWD/build/ninja-debug" `
  -DCONFIG=Debug `
  -P .\tools\run_release_gate.cmake
```

自动化覆盖 CLI、ECS、编辑器历史、场景序列化、资源定位、扩展注册和 GPU 能力报告。视觉变更还必须通过公共资产捕获、图像比较和 Debug Validation；单元测试不能替代 GPU 实机验收。

## 资产与许可

- `assets_public/` 可用于 CI、发布和公开截图。
- `assets_private/` 仅供本机授权范围内的视觉检查，不进入公开仓库、安装包或作品集。
- `portfolio/` 只保存经过选择的公共视觉证据与机器可读 manifest。
- 第三方许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

项目当前版本为 `0.1.0-rc1`。功能状态和兼容性以源码、Schema、测试及 [CHANGELOG.md](CHANGELOG.md) 为准。
