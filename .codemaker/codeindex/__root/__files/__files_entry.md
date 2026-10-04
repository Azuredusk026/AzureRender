---
type: "Fragment"
id: __files/entry
title: "仓库根入口与边界契约 / 仓库定位与文档入口"
description: "仓库的完整技术文档在哪里，新增入口该改哪份文件？"
parent: /__files/_overview.md
fragment: entry
entity_names:
  constants:
    - name: CXX_STANDARD
      value: "17"
      source: Project/AzureRender/CMakeLists.txt
    - name: build_dir_ninja_debug
      value: "build/ninja-debug"
      source: README.md
    - name: smoke_frames_example
      value: "120"
      source: README.md
    - name: doc_entry_count
      value: "8"
      source: README.md
retrieval_hints:
  - "仓库的完整技术文档在哪里？"
  - "本地怎么构建并跑起 AzureRender？"
  - "新增一篇主题文档时，根级入口应该改哪份文件？"
  - "本模块也叫仓库根文档 / 根级散落文件，对应需求中的「仓库入口文档」与「根目录配置」"
  - "新增文档入口必须追加到 README.md 的「文档」编号清单，不可另建根级导航文件或目录页"
  - "⚠️ 如果你要找的是渲染器源码、Shader 或 Vulkan 实现细节，不在这里，在 Project 模块"
architectural_role: "仓库对外入口，只承担定位与导航，禁止复制主题文档正文"
---

## Files

- 源码文件：`README.md`（仓库根）
- 关联校验入口：`Project/AzureRender/tools/check_docs.sh`、`.github/workflows/documentation.yml`

## 本子模块解决什么问题

AzureRender 的渲染知识分散在八份主题文档里，读者需要一个稳定的第一跳。根 `README.md` 把"这是什么项目、当前文档在哪、怎么跑起来、资产能用到什么程度"压缩在一屏之内，并把所有技术细节交给主题页，使读者与 Agent 都能用一次链接跳转定位到与问题对应的页面，而不必逐层浏览目录。

## 对外接口

| 入口 | 关键内容 | 业务说明 |
|------|---------|---------|
| `README.md` → 项目总览 | `Project/AzureRender/docs/index.md` | 项目定位与文档体系的第一跳 |
| `README.md` → 构建与使用 | `docs/getting-started.md` | 环境准备、配置与命令行使用 |
| `README.md` → 渲染器架构与 Vulkan | `docs/architecture.md` | 帧资源、同步、所有权与 Renderer SDK |
| `README.md` → 风格化角色渲染 | `docs/character-rendering.md` | 角色材质、Face SDF 与发丝表现 |
| `README.md` → 黑洞模拟 | `docs/blackhole-rendering.md` | 相对论黑洞、TAA 与 Bloom |
| `README.md` → 资产、场景与编辑器 | `docs/assets-and-editor.md` | 资产边界与编辑器工作流 |
| `README.md` → 开发、测试与发布 | `docs/development-and-release.md` | 门禁、测试分层与打包要求 |
| `README.md` → 参数与接口参考 | `docs/reference.md` | 集中式参数与接口速查 |

同一页还给出仓库结构块与快速构建命令：以 `PowerShell` 进入 `Project\AzureRender`，设置 `VULKAN_SDK` 与 `VCPKG_ROOT`，执行 `tools\configure_windows.ps1 -Config Debug` 后构建 `build/ninja-debug`，并用 `--smoke-frames 120` 做冒烟运行。

## 变更风险

- **改链接或锚点会让文档流水线失败**：`.github/workflows/documentation.yml` 由根 `README.md` 变更触发，`Project/AzureRender/tools/check_docs.sh` 逐项检查主题文档的存在性与关键内容锚点。根级入口与主题页文件名不一致时，文档站构建与 CI 文档检查会同时报错，发布被迫中断。
- **在根级复制主题正文会产生双维护入口**：`docs/documentation-standard.md` 规定每项行为只有一个维护入口。一旦把 Vulkan 同步规则、资产命名规则等内容抄到根 README，主题页与根页会分叉，读者与 Agent 会读到互相矛盾的说明，且改动时只更新一处。
- **改写仓库结构块而不同步下游回链**：`Project/AzureRender/docs/index.md`、`Project/AzureRender/README.md` 与 `portfolio/README_CN.md` 都以本页的结构与定位表述为准。结构描述单独变动会让回链指向不存在的层级，克隆者按图索骥找不到目录。
- **漏掉新增主题文档的入口登记**：新增主题页而不更新根级清单，`mkdocs.yml` 导航与文档站会缺少该页入口，主题页成为孤儿文档，检索时不可达。
- **把未实现的能力写进定位段**：根 README 是读者判断项目现状的第一依据，将规划中的功能描述成现有能力会让下游读者、测试与发布说明都按错误前提工作，事后需要逐页纠正。结果描述只区分已验证行为与目标。

## 边界：允许改什么、禁止改什么

允许修改的内容：仓库定位段的措辞与英文摘要、主题文档入口清单的条目顺序与描述文字、仓库结构块内的目录列表、快速构建示例中的预设与冒烟参数。这些都属于导航层信息，改动后只需保证链接指向存在且主题页确实存在。

禁止的改动有三类。第一，禁止在根级复述主题页的技术细节（渲染管线步骤、同步规则、资产导入参数），根页只做导航：由来是 `docs/documentation-standard.md` 规定的「每项行为只有一个维护入口，其他页面通过链接引用」。第二，禁止把待实现的功能写成现有能力，包括在定位段声明尚未验收的子系统；文档站构建与人工复核都以根级描述为基准，虚高一档会连带误导作品集与发布说明。第三，禁止在根级写入本机绝对路径与可再生成文件作为示例；运行时资源不得依赖源码绝对路径，示例中的路径统一使用仓库相对路径或 `<width>`、`<height>` 一类占位形式。

命名与行文约束同样适用：中文完整句子为主，英文仅用于路径、命令与已约定术语；图片与文档中的展示文件名需遵守 `__files_policy.md` 记入的证据命名规约，不使用任务过程名称。

## 实现约束清单

> 实现本模块相关需求时，Agent 必须在动笔前逐条核对以下项。

### 必须定义的常量/枚举
| 标识符 | 值 | 所在文件 | 说明 | 约束由来（如可追溯） |
|-------|----|---------|------|---------------------|
| `CXX_STANDARD` | `17` | `Project/AzureRender/CMakeLists.txt` | 项目语言标准，根级定位描述必须与此一致 | — |
| 默认构建目录 | `build/ninja-debug` | `README.md` | 快速构建示例使用的配置预设目录名 | `chore(release): consolidate project layout and documentation` |
| 冒烟帧数示例 | `120` | `README.md` | 冒烟运行参数示例，与 `.github/workflows/ci.yml` 保持一致 | — |
| 主题文档条目数 | `8` | `README.md` | 根级文档清单条数，与 `mkdocs.yml` 导航逐项对应 | `docs(site): 重构外部技术文档` |

### 设计决策（存在多种可行方案时必填）
| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 技术文档唯一来源 | 主题页 `Project/AzureRender/docs/<topic>.md` | 在根 `README.md` 内展开说明 | 每项行为只保留一个维护入口，根页只做导航，避免两处描述分叉 |
| 根级入口清单位置 | `README.md` 的「文档」编号列表 | 新建独立目录页 | CI 文档检查与文档站构建均以根 README 为入口，另建文件会绕开既有门禁 |
| 构建命令示例位置 | 根 `README.md` 快速构建节 + 详细步骤在 `docs/getting-started.md` | 只写在 `getting-started` | 新读者从仓库首页即可获得可运行命令，不必先跳转才能动手 |
| 中英文表述 | 中文为主，附一段英文摘要 | 全英文 | 公开仓库同时面向中文开发者与英文检索，摘要兼顾两者 |

## 跨模块依赖

> 实现本子模块功能时，除本模块外还需引用的外部模块：

| 依赖模块 | 引用原因 | 关键符号 | confidence |
|---------|---------|---------|------------|
| `Project` | 主题文档、构建脚本与文档检查脚本都位于主工程内 | `tools/check_docs.sh`、`tools/configure_windows.ps1`、`docs/index.md` | extracted |
| `.github` | 根 README 变更触发文档站点发布流水线 | `.github/workflows/documentation.yml` | extracted |

> 反向依赖（谁调用了本子模块）：

| 调用方模块 | 调用场景 | 关键符号 |
|-----------|---------|---------|
| `Project` | 主工程 README 与作品集说明回链根级定位 | `Project/AzureRender/README.md` |
| `.github` | CI 与文档工作流校验根级入口所指路径是否存在 | `tools/check_docs.sh` |

## 典型调用链

### 读者或 Agent 需要构建并跑起渲染器
```
根 README.md「快速构建」节                              ← 本模块入口
  → Project/AzureRender/tools/configure_windows.ps1     ← 跨模块：Project 配置
  → Project/AzureRender/CMakePresets.json:ninja-debug   ← 跨模块：Project 构建预设
  → build/ninja-debug/AzureRender.exe --smoke-frames 120 ← 跨模块：Project 冒烟运行
```

### 需求新增一篇主题文档
```
根 README.md「文档」编号清单追加条目                     ← 本模块入口
  → Project/AzureRender/docs/<topic>.md                 ← 跨模块：Project 主题页
  → Project/AzureRender/mkdocs.yml 导航登记              ← 跨模块：Project 站点导航
  → Project/AzureRender/tools/check_docs.sh 校验         ← 跨模块：Project 门禁
```

> 📄 本节内容来源于仓库内置文档：`README.md`、`AGENTS.md`（原文已提炼，非完整转录）。
