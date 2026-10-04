---
type: "Fragment"
id: .github/ci_pipeline
title: "持续集成与发布门禁流水线"
description: "改动提交后 CI 会执行哪些构建、测试和门禁检查"
parent: /.github/_overview.md
fragment: ci_pipeline
entity_names:
  constants:
    - name: linux.runs-on
      value: "ubuntu-24.04"
      source: .github/workflows/ci.yml
    - name: windows.runs-on
      value: "windows-2025"
      source: .github/workflows/ci.yml
    - name: ci.branches
      value: "dev, main"
      source: .github/workflows/ci.yml
    - name: ci.working-directory
      value: "Project/AzureRender"
      source: .github/workflows/ci.yml
    - name: linux.vulkan_icd
      value: "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json"
      source: .github/workflows/ci.yml
    - name: smoke.frames
      value: "120"
      source: .github/workflows/ci.yml
    - name: visual_regression.tolerance
      value: "relaxed"
      source: .github/workflows/ci.yml
retrieval_hints:
  - "提交代码后 CI 会跑哪些构建和门禁检查"
  - "为什么构建通过却没有安装包产出"
  - "视觉回归在流水线里用哪个容差跑"
  - "改动渲染器后哪些画面表现会被自动拦截"
  - "⚠️ 本地一键发布门禁脚本 `tools/run_release_gate.cmake` 的实现不在这里，在 Project/AzureRender/tools"
  - "⚠️ 文档站点发布流程不在这里，在 `github_docs_publish.md`"
architectural_role: "代码与资产类门禁的执行层，只允许消费公开资产"
---

## 业务意图

本子模块解决的问题是：任何一次提交是否处于可发布状态，不需要人工判断也能得到结论。它把构建、单元测试、安装包完整性、画面基准和内容隐私这五类风险固定在同一个自动入口上执行。

对开发者而言，这条流水线是发布前置条件。安装包的产出与失败诊断材料都由它提供，缺了这一步，画质退化与包体污染只能靠人工事后发现。

## 对外接口

流水线对外的契约由触发条件、执行阶段和产物组成。开发者只需推送改动或手工触发，无需额外调用。

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `push` | 仓库 → CI | `dev`, `main` | 合并后确认主线仍可发布 | `.github/workflows/ci.yml` |
| `pull_request` | 仓库 → CI | 任意目标分支 | 合并前暴露构建与画质问题 | `.github/workflows/ci.yml` |
| `workflow_dispatch` | 手工 → CI | 无 | 在指定分支复现门禁结论 | `.github/workflows/ci.yml` |
| 安装包产物 | CI → 开发者 | `build/ci/AzureRender-*.tar.gz*` | 仅门禁通过后产出 | `.github/workflows/ci.yml` |
| 回归摘要产物 | CI → 开发者 | `visual-regression-summary.json`, `*_diff.png` | 失败时定位偏离基线的用例 | `.github/workflows/ci.yml` |

## 跨模块依赖

本子模块不实现任何校验逻辑，全部委托给项目脚本，这一委托关系本身就是契约。

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project` | 发布门禁阶段序列定义在项目脚本内 | `tools/run_release_gate.cmake` | extracted |
| `Project` | 视觉回归比对与容差判定 | `run_visual_regression.py:main`, `compare` | extracted |
| `Project` | 渲染器 CLI 是冒烟测试执行体 | `azurerender.parseCommandLine`, `AzureRenderApp.mainLoop` | extracted |
| `Project` | 依赖版本定位 | `vcpkgJsonGlob` → `Project/AzureRender/vcpkg.json` | extracted |
| `Project` | 作品集公开性与哈希校验 | `tools/verify_portfolio.ps1` | extracted |

反向依赖：项目安装树内的 `share/AzureRender/README.md` 引用 CI 产物与站点地址，改动产物命名会连带破坏包内说明的可读性。仓库根 `README.md` 描述的构建入口同样以本流水线的结论为准。

## 典型调用链

### 发布门禁链（Linux，Debug 配置）

```
push / pull_request → ci.yml:linux        ← 本模块入口
  → cmake 配置 build/ci（Ninja, Debug）
    → cmake -P tools/run_release_gate.cmake   ← 跨模块：Project 门禁
      → configure → build → test → install
        → write-install-manifest → verify-install-manifest
```

### 视觉回归与冒烟链（Linux，软件栅格化）

```
push / pull_request → ci.yml:linux        ← 本模块入口
  → xvfb-run + VK_ICD_FILENAMES=lvp_icd.x86_64.json
    → AzureRender --smoke-frames 120      ← 跨模块：渲染器 CLI
    → run_visual_regression.py --tolerance relaxed --ci-only
      → run_visual_regression.py:compare  ← 跨模块：基线比对
        → 上传回归摘要与差异图            ← 本模块出口
```

## 实现约束清单

> 实现本模块相关需求时，Agent 必须在动笔前逐条核对以下项。

### 必须固定的运行环境

| 标识符 | 值 | 说明 | 约束由来 |
|-------|----|------|---------|
| `linux.runs-on` | `ubuntu-24.04` | 需自带 Vulkan 验证层与 `glslc` | 完成 AR-4.4 跨平台持续集成 |
| `windows.runs-on` | `windows-2025` | MSVC 构建与命令行冒烟的执行环境 | 完成 AR-4.4 跨平台持续集成 |
| `ci.working-directory` | `Project/AzureRender` | 全部相对路径的基准目录 | chore(release): consolidate project layout and documentation |
| `linux.vulkan_icd` | `lvp_icd.x86_64.json` | 无物理 GPU 时的渲染来源 | feat(e0): 建立视觉回归与性能基准安全网 |
| `smoke.frames` | `120` | 覆盖资源加载与首帧之后的稳定期 | 完成 AR-4.4 跨平台持续集成 |

### 必须保持的校验语义

| 项目 | 说明 |
|------|------|
| 门禁阶段序列 | 必须依次通过配置、构建、测试、安装与安装清单校验才算通过 |
| 包内容隐私检查 | 打包内容命中 `assets_private` 或 `captures/` 即失败，CI 不得跳过 |
| 回归容差分工 | 流水线用 `relaxed`，本地基线比对用 `strict`，两者不可互换 |
| 文档检查复用 | 同一份 `tools/check_docs.sh` 在 Linux 与 Windows 各跑一次 |
| 作品集校验 | 仅 Windows 侧执行，拒绝私有路径并核对文件哈希 |

### 设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 回归用的 GPU | 软件栅格化加 `relaxed` 容差 | 托管运行器不提供稳定 GPU，软件路径让结论可复现 |
| 平台分工 | Linux 跑 Debug 全量校验，Windows 跑 Release 加命令行冒烟 | Linux 便于定位失败，Windows 补打包与运行时验证 |
| 失败留证范围 | 只留摘要与差异图 | 差异图足以定位偏差，全图集推高存储成本 |
| 基线更新位置 | 只允许在本地执行 | 流水线更新基线会让基准跟随实现漂移 |

## 边界规则

允许在本子模块新增平台矩阵项与校验步骤。前提是新步骤只使用 `assets_public` 下的公开资产，并显式声明工作目录，否则相对路径会在另一平台上失效。

禁止在流水线中更新视觉基准，也禁止把基准文件的写入权限交给 CI。基线一旦由被验证对象自己改写，比对结果不再具备约束力。

不得用单独执行构建命令替代门禁结论。发布判断的权威来源是 `run_release_gate.cmake` 的阶段序列与结果文件，绕开它就绕开了安装清单校验和隐私检查。

流水线内部不得引用 `assets_private/`，也不得依赖开发者机器上的绝对路径。由来是发布包必须是自包含且可公开的产物。

## 谁依赖这条流水线的结论

开发者依赖它判断一次提交是否可以合并。PR 阶段的门禁结论就是合并前的最后一次自动校验，结论为失败时不需要人工复核即可判断改动方向有误。

发布者依赖它拿到可交付的安装包。安装包只在门禁通过后产出。包内的共享库、资源与文档都经过安装清单校验，因此拿到产物即可视为已通过完整性检查。

作品集与公开演示依赖它的隐私边界。包内容不得命中 `assets_private` 与 `captures/`，这条检查让公开仓库的产物可以安全分享给外部读者。

手工触发场景依赖它的可复现性。需要确认某次历史提交是否仍然合格时，开发者可以直接对目标分支执行手工触发。这样不必在本地重建整套环境。

## 失败定位路径

门禁失败时，第一手材料是各阶段日志。项目脚本会把每个阶段的输出写入构建目录下的门禁子目录。YAML 中只需指出失败阶段名，定位工作交给脚本产物。

画面类失败的第一手材料是回归摘要与差异图。摘要记录每个用例的容差与偏差指标，差异图标出偏离区域。这样不需重新渲染即可判断是基线过期还是实现退化。

构建与环境类失败通常在配置阶段就能分辨。依赖安装、Vulkan 加载器与编译器诊断都会写入日志，区分环境问题与代码问题的依据是失败阶段名。

## 约束的由来

发布门禁的存在是为了让交付物可以脱离开发机运行。安装清单校验与资源检查都围绕自包含性设计，因此任何新增运行时依赖都必须先进入安装树。

画面基准的约束来自一次专门的安全网建设。视觉回归与性能基准在同一步引入。其目的是让后续结构改造不会静默改变外观，所以容差阈值与用例清单都属于需要评审的变更。

隐私边界来自公开仓库的属性。安装包与作品集都必须能交给外部读者。打包内容对私有目录的检查因此是硬性条件，而不是可选的清理步骤。

## 门禁结论的判定口径

一次运行只有在全部阶段返回成功时才视为通过。任一阶段失败都会终止后续阶段，因此失败日志指向的阶段通常就是根因所在层。

标记为仅开发可用的产物不等同于发布失败。Windows 调试配置的安装树会被标注为开发用途，这类结论表明校验已经完成，只是不适合对外分发。

产物只在结论为正时产生。开发者在下载列表里看到安装包，就可以确认该次提交已经通过完整门禁，而不必回溯每次运行的日志。

## 修改本子模块会破坏什么

触发条件是门禁的覆盖范围。删除 `pull_request` 后，问题只能在合并之后暴露，主线会先带上失败提交。

冒烟步骤承担启动级回归的拦截。移除后，渲染器在资源加载或场景创建路径上损坏时，安装包仍会照常发行。使用者在首次启动时才会遇到失败。

回归步骤承担画面基准的拦截。容差被放宽或步骤被删除时，角色渲染的退化不再有自动信号，公开画质只能靠人工看图发现。

产物路径同时出现在流水线与工具默认值中。两者不一致时上传步骤拿到空目录，失败诊断失去材料，问题定位成本明显上升。
