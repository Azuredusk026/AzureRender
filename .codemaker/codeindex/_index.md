---
type: "Module"
id: AzureRender
title: "AzureRender 实时渲染器"
description: "用 C++17 与原生 Vulkan 实现的实时渲染器，同一宿主承载风格化角色与黑洞两条渲染路径，并提供编辑器、确定性捕获与发布门禁。"
version: "1"
last_updated: "2026-10-01"
total_modules: 3
kb_path: .codemaker/codeindex/_index.md
---

# AzureRender 全局架构索引

## 路径映射

| 项 | 路径 |
|----|------|
| 知识库根 | `.codemaker/codeindex/`（仓库根 `E:/docs/projects/AzureRender`） |
| 全局文档 | `_project_overview.md`、`_architecture.md`、`_core_systems.md`、`_catalog.md`、`_concept_index.md` |
| 配置文件 | `.codemaker/codeindex/kb-config.json` |
| 符号索引 | 由 **Codemap MCP** 实时提供（`find_symbol` / `search_code` / `get_symbol_detail`） |

## 模块清单

| 模块 | 源码路径 | 知识库路径 | 架构角色 | 子文档 |
|------|---------|-----------|---------|--------|
| `.github` | `.github/` | `.codemaker/codeindex/.github/` | 工程门禁层，不参与运行期渲染逻辑 | `github_ci_pipeline.md`、`github_docs_publish.md` |
| `Project` | `Project/` | `.codemaker/codeindex/Project/` | 引擎工程主体：Vulkan 宿主 + 渲染核心 + 场景 Renderer + 编辑器 + 工具链 | `Project_host`、`Project_render_core`、`Project_scene_editor`、`Project_renderer_sdk`、`Project_character`、`Project_blackhole`、`Project_build_release` |
| `__files` | `.`（`README.md`、`AGENTS.md`、`.gitignore`） | `.codemaker/codeindex/__root/__files/` | 仓库根入口与契约层 | `__files_entry.md`、`__files_policy.md` |

## 模块依赖关系

| 方向 | 模块 | 对端 | 说明 | confidence |
|------|------|------|------|------------|
| 上游 | `.github` | `.`、`Project` | 根文档改动触发路径白名单；工程内构建、测试、脚本与文档是校验对象 | extracted |
| 下游 | `.github` | `Project` | 门禁调用 `run_release_gate.cmake`、`run_visual_regression.py`、`check_docs.sh` | extracted |
| 上游 | `Project` | `.` | 根 README 指向工程；根 `.gitignore` 决定 `build/`、`captures/`、`assets_private/` 不入库 | extracted |
| 下游 | `Project` | `.` | 工程产出视觉基线与作品集证据，受根级公开边界约束 | inferred |
| 上游 | `__files` | `Project` | 主题页与作品集说明通过相对链接被根级入口引用 | inferred |
| 下游 | `__files` | `.github`、`Project` | 文档发布以根 README 变更为触发条件；工程文档遵守根级入口与资产规约 | extracted |

## 阅读路径

1. `_project_overview.md` 确认仓库定位与技术栈
2. `_concept_index.md` 按需求关键词直达子文档
3. `_architecture.md` 的「系统架构约束」查红线与禁忌
4. `_core_systems.md` 查子系统上下游
5. 符号与调用关系一律用 Codemap MCP 实时查询
