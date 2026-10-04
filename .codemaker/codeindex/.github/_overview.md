---
type: "Module"
id: .github
title: "持续集成与发布门禁"
description: "回答“哪些改动允许进入 dev 与 main”。在 Linux 和 Windows 上自动构建、跑测试、执行发布门禁与视觉回归，文档改动时校验文风并发布 GitHub Pages。"
module_id: .github
architectural_role: "工程门禁层，不参与运行期渲染逻辑"
world_model_hints:
  - "位于仓库最外层，由 GitHub 事件触发，不随渲染器进程加载"
  - "自身不含 C++ 代码，校验能力全部来自 Project/AzureRender 下的脚本与 CMake 入口"
upstream_modules:
  - module: "."
    confidence: extracted
  - module: Project
    confidence: extracted
downstream_modules:
  - module: Project
    confidence: extracted
---

## Files

### 源代码路径
- `.github/`（工作流定义目录，当前含 `workflows/ci.yml` 与 `workflows/documentation.yml`）

### 知识库文档
- `.codemaker/codeindex/.github/_overview.md`（本文件）
- `.codemaker/codeindex/.github/github_ci_pipeline.md`
- `.codemaker/codeindex/.github/github_docs_publish.md`

### 符号索引
- 由 **Codemap MCP** 实时提供（`find_symbol` / `search_code` / `get_symbol_detail`）

## 子文档速览

| 子文档 | 覆盖内容 | 关键实体 |
|--------|---------|---------|
| `github_ci_pipeline.md` | 双平台构建、测试、发布门禁、视觉回归、隐私校验与产物上传 | `ci.yml`, `tools/run_release_gate.cmake`, `tools/run_visual_regression.py`, `tools/verify_portfolio.ps1` |
| `github_docs_publish.md` | 文档改动触发、文风与陈旧入口校验、MkDocs 严格构建与 Pages 发布 | `documentation.yml`, `tools/check_docs.sh`, `tools/check_doc_style.py`, `mkdocs.yml` |

## 模块概述

本模块是仓库的质量门禁，决定一次改动能否被合并或被发布。它把“这个改动是否破坏了构建、测试、视觉基准、隐私边界和文档规范”变成每条流水线都会自动回答的问题。

上游触发来自 GitHub 平台事件。`push` 到 `dev` 或 `main`、任意 `pull_request`、以及手工 `workflow_dispatch` 触发 `ci.yml`。只有跟随文档相关路径改动的 `push` 或 `pull_request` 才触发 `documentation.yml`，手工触发同样可用。

下游影响覆盖开发与交付两端。`ci.yml` 的失败会直接阻断合并与发布，并决定 `build/ci/AzureRender-*.tar.gz` 这类安装包是否被产出。`documentation.yml` 决定公开技术文档站点是否发布，而已安装包内的 `share/AzureRender/README.md` 会指向该站点，因此发布中断会让包内文档链接失去可读目标。

## 架构简析

分层结构：`GitHub 事件` → `.github/workflows/ci.yml`（或 `documentation.yml`）→ `Project/AzureRender/tools` 下的脚本入口。

本模块用两个工作流覆盖两类完全不同的风险面。`ci.yml` 面对代码与资产，核心文件是 `Project/AzureRender/tools/run_release_gate.cmake` 与 `tools/run_visual_regression.py`。`documentation.yml` 面对文字与站点，核心文件是 `tools/check_docs.sh`、`tools/check_doc_style.py` 与 `mkdocs.yml`。

两个工作流都以 `Project/AzureRender` 作为 `working-directory`。所有相对路径、构建目录和产物路径都基于这一约定展开。前置目录约定变化时，两个工作流会同时失效。

触发流可以概括为：事件筛选 → 依赖安装 → 构建与测试 → 门禁脚本 → 结果上传或站点发布。文本类校验与渲染类校验互不重叠，一条流水线失败不会掩盖另一条的结论。

扩展点体现在路径白名单与 `ciEnabled` 标记。新增文档页面需要同时更新 `mkdocs.yml` 的 `nav` 与 `documentation.yml` 的 `paths` 白名单。新增视觉回归用例则在 `tools/visual_regression_cases.json` 中声明，并决定是否纳入 CI 范围。

## 上下游关系

| 方向 | 模块 | 关联原因 | confidence |
|------|------|---------|------------|
| 上游 | `.` | 仓库根目录的 `README.md` 与文档改动会触发 `documentation.yml` 的路径白名单匹配 | extracted |
| 上游 | `Project` | `Project/AzureRender` 下的构建、测试、脚本与文档是流水线的实际校验对象 | extracted |
| 下游 | `Project` | CI 通过 `tools/run_release_gate.cmake`、`tools/run_visual_regression.py`、`tools/check_docs.sh` 调用项目的脚本入口 | extracted |
| 下游 | `.` | 工作流自身路径被写入 `documentation.yml` 的 `paths` 白名单，改动本模块会重新触发文档流水线 | extracted |

## 修改本模块会破坏什么

`ci.yml` 是发布门禁的唯一自动执行点。删掉或跳过发布门禁步骤，安装清单校验、资源检查和包内容隐私检查都会失去执行者。带私有资产的安装包仍能通过流水线。

视觉回归步骤是画面基准的安全网。放宽 `--tolerance relaxed` 或移除 `--ci-only`，会让角色渲染基线漂移无法被发现，画质变化只能靠人工肉眼看图确认。

`documentation.yml` 的 `paths` 白名单是文风与隐私命名规则的执行开关。白名单收窄后，改动仍会合并，但 `tools/check_docs.sh` 不再运行，密集标点、陈旧文档入口、作品集任务式截图命名都会静默通过。

`site_url` 与 GitHub Pages 配置是已安装包的对外链接契约。改动发布目标会让包内 `share/AzureRender/README.md` 指向不可达页面。

## 禁止与允许

允许在 `ci.yml` 中新增平台向量或校验步骤，前提是新步骤只使用 `assets_public` 下的公开资产，并显式声明 `working-directory`。

禁止在 CI 流程中写入或更新视觉回归基线。基线更新属于本地操作，由 `run_visual_regression.py --update-baseline` 承担，进入流水线会让基准失去独立参照意义。

禁止让流水线引用 `assets_private/` 与仓库内 `captures/`。`tools/run_release_gate.cmake` 对打包内容做隐私检查，`tools/verify_portfolio.ps1` 拒绝这两类路径进入作品集清单。约束由来见提交 `Initialize AzureRender workspace` 与 `chore(release): consolidate project layout and documentation`。

禁止以绕过脚本的方式认定通过。判断安装包与运行时是否合格，必须走 `run_release_gate.cmake` 定义的阶段序列，而不是单独执行 `cmake --build`。
