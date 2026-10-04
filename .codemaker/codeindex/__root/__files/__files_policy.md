---
type: "Fragment"
id: __files/policy
title: "仓库根入口与边界契约 / 检出过滤与资产边界"
description: "哪些文件禁止提交，私有角色资产和凭据会不会被带进仓库或发布包？"
parent: /__files/_overview.md
fragment: policy
entity_names:
  constants:
    - name: ignore_local_reference
      value: "/AfterglowRender/"
      source: .gitignore
    - name: ignore_build_globs
      value: "**/build/、**/cmake-build-*/、**/out/"
      source: .gitignore
    - name: ignore_capture_dir
      value: "**/captures/"
      source: .gitignore
    - name: ignore_credential_globs
      value: ".env、.env.*、*.pem、*.pfx、*.p12"
      source: .gitignore
    - name: ignore_release_package_globs
      value: "/AzureRender-*-Windows-AMD64/、.zip、.tar.gz、*.dll、*.exe、*.pdb"
      source: .gitignore
    - name: ignore_local_tooling
      value: ".codemap/、.codemaker/codemap/、.codemaker/codeindex/、opencode.json"
      source: .gitignore
    - name: public_asset_dir
      value: "Project/AzureRender/assets_public/"
      source: README.md
    - name: private_asset_dir
      value: "Project/AzureRender/assets_private/"
      source: README.md
    - name: portfolio_image_naming
      value: "<scene>_<view-or-purpose>_<look-or-technique>_v<NN>_<width>x<height>.png"
      source: Project/AzureRender/portfolio/README_CN.md
    - name: editor_capture_tag_charset
      value: "字母、数字、连字符、下划线"
      source: Project/AzureRender/docs/assets-and-editor.md
retrieval_hints:
  - "哪些文件或目录禁止提交到版本库？"
  - "私有角色模型、纹理和派生媒体能进 CI、发布包或作品集吗？"
  - "Agent 检索本仓库时必须用哪些工具？"
  - "公开视觉证据的文件名要按什么规则写？"
  - "本模块也叫根级忽略规则 / 资产边界，对应需求中的「仓库卫生」「公开证据」「许可边界」"
  - "新增忽略规则一律写入仓库根 .gitignore，不可在子目录另建忽略文件或临时白名单"
  - "⚠️ 如果你要找的是资产导出、格式校验或编辑器 Capture 流程细节，不在这里，在 Project 模块的 docs/assets-and-editor.md"
architectural_role: "仓库卫生与资产授权边界，禁止私有资产与凭据进入版本库、CI 与发布包"
---

## Files

- 源码文件：`.gitignore`（检出过滤）、`AGENTS.md`（Agent 工具协约）
- 被本模块引用、并由本模块保证边界的相邻说明：`Project/AzureRender/assets_private/README.md`、`Project/AzureRender/portfolio/README_CN.md`

## 本子模块解决什么问题

本模块把"什么东西绝对不能离开本机"写在仓库最外层：私有第三方角色资产、本机凭据、可再生的构建产物与捕获、仅供本地参考的旧渲染器，都由 `.gitignore` 一次挡掉；同时用根 `README.md` 的资产边界段与 `AGENTS.md` 的工具协约，向提交者与 Agent 声明同一条授权与检索前提。它保护的是许可合规、凭据安全与公开证据的可信度，这三项一旦破坏无法靠后续提交回退。

## 对外接口

本子模块无协议与 RPC 接口，对外生效的形式是三条可核验规约：

| 契约 | 适用对象 | 允许 | 禁止 |
|------|---------|------|------|
| 检出过滤 | 全部本地文件 | 源码、许可明确的资产、测试基线、构建配置、维护中的文档 | 构建目录、站点输出、临时捕获、分析数据、本机 IDE 状态 |
| 资产授权边界 | `assets_public/` 与 `assets_private/` | 公共资产进入 CI、安装树与公开回归 | 私有模型、纹理、派生导出、私有截图与视频进入版本库、CI、安装与公开发布包 |
| Agent 检索协约 | 父代理与子代理 | Codemap MCP 工具检索，取得符号详情后立即编辑 | 用 Grep/Read 替代 Codemap，用不支持 MCP 的 `explore` 代理 |

## 变更风险

- **放宽或删除 `.gitignore` 中的资产与凭据模式**：`assets_private/` 下的第三方角色网格、纹理与派生导出，以及 `.env`、`*.pem`、`*.pfx`、`*.p12` 一旦提交，会随历史提交进入远端，撤销单个文件不影响历史，属于许可泄露与凭据泄露类的不可回退事故；`assets_private/README.md` 明确要求未经验证许可不得提交。
- **把 `.codemaker/codeindex/`、`.codemaker/codemap/`、`opencode.json` 从忽略规则中移除**：本机知识库索引与 MCP 配置进入版本库，克隆体积与 CI 缓存随之膨胀，且其中含有本机绝对路径。
- **删除 `**/build/`、`**/captures/`、`**/cmake-build-*/` 等再生项过滤**：可再生成的大体积中间产物被提交，发布门禁与产物校验（`tools/verify_portfolio.ps1`、安装清单文件级 SHA-256）会被无关差异污染，比对结果失去意义。
- **修改 `AGENTS.md` 的 Codemap 工具规约**：Agent 退回纯文本检索，拿不到调用链、类型层次与跨文件引用，改动评估退化为按文件而非按符号，跨模块影响被漏判；同步要求（取得符号详情后立即编辑，不重复读同一文件）失效后会产生冗余读取代价。
- **改动作品集命名规约而不同步校验脚本**：`portfolio/README_CN.md` 规定正式图像使用固定命名模板，并禁止任务过程名与时间戳；命名与 `portfolio_manifest.json`、`tools/verify_portfolio.ps1` 的期望不一致时，CI 的证据检查失败或证据指向错误文件。

## 边界：允许什么、禁止什么

允许进入版本控制的内容：渲染器与工具源码、许可明确的公共资产、测试基线与视觉回归基线、构建与打包配置、维护中的文档与 Schema、必要的许可与第三方声明文件。判断标准只有一条——提交者能否说明其许可来源与重建方式。

禁止进入版本控制的内容分为四类。凭据类：`.env` 及变体、`*.pem`、`*.pfx`、`*.p12`，因为一旦推送即进入远端历史，凭据轮换也无法撤销已泄露的内容。私有资产类：`assets_private/` 下的第三方角色网格、纹理、派生导出与可再分发压缩包，除非许可已被逐项确认；该目录在全新克隆、源码归档、CI、安装树与发布包中均不存在，本地文件仅用于可选的 QA。再生类：`**/build/`、`**/cmake-build-*/`、`**/captures/`、站点输出与探针可执行文件，它们构成本地状态而非交付物，提交后会污染安装清单的文件级 SHA-256 比对。本地工具类：`.codemap/`、`.codemaker/codemap/`、`.codemaker/codeindex/`、`opencode.json` 与本机 IDE 状态，其中含本机绝对路径与个人检索配置。

资产使用的方向性约束同样明确：`assets_public/` 可以进入 CI、发布包与公开回归；公开作品集只能使用许可明确的公共资产，不得包含 `assets_private/` 内容。发布包侧的禁止清单与本节一致，另外排除本机绝对路径、凭据与临时日志。

平台与路径约束：运行时资源不依赖源码绝对路径，示例与配置中的路径使用仓库相对形式或占位符；忽略规则集中写在仓库根，用 `**/` 递归匹配，子目录不另行声明规则。

## 实现约束清单

> 实现本模块相关需求时，Agent 必须在动笔前逐条核对以下项。

### 必须定义的常量/枚举
| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------|
| 本地参考渲染器排除 | `/AfterglowRender/` | `.gitignore` | 早期参考代码仅作历史输入，不属于 AzureRender | `Initialize AzureRender workspace` |
| 私有资产目录 | `assets_private/` | `README.md` | 模型、纹理与派生媒体不可提交、不可公开、不可进安装包 | `docs(site): 重构外部技术文档` |
| 公共资产目录 | `assets_public/` | `README.md` | 许可明确，可进 CI 与发布包 | `chore(release): consolidate project layout and documentation` |
| 凭据文件模式 | `.env`、`.env.*`、`*.pem`、`*.pfx`、`*.p12` | `.gitignore` | 本机凭据永不入库 | `chore(repo): 清理误提交的发布包` |
| 发布包模式 | `/AzureRender-*-Windows-AMD64/` 及 `.zip`、`.tar.gz` | `.gitignore` | 发布包由门禁生成，不入库 | `chore(repo): 清理误提交的发布包` |
| 证据命名模板 | `<scene>_<view-or-purpose>_<look-or-technique>_v<NN>_<width>x<height>.png` | `portfolio/README_CN.md` | 版本号只在有意改变画面基准时递增 | `chore(release): consolidate project layout and documentation` |
| 编辑器 Capture 标签字符集 | 字母、数字、连字符、下划线 | `docs/assets-and-editor.md` | 输出到本地 `captures/`，属工作图，不构成作品集证据 | `docs(site): 重构外部技术文档` |

> `assets_private/README.md` 与 `docs/documentation-standard.md` 的「文件卫生」一节是本清单两条边界的出处：源文件、许可明确的资产与维护中的文档入库；构建、站点输出、临时捕获与分析数据进入生成目录并配置忽略规则。

### 设计决策（存在多种可行方案时必填）
| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 忽略规则放置位置 | 仓库根 `.gitignore` 集中声明，用 `**/` 递归匹配 | 每个子目录各放一份忽略文件 | 一条规则覆盖全部工程，避免子目录漏配导致构建产物或凭据入库 |
| 私有资产处置 | 目录保留在工作树，用 `.gitignore` 排除，目录内仅保留说明与自身忽略文件 | 直接删除目录 | 本地 QA 仍需要私有角色做验证，同时保证克隆、CI 与发布包中不存在该资产 |
| 排除性说明写法 | 在根 README 资产边界段落陈述现行许可规则 | 只留在忽略文件里 | 忽略规则对读者隐藏，许可边界需要面向读者可见的表述 |
| Agent 工具规约载体 | 根 `AGENTS.md`，位于代理默认读取路径 | 放在 `Project/AzureRender/docs/` | 该文件被所有进入仓库的代理读取，属于跨工程协约而非产品文档 |

## 跨模块依赖

> 实现本子模块功能时，除本模块外还需引用的外部模块：

| 依赖模块 | 引用原因 | 关键符号 | confidence |
|---------|---------|---------|------------|
| `Project` | 资产目录、忽略对象与校验脚本的实际拥有者 | `assets_public/`、`assets_private/`、`tools/verify_portfolio.ps1` | extracted |
| `.github` | CI 在干净检出上构建，忽略规则缺失会直接改变 CI 输入 | `.github/workflows/ci.yml` | extracted |

> 反向依赖（谁调用了本子模块）：

| 调用方模块 | 调用场景 | 关键符号 |
|-----------|---------|---------|
| `Project` | 发布与安装清单要求排除私有资产、缓存与本机绝对路径 | `docs/development-and-release.md` |
| `Project` | 作品集证据校验依赖固定命名与公开资产来源 | `portfolio/README_CN.md`、`tools/verify_portfolio.ps1` |
| `.github` | 全新克隆即为 CI 输入，本地未忽略文件等同进入 CI | `ci.yml` |

## 典型调用链

### 提交前的仓库卫生自检
```
本地生成构建产物、捕获或私有资产
  → 仓库根 .gitignore 过滤再生项与私有资产                     ← 本模块入口
  → git status 确认无 assets_private/ 与凭据类文件              ← 本模块边界
  → .github/workflows/ci.yml 在全新检出上执行发布门禁            ← 跨模块：.github
```

### 新增一张公开视觉证据
```
确定展示基准与版本号
  → 按 portfolio 命名模板命名图像文件                            ← 本模块规约
  → 写入 Project/AzureRender/portfolio/images/<scene>/           ← 跨模块：Project
  → 更新 portfolio_manifest.json 与 evidence/                    ← 跨模块：Project
  → tools/verify_portfolio.ps1 校验命名、来源与 SHA-256           ← 跨模块：Project
```

> 📄 本节内容来源于仓库内置文档：`README.md`、`AGENTS.md`、`.gitignore`，并引用 `Project/AzureRender/assets_private/README.md`、`Project/AzureRender/portfolio/README_CN.md`、`Project/AzureRender/docs/documentation-standard.md` 中与本模块边界直接相关的表述（原文已提炼，非完整转录）。
