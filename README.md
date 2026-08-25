# AzureRender

AzureRender 是一个基于 C++17 和原生 Vulkan API 构建的可扩展实时渲染器。项目以统一宿主承载风格化角色渲染和相对论黑洞模拟，同时提供 ImGui 编辑器、确定性捕获、诊断视图、GPU Pass Timing、视觉回归工具和进程内 Renderer SDK。

> **English summary:** AzureRender is an extensible real-time renderer built directly on Vulkan. Its shared host supports a stylized character pipeline and a relativistic black-hole renderer, together with deterministic capture, diagnostics, GPU timing and an editor workflow.

![AzureRender black-hole scene](https://raw.githubusercontent.com/Azuredusk026/AzureRender/main/Project/AzureRender/portfolio/images/blackhole/blackhole_temporal_beauty_v1_1280x720.png)

## 文档

完整技术文档发布在 [AzureRender GitHub Pages](https://azuredusk026.github.io/AzureRender/)。仓库内入口：

1. [项目总览](Project/AzureRender/docs/index.md)
2. [构建与使用](Project/AzureRender/docs/getting-started.md)
3. [渲染器架构与 Vulkan 实现](Project/AzureRender/docs/architecture.md)
4. [风格化角色渲染](Project/AzureRender/docs/character-rendering.md)
5. [黑洞模拟](Project/AzureRender/docs/blackhole-rendering.md)
6. [资产、场景与编辑器](Project/AzureRender/docs/assets-and-editor.md)
7. [开发、测试与发布](Project/AzureRender/docs/development-and-release.md)
8. [参数与接口参考](Project/AzureRender/docs/reference.md)

历史计划、阶段验收和原始 DOCX 位于 `Project/AzureRender/docs/archive/`，不作为当前实现依据。

## 仓库结构

```text
Project/AzureRender/        主工程、Shader、测试、工具和活动文档
Project/AzureRender/docs/   GitHub Pages 文档源
Project/AzureRender/portfolio/
                            公共视觉证据和机器可读 Manifest
AfterglowRender/            早期参考代码，仅保留为历史输入
```

## 快速构建

```powershell
cd Project\AzureRender

$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

.\tools\configure_windows.ps1 -Config Debug
cmake --build .\build\ninja-debug
.\build\ninja-debug\AzureRender.exe --smoke-frames 120
```

Vulkan 后端、场景架构和 Shader 算法由项目实现。GLFW、Dear ImGui、tinygltf、stb 和 nlohmann/json 分别提供窗口界面、资产解析和 JSON 基础能力，不替代渲染核心。

## 资产边界

`Project/AzureRender/assets_public/` 可以进入 CI 和发布包。`assets_private/` 中的模型、纹理及其派生媒体不得提交、公开或进入安装包；公开回归和作品集必须只依赖许可明确的公共资产。
