---
type: "Module"
id: AzureRender
title: "系统层次与模块边界"
description: "描述交付门禁层、引擎工程层与仓库契约层的划分、模块边界规则、四条核心数据流与系统架构约束。"
kb_path: .codemaker/codeindex/_architecture.md
---

## 系统层次划分

| 层次 | 载体 | 职责 | 不承担 |
|------|------|------|--------|
| 契约层 | 根 `README.md`、`AGENTS.md`、`.gitignore`（`__files`） | 仓库定位、文档入口清单、检出过滤与私有资产边界 | 任何渲染运行时逻辑 |
| 交付门禁层 | `.github/workflows/ci.yml`、`documentation.yml` | 决定改动能否合并与发布，执行构建、测试、门禁与站点发布 | 校验逻辑本身，全部委托 `Project/AzureRender/tools` |
| 工程层 | `Project/AzureRender` | 宿主、渲染核心、场景 Renderer、编辑器、资产管线、测试与发布脚本 | 不定义仓库对外定位，不含私有资产 |

工程层内部分五段：宿主 `src/app` → 图与资源 `src/render` → 后端 `src/rhi` → 场景算法 `src/scenes` → 数据与交互 `src/editor`、`src/scene`、`src/ecs`；扩展边界由 `src/extensions` 与 `src/scenes/BuiltinRendererCatalog` 定义。

## 模块边界规则

| 规则 | 内容 | 破坏后果 |
|------|------|---------|
| 场景 Renderer 只用宿主借出的对象 | 自有 Pipeline / Descriptor / Buffer / Image 全在 `onLoad` 创建、`onUnload` 释放 | 句柄悬空或重复释放 |
| 每帧只驱动一个 Renderer | 按 `RenderSettings::sceneType` 选中唯一 Renderer，类型分支不进宿主 | 两场景同时写公共 Attachment |
| 新增场景必须走注册 | 扩展 `SceneType` 并在 `BuiltinRendererCatalog` 注册，经 `ExtensionRegistry` 校验 | 能力声明与依赖校验失效 |
| 帧内资源归 RenderGraph | 只经 `addResource` / `importImage` 声明 | 屏障漏生成，偶发闪烁或黑图 |
| 验收判据只在交付层 | 视觉回归容差与门禁阶段序列定义在 `tools/` | 门禁失去独立参照 |
| 公共与私有资产分离 | `assets_public/` 可进 CI 与安装包，`assets_private/`、`captures/` 一律排除 | 许可与凭据风险不可回退 |

## 核心数据流

**启动与主循环**：`main.cpp` 解析命令行 → `SceneRendererRegistry` 选中 Renderer → `AzureRenderApp` 创建 Vulkan 对象 → `AzureRenderFrame.cpp:drawFrame` → `RenderGraph.cpp:compile/execute` → `VulkanRhi.cpp:recordScene` → 场景 `recordScene` → Composite / HUD / Capture。

**画质设置与场景文档**：`RenderSettings` v7 与 `.azscene` v3 内嵌 `renderSettings` 双向对应 → `SceneDocument::load`（含 `migrateRenderSettings`）→ `EditorContext` → 每帧 `SceneDescription` / `SceneInstance` 快照 → Renderer 消费。

**确定性与证据**：CLI `--capture-*` → `CaptureRequest` 校验尺寸 64..7680 × 64..4320、fps 1..240 → 固定帧 Capture → PNG 与 Capture Manifest → `run_visual_regression.py` 比对 `assets_public/baselines` → `ci.yml` 门禁结论。

**文档交付**：根 `README.md` 八条主题入口 → `docs/*.md` 唯一维护入口 → `mkdocs.yml` nav → `check_docs.sh` 与 `check_doc_style.py` → `documentation.yml` 发布 Pages。

## 系统架构约束

- [跨模块禁忌] 编辑器与 GUI 禁止直接修改 Descriptor、Pipeline 或 Vulkan 资源，必须经宿主与 Renderer SDK → 否则所有权与重建顺序不可回答
- [跨模块禁忌] Renderer 禁止销毁 `RenderContext` 借出的 Handle，也不得假定 `sceneFramebuffer` 跨帧有效 → 交换链重建后会出现悬空引用
- [跨模块禁忌] 新增场景类型必须扩展 `SceneType` 并在 `BuiltinRendererCatalog` 注册，禁止在宿主加类型分支 → 绕过能力声明会静默缺少 Attachment
- [跨模块禁忌] 角色专用光照与材质逻辑只能落在 `shaders/mesh.*` 与 `CharacterSceneRenderer.*`，禁止为单套资产写 Shader 分支 → 资产命名会渗入着色器
- [数据流方向] 输入 → 宿主 → RenderGraph → RHI → 场景 Renderer 单向，场景不得反向驱动帧循环 → 反向调用破坏帧槽与屏障时序
- [数据流方向] Base Color 与 Emissive 允许 sRGB 解码，Normal / HN / 打包数据禁止 → 误解码使材质整体偏灰
- [数据流方向] 场景侧已是线性 HDR，禁止在 Shader 内再次 `pow(color, 1/2.2)` → 重复 Gamma 造成整体洗白
- [性能红线] 普通帧禁止 `vkQueueWaitIdle`；`executeOneShot`、`copyBuffer`、`generateMipmaps` 只允许资源加载期调用 → 每帧调用退化为同步等待
- [性能红线] Material Push Constant 固定 128 bytes，继续扩展必须迁移 UBO/SSBO → 超出即 Pipeline 创建失败
- [性能红线] 每帧上传走 `UploadRingBuffer` 单一持久映射缓冲分段复用，禁止每帧建多个小 Uniform Buffer → 否则分配压力与句柄数失控
- [契约版本] `RenderSettings` v7、`.azscene` v3、Renderer SDK 与 `SceneRendererCapabilities` API 1，语义不兼容才递增 → 递增须同步 Loader、Schema、迁移函数与文档
- [验收口径] `ctest` 通过不代表 GPU 验收通过，须配合 Debug Validation Smoke、固定 Capture 与人工视觉对照 → 单元测试不创建完整可见 Vulkan 场景
- [交付边界] `assets_private/` 与 `captures/` 不得进入版本库、CI、安装包与作品集 → 由 `run_release_gate.cmake` 与 `verify_portfolio.ps1` 拒绝
- [文档边界] 每项行为只有一个维护入口，根级文档只做导航，新页面必须挂进 `mkdocs.yml` nav → 未挂载页面在站点不可达且 `--strict` 不报错

> 📄 本节内容来源于仓库内置文档：`docs/architecture.md`、`docs/development-and-release.md`、`docs/assets-and-editor.md`、`docs/documentation-standard.md`（原文已提炼）
