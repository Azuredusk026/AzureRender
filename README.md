# AzureRender

AzureRender 是一个用 C++17 和原生 Vulkan API 编写的实时渲染器。角色渲染和黑洞模拟共用同一个宿主。项目还提供 ImGui 编辑器、确定性捕获、诊断视图和 GPU 计时工具。场景可以通过进程内 Renderer SDK 接入。

> **English summary:** AzureRender is a real-time renderer written with C++17 and the native Vulkan API. One host runs both the stylized character renderer and the relativistic black-hole simulation. The project also includes an editor, deterministic capture, diagnostics and GPU timing.

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
9. [开发总计划](Project/AzureRender/docs/plans/azure-engine-plan.md)
10. [第三人称角色与可玩关卡计划](Project/AzureRender/docs/plans/third-person-playable-plan.md)

当前开发路线覆盖莱万汀外观、真实 idle/walk、基础 3C 与交互关卡。F1 为 Complete，R5、G5、G6、U1、F2 和 P1 为 Planned。

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

.\tools\msvc_env.bat cmake --preset msvc-debug
.\tools\msvc_env.bat cmake --build --preset msvc-debug
.\build\ninja-msvc-debug\AzureRender.exe --smoke-frames 120
```

项目直接实现 Vulkan 后端、场景架构和 Shader 算法。GLFW 和 Dear ImGui 负责窗口与界面。tinygltf、stb 和 nlohmann/json 负责解析资产和 JSON。它们不接管渲染核心。

## 资产边界

`Project/AzureRender/assets_public/` 可以进入 CI 和发布包。`assets_private/` 中的模型、纹理和派生媒体不能提交或公开，也不能进入安装包。公开回归和作品集只能使用许可明确的公共资产。
