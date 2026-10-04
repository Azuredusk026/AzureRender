---
type: "Module"
id: AzureRender
title: "核心子系统索引"
description: "按职责列出宿主、渲染核心、Renderer SDK、角色、黑洞、场景编辑器、构建发布、门禁与仓库契约九个子系统及其上下游影响。"
kb_path: .codemaker/codeindex/_core_systems.md
---

## 子系统列表

| 子系统 | 职责 | 核心入口 | 上游触发 | 下游影响 |
|--------|------|---------|---------|---------|
| 宿主与帧循环（`Project_host.md`） | 持有全部 Vulkan 对象，推进帧槽、重建交换链，交付 Capture、GPU 计时与 HUD | `AzureRenderFrame.cpp:drawFrame`, `AzureRenderApp.cpp` | `main.cpp` 命令行、窗口输入、编辑器命令 | 三场景共用其帧资源；改公共 Attachment 须回归全部场景 |
| 渲染核心与 RHI（`Project_render_core.md`） | 声明帧内资源并生成屏障，提供 Vulkan 与 Null 双后端，承载 `RenderSettings` v7 | `RenderGraph.cpp:compile`, `VulkanRhi.cpp:recordScene` | 宿主每帧调用 `compile/execute` | 屏障与资源生命周期；改 ID 语义或 Binding 波及所有 Pass |
| Renderer SDK（`Project_renderer_sdk.md`） | 定义 `ISceneRenderer` 生命周期与能力声明，注册并校验扩展 | `ISceneRenderer.hpp`, `ExtensionRegistry.hpp` | 启动时 `BuiltinRendererCatalog` 注册 | 三个内置场景与外部 SDK 用例；生命周期改动须提升 API 版本 |
| 角色渲染（`Project_character.md`） | 材质分类与 Profile、聚簇光照、级联阴影与 PCSS、Face SDF、Hair KK、描边 | `CharacterSceneRenderer.cpp:recordScene`, `shaders/mesh.frag` | `--scene-type character`、`.azscene` 资源与节点 | 角色画面与视觉基线；改 Class 取值会让已发布资产 Profile 失效 |
| 黑洞渲染（`Project_blackhole.md`） | 全屏光线积分、程序化吸积盘、多普勒与红移近似、双 History TAA | `BlackholeSceneRenderer.cpp:recordScene`, `shaders/blackhole.frag` | `--scene-type blackhole`、质量档位与相机预设 | 黑洞基线与 Manifest 档位语义；已冻结，改动须单独立项 |
| 场景数据与编辑器（`Project_scene_editor.md`） | `.azscene` v3 序列化与迁移、层级变换、ECS 快照、ImGui 编辑器与 Undo/Redo | `SceneModel.cpp:save/load`, `EditorSession.cpp:execute` | 编辑器交互、`.azscene` 文件、`--scene` | 渲染快照与点光源世界位置；改字段影响历史文件与全部场景 |
| 构建、资源定位与发布（`Project_build_release.md`） | CMake 目标与安装树、开发/构建/安装三态资源定位、视觉基线与发布门禁 | `tools/run_release_gate.cmake`, `ResourceLocator.cpp` | CMake Presets、CI 工作流、本地打包 | 安装包完整性、资源可发现性与基线可信度 |
| 持续集成与发布门禁（`.github`） | 双平台构建、测试、视觉回归、隐私校验与安装包产出；文档校验与站点发布 | `.github/workflows/ci.yml`, `documentation.yml` | `push` 到 `dev`/`main`、`pull_request`、`workflow_dispatch` | 合并与发布准入；移除门禁步骤会让画质漂移静默通过 |
| 仓库入口与边界契约（`__files`） | 仓库定位与八条文档入口、Agent 工具协约、检出过滤与私有资产边界 | `README.md`, `AGENTS.md`, `.gitignore` | 提交者与 Agent 直接编辑 | 文档可得性、版本库卫生与公开材料授权风险 |

## 关键流程描述

- **帧推进**：`main.cpp` 解析 `--scene-type` → 注册表选中唯一 Renderer → 宿主持有帧槽与交换链 → `drawFrame` 编译 RenderGraph → RHI 录制 → 场景 `recordScene` → 公共 Composite / HUD → Present
- **交换链重建**：`VK_ERROR_OUT_OF_DATE_KHR` 或 resize → 宿主重建 Surface 相关对象 → 回调 `onSwapchainRecreate` → 黑洞在此丢弃时间累积 History
- **确定性捕获**：`--capture-*` → `CaptureRequest` 逐项校验 → 固定帧渲染 → PNG 与 Capture Manifest（含 Renderer 追加字段）→ 作为视觉回归输入
- **设置迁移**：`.azscene` 内嵌 `renderSettings` → `SceneDocument::load` 识别版本 → `migrateRenderSettings` 唯一旧版本入口 → 未识别的未来版本拒绝加载
- **发布验收**：`run_release_gate.cmake` 依次 configure / build / test / install / version / resources / isolated-runtime / package / manifest；Debug 只走开发态阶段并标记 `development-only`
- **编辑器改动**：界面命令 → `EditorSession::execute(EditorCommand)` → 快照写入 Undo 历史（上限 100）→ `SceneDocument::save` 原子替换 → 失败时 `lastError()` 给出原因

> 📄 本节内容来源于仓库内置文档：`docs/architecture.md`、`docs/development-and-release.md`、`docs/assets-and-editor.md`、`docs/reference.md`（原文已提炼）
