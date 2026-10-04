---
type: "Fragment"
id: .github/docs_publish
title: "文档校验与站点发布"
description: "文档改动为什么必须在合并前通过文风与内容校验"
parent: /.github/_overview.md
fragment: docs_publish
entity_names:
  constants:
    - name: docs.branches
      value: "main"
      source: .github/workflows/documentation.yml
    - name: docs.runs-on
      value: "ubuntu-24.04"
      source: .github/workflows/documentation.yml
    - name: docs.python_version
      value: "3.12"
      source: .github/workflows/documentation.yml
    - name: docs.permissions
      value: "contents: read / pages: write / id-token: write"
      source: .github/workflows/documentation.yml
    - name: docs.concurrency_group
      value: "pages"
      source: .github/workflows/documentation.yml
    - name: docs.artifact_path
      value: "Project/AzureRender/site"
      source: .github/workflows/documentation.yml
    - name: mkdocs.site_url
      value: "https://azuredusk026.github.io/AzureRender/"
      source: Project/AzureRender/mkdocs.yml
    - name: style.max_cjk_per_sentence
      value: "45"
      source: Project/AzureRender/tools/check_doc_style.py
    - name: style.max_sentences_per_paragraph
      value: "4"
      source: Project/AzureRender/tools/check_doc_style.py
retrieval_hints:
  - "为什么只改了一个文档却被 CI 拦下"
  - "文档站点的自动发布流程是什么"
  - "文档标点和句长在哪个环节被强制检查"
  - "PR 阶段会不会部署文档站点"
  - "⚠️ 代码构建与发布门禁不在这里，在 `github_ci_pipeline.md`"
  - "⚠️ 文档写作规范本身不在这里，在 Project/AzureRender/docs/documentation-standard.md"
  - "本子模块也叫文档流水线，对应需求中的文档检查与站点发布"
architectural_role: "文档质量的执行层，是文风与公开性规则的唯一自动强制点"
---

## 业务意图

本子模块解决的问题是：文档不会因为作者疏忽而带着密集标点、陈旧入口或断链上线。文字类规则原本只是约定，这里把它变成合并与发布前的硬性检查。

它同时承担公开站点的交付。技术文档站点由主线文档自动构建，外部读者看到的内容与仓库内文档保持一致，不需要人工执行发布。

## 对外接口

对外承诺两件事：文档类改动会得到校验，主线文档会发布为站点。开发者合并到 `main` 即完成发布。

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `push` 加路径白名单 | 仓库 → CI | `README.md`, `docs/**`, `mkdocs.yml`, `requirements-docs.txt`, 两个检查脚本 | 主线文档改动触发校验并发布 | `.github/workflows/documentation.yml` |
| `pull_request` 加路径白名单 | 仓库 → CI | 同上 | 合并前先校验，失败不允许进入主线 | `.github/workflows/documentation.yml` |
| `workflow_dispatch` | 手工 → CI | 无 | 重跑校验或重新发布 | `.github/workflows/documentation.yml` |
| `pages` 并发组 | CI → CI | `cancel-in-progress: true` | 同一时刻只保留一次构建，避免半成品站点上线 | `.github/workflows/documentation.yml` |
| `AzureRender` 站点 | CI → 外部读者 | `mkdocs.site_url` | 公开技术文档入口，安装包内 README 指向它 | `Project/AzureRender/mkdocs.yml` |

## 跨模块依赖

本子模块只负责调度，规则内容全部来自项目侧的脚本与配置。

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project` | 必检文件与关键术语断言 | `tools/check_docs.sh` | extracted |
| `Project` | 中文句长与段落句数阈值 | `tools/check_doc_style.py` | extracted |
| `Project` | 站点导航、地址与排除规则 | `mkdocs.yml` | extracted |
| `Project` | 文档工具链版本 | `requirements-docs.txt` | extracted |
| `Project` | 被校验的运行时与计划文档 | `docs/runtime/rhi-synchronization.md`, `docs/plans/azure-engine-plan.md` | extracted |

反向依赖：安装树的 `share/AzureRender/README.md` 引用已发布站点地址，仓库根 `README.md` 属于路径白名单，其改动会触发站点重建。

## 典型调用链

### 文档改动的完整链路

```
push 到 main 且命中白名单 → documentation.yml:build   ← 本模块入口
  → pip install -r requirements-docs.txt
    → bash tools/check_docs.sh                ← 跨模块：必检项与术语断言
      → python tools/check_doc_style.py       ← 跨模块：句长与标点阈值
    → mkdocs build --strict                   ← 跨模块：站点严格构建
      → upload-pages-artifact（非 PR 才执行） ← 本模块
        → deploy-pages → github-pages 环境    ← 本模块出口
```

### PR 阶段的分支路径

```
pull_request 且命中白名单 → documentation.yml:build   ← 本模块入口
  → check_docs.sh 与 mkdocs build --strict    ← 跨模块：只校验
    → 制品上传与部署步骤整体跳过              ← 本模块：未合并内容不进入公开站点
```

## 实现约束清单

> 实现本模块相关需求时，Agent 必须在动笔前逐条核对以下项。

### 必须保持的发布配置

| 标识符 | 值 | 说明 | 约束由来 |
|-------|----|------|---------|
| `docs.runs-on` | `ubuntu-24.04` | 与主流水线保持同一镜像 | docs(site): 重构外部技术文档 |
| `docs.python_version` | `3.12` | 与文档工具链要求匹配 | 同上 |
| `docs.permissions` | `contents: read` 加 `pages: write` 加 `id-token: write` | 缺写权限会让部署失败而构建照常通过 | 同上 |
| `docs.concurrency_group` | `pages` | 同组互斥，防止并发部署互相覆盖 | 同上 |
| `docs.artifact_path` | `Project/AzureRender/site` | 必须与严格构建的输出目录一致 | 同上 |
| `style.max_cjk_per_sentence` | `45` | 单句汉字上限，超出直接失败 | docs(site): 简化文档行文 |
| `style.max_sentences_per_paragraph` | `4` | 单段句数上限，抑制长复合句 | 同上 |

### 必须保留的文档规则执行点

| 项目 | 说明 |
|------|------|
| 必检文件存在性 | 缺任一文档即失败，含 E0 交付物与公开基线目录 |
| 关键术语断言 | `SceneRendererRegistry`、`vkAcquireNextImageKHR`、`vkQueueSubmit`、`GpuAllocator`、`Face SDF`、`Schwarzschild` 必须仍在对应文档出现 |
| 标点禁用 | 密集标点与 LaTeX 反斜杠定界符被拒绝，数学式用 `$` 或 `$$` |
| 陈旧文档入口 | `ARCHITECTURE_CN`、`PROJECT_OVERVIEW_CN` 一类旧入口名不得再现 |
| 作品集图片命名 | `P1`、`S36`、`CQ0`、`final_final` 一类任务式命名被拒绝 |
| 站点导航完整性 | `nav` 列出的页面必须真实存在，缺页即构建失败 |

### 设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 校验逻辑归属 | 复用项目侧检查脚本 | 本地与流水线结论一致，规则只有一份定义 |
| 构建严格程度 | 使用 `--strict` | 把断链与缺页变成失败，避免缺陷内容上线 |
| PR 行为 | 只构建不部署 | 未合并内容不应覆盖公开站点 |
| 归档文档处理 | 用 `exclude_docs` 排除归档目录与旧命名文档 | 归档内容含旧结构，参与构建会带入陈旧描述 |
| 工具链版本 | 固定 Python 与依赖版本范围 | 工具升级会改变渲染结果，固定版本让失败原因收敛 |

## 边界规则

允许调整 `nav` 结构与新增公开文档。前提是每个新页面在提交中真实存在，否则严格构建会直接失败。

禁止把未完成的能力写进公开文档。文档只能描述已经实现的行为，只有计划文档可以使用计划语气。

不得绕过路径白名单来放宽检查。把新文档目录加入白名单是正确做法，把检查脚本从白名单移除会让规则失去执行者。

禁止在站点中发布归档类文档。归档内容依赖旧命名与旧结构，其对外描述与当前实现不一致。

路径白名单的维护规则是只增不减。新增文档目录或脚本时同步追加白名单，否则该目录的改动会绕过全部文档检查。

## 站点交付契约

站点内容来自仓库内的 Markdown。`docs/` 下的架构、运行时、计划与验收文档构成主体，仓库根 `README.md` 与项目 `README.md` 提供入口说明，导航顺序由 `mkdocs.yml` 的 `nav` 决定。

站点地址是已安装包的对外引用目标。安装树内的说明文件指向公开地址。发布目标与站点地址变更因此属于跨模块契约，不能只当构建参数处理。

归档类内容不进入公开站点。旧命名与旧结构的文档通过排除规则跳过，避免读者把历史描述当成当前实现。

站点只发布主线内容。主线之外的改动会在 PR 阶段得到校验结论，但不会生成可访问的站点版本。

新增公开文档需要同时更新三处。文件本体进入 `docs/`，导航项进入 `mkdocs.yml`，涉及发布说明的术语变化还要同步到参考文档。三处不一致时，严格构建与术语断言会分别报错，错误信息能指出缺失的那一处。

## 与主流水线的分工

两条流水线按风险类型分工。代码与资产类检查归主流水线，文字与站点类检查归本子模块，两者不互相替代。

文档一致性检查在两条流水线中都会运行。主流水线的检查保护的是与代码同批提交的文档，本子模块的检查保护的是文档自身的完整性。

因此文档规则的实际执行点有两个。只要其中一个被删除或跳过，文风与隐私命名规则就只剩一个执行点，覆盖面随之下降。

## 规则的由来

文风阈值来自一次专门的文档简化工作。其结论是文档必须使用短句，并限制每段句数。长复合句在翻译与检索场景下容易丢失主语，所以阈值被写进脚本，而不是停留在评审意见里。

作品集命名规则同样有明确由来。项目整理阶段要求图片与截图使用描述性命名，外部读者不关心任务编号。因此检查脚本会把任务式命名判为失败，而不是仅作提醒。

必检文件与关键术语断言保护的是文档骨架。渲染架构、RHI 同步与角色渲染三类文档必须持续覆盖核心术语。这样重构后文档与实现不会脱节，缺失会被当作构建失败。

状态描述的真实性要求来自项目规范。文档禁止把待实现功能写成现有能力。新增能力时先更新实现再更新描述，计划语气只允许出现在计划文档中。

## 谁在读这些文档

外部读者通过站点了解渲染器的架构、材质与资产管理方式。文档中的术语必须与实现一致，路径与命令必须可以直接执行。

项目维护者依赖文档定位实现边界。运行时文档记录渲染图、同步与聚簇光照的设计取舍。改动这些子系统前先读对应文档，可以避免重复讨论相同取舍。

验收记录使用固定结论词。验收矩阵只允许通过、失败、未执行与阻塞四种状态，并记录原因与证据路径，使结论可被复查。

新加入者依赖入口文档判断阅读顺序。总览、构建说明与架构文档构成最短路径，站点导航顺序因此属于有意的设计，而不是列表项的排列。

## 修改本子模块会破坏什么

路径白名单是文档规则的开关。白名单收窄后，改动仍会合并，但标点、句长、陈旧入口与作品集命名规则不再有人执行，规则退化为约定。

移除严格构建会让缺陷内容上线。导航新增页面却未提交对应文件时，默认构建只给出警告，站点会出现空白或断链入口。

站点地址与部署权限是对外链接契约。地址变更会让安装包内 README 指向不可达页面，缺少写权限时构建通过而部署失败，读者看到的版本落后于主线。

文风阈值决定既有文档的通过状态。阈值放宽会让长复合句重新进入文档，收紧会让已合并文档在下一次文档改动时集中失败。
