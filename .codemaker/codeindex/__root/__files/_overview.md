---
type: "Module"
id: __files
title: "仓库根入口与边界契约"
description: "定义 AzureRender 的对外定位、主题文档入口、检出过滤规则与私有资产边界，是阅读仓库和提交改动的第一道契约。"
module_id: __files
architectural_role: "仓库根入口与契约层"
world_model_hints:
  - "位于仓库根，不在渲染器运行路径内，供人与 Agent 阅读、导航与校验"
  - "被 .github 的文档工作流、Project 的主题文档与 CI 文档检查反向引用"
upstream_modules:
  - module: Project
    confidence: inferred
downstream_modules:
  - module: Project
    confidence: extracted
  - module: .github
    confidence: extracted
---

## Files

### 源代码路径
- `.`（仓库根散落文件：`README.md`、`AGENTS.md`、`.gitignore`）

### 知识库文档
- `.codemaker/codeindex/__root/__files/_overview.md`（本文件）
- `.codemaker/codeindex/__root/__files/__files_entry.md`
- `.codemaker/codeindex/__root/__files/__files_policy.md`

### 符号索引
- 由 **Codemap MCP** 实时提供（`find_symbol` / `search_code` / `get_symbol_detail`）

## 子文档速览

| 子文档 | 覆盖内容 | 关键实体 |
|--------|---------|---------|
| `__files_entry.md` | 仓库对外定位、八份主题文档入口、快速构建命令、文档唯一来源规约 | `README.md`、`docs/index.md`、`build/ninja-debug`、`--smoke-frames 120` |
| `__files_policy.md` | 检出过滤、公开与私有资产边界、凭据禁忌、Agent 工具规约、证据命名规约 | `.gitignore`、`assets_public/`、`assets_private/`、`AGENTS.md` |

## 模块概述

本模块解决"进入仓库先读哪一页、什么能提交、什么必须挡在版本库和发布包之外"这一组问题：它把仓库定位、文档入口清单、检出过滤规则与私有资产边界固化在三个根级文件里，使外部读者、CI 与 Agent 依据同一份契约行动，而不必靠目录猜测或逐层试探。

上游：由提交者与 Agent 直接编辑；`Project/AzureRender/docs/**` 的主题页与 `portfolio/` 的证据说明通过相对链接被本模块引用，`.github` 的文档工作流把根 `README.md` 的变更作为触发条件。

下游：`.github` 的文档发布工作流与 CI 文档检查依赖本模块给出的入口路径；发布包与公开证据的可信度取决于本模块的资产边界。改动这里影响的是文档可得性、版本库卫生与公开材料的授权风险，而非渲染运行时本身。

## 架构简析

分层结构（阅读顺序）：入口层:`README.md` → Agent 协约层:`AGENTS.md` → 检出过滤层:`.gitignore`

- **`README.md`**：仓库对外定位与八份主题文档入口，唯一面向人类读者的根级导航；技术细节一律外链到 `Project/AzureRender/docs/`，不在根级复述。
- **`AGENTS.md`**：约定 Agent 使用 Codemap MCP 工具检索而非 Grep/Read，并要求取得符号详情后立即编辑。
- **`.gitignore`**：声明构建产物、捕获、凭据、本地参考代码与本地知识库不进入版本控制。

核心数据流：读者或 Agent 依据 `README.md` 选定主题入口 → 进入 `Project/AzureRender/docs/<topic>.md` 获取现行规则 → 提交前按 `.gitignore` 与资产边界判断本地产物是否需要排除。

## 上下游关系

> `extracted` = 静态分析可信；`inferred` = Agent 推断待复核

| 方向 | 模块 | 依据 | confidence |
|------|------|------|-----------|
| 我引用 | `Project` | `README.md` 链接 `Project/AzureRender/docs/*.md` 与 `portfolio/images/**` | extracted |
| 我引用 | `.github` | 根 `README.md` 变更触发 `.github/workflows/documentation.yml` | extracted |
| 引用我 | `Project` | `Project/AzureRender/README.md`、`portfolio/README_CN.md` 回链根级定位表述 | inferred |
| 引用我 | `.github` | 文档工作流与 CI 文档检查以根级入口路径与资产边界为校验对象 | extracted |
