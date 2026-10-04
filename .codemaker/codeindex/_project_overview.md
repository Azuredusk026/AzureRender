---
type: "Module"
id: AzureRender
title: "AzureRender 实时渲染器"
description: "用 C++17 与原生 Vulkan 实现的实时渲染器，同一宿主承载风格化角色与黑洞两条渲染路径，并提供编辑器、确定性捕获与发布门禁。"
kb_path: .codemaker/codeindex/_project_overview.md
---

## 仓库定位

AzureRender 是 C++17 与原生 Vulkan 的实时渲染器，风格化角色与黑洞模拟共用同一宿主。工程直接实现 Vulkan 后端、场景架构与 Shader 算法，GLFW、Dear ImGui、tinygltf、stb、nlohmann/json 只承担窗口、界面与解析，不接管渲染核心。

交付物是安装树加一组可复现视觉证据，并附带 ImGui 编辑器、确定性 Capture 与 Manifest、诊断视图、GPU 计时与进程内 Renderer SDK。技术细节的唯一维护入口是 `Project/AzureRender/docs/` 下八份主题文档；本知识库只沉淀业务规则、系统约束与模块接口，符号定义由 Codemap MCP 实时提供。

## 技术栈

| 层面 | 选型 |
|------|------|
| 语言与构建 | C++17、CMake ≥ 3.20、CMakePresets、vcpkg（baseline `2bc124dac8436f5d8e108273aacf3f68d9824185`）；工程版本 `0.1.0`，应用版本宏 `0.1.0-rc1` |
| 图形 API | 原生 Vulkan，单图形队列，`glslc` 将 GLSL 编为 SPIR-V |
| 窗口与界面 | GLFW、Dear ImGui 1.92.8 docking，自源码编译以保证与编译器 ABI 一致 |
| 资产与序列化 | tinygltf、stb、nlohmann/json、OpenEXR、VMA |
| 工具链 | Windows MinGW（`x64-mingw-dynamic`）与 MSVC（`x64-windows`）双路径均须过门禁 |

## 顶层目录结构

| 路径 | 归属模块 | 职责 |
|------|---------|------|
| `.github/` | `.github` | 双平台 CI 与发布门禁、文档校验与 Pages 发布 |
| `Project/AzureRender/src/` | `Project` | app / render / rhi / scene / ecs / editor / extensions / scenes / assets / resources / platform / diagnostics |
| `Project/AzureRender/shaders/`、`schemas/` | `Project` | GLSL 源与 Shader Feature 目录、资产 Schema |
| `Project/AzureRender/tests/`、`tools/` | `Project` | 契约测试与配置、资产处理、视觉 QA、发布门禁脚本 |
| `Project/AzureRender/docs/` | `Project` | 面向读者的现行文档与 Pages 源 |
| `Project/AzureRender/assets_public/`、`portfolio/` | `Project` | 公共资产、视觉基线与作品集证据 |
| `Project/AzureRender/assets_private/` | `Project` | 本地私有角色资产，禁止提交与分发 |
| 仓库根 `README.md`、`AGENTS.md`、`.gitignore` | `__files` | 仓库定位与文档入口、Agent 工具协约、检出过滤与资产边界 |

> 📄 本节内容来源于仓库内置文档：`README.md`、`Project/AzureRender/README.md`、`docs/index.md`（原文已提炼）
