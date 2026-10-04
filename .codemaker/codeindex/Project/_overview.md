---
type: "Module"
id: Project
title: "渲染引擎工程"
description: "AzureRender 引擎工程主体，用原生 Vulkan 实现风格化角色与黑洞两条渲染路径，并向编辑器、Capture 与发布门禁提供统一宿主。"
module_id: Project
architectural_role: "引擎工程主体（Vulkan 宿主 + 渲染核心 + 场景 Renderer + 编辑器 + 工具链）"
world_model_hints:
  - "仓库 depth-1 扫描下的唯一实体工程目录，Project/AzureRender 即整个引擎"
  - "上层由命令行、.azscene 文档与编辑器交互触发，下层直连 Vulkan 驱动"
  - "仓库根 .github 工作流与根 README/AGENTS.md 只作门禁与写作约束引用，无源码级依赖"
upstream_modules:
  - module: "."
    confidence: extracted
  - module: .github
    confidence: extracted
downstream_modules:
  - module: .github
    confidence: extracted
  - module: "."
    confidence: inferred
---

## Files

### 源代码路径

- `Project/AzureRender/src`（C++17：app / render / rhi / scene / ecs / editor / extensions / scenes / assets / resources / platform / diagnostics）
- `Project/AzureRender/shaders`（21 个 GLSL，由 CMake 调 `glslc` 编为 SPIR-V）
- `Project/AzureRender/tests`（23 个不依赖可见窗口的契约测试）
- `Project/AzureRender/tools`（配置、资产处理、视觉 QA、发布门禁）
- `Project/AzureRender/schemas`、`assets_public`、`assets_private`、`assets_placeholder`、`portfolio`、`captures`
- `Project/AzureRender/docs`（面向读者的现行文档，本知识库的内置摘要来源）
- `Project/AzureRender/CMakeLists.txt`、`CMakePresets.json`、`mkdocs.yml`、`vcpkg.json`、`azurerender-editor.ini`

### 知识库文档

- `.codemaker/codeindex/Project/_overview.md`（本文件）
- `.codemaker/codeindex/Project/Project_host.md`
- `.codemaker/codeindex/Project/Project_render_core.md`
- `.codemaker/codeindex/Project/Project_scene_editor.md`
- `.codemaker/codeindex/Project/Project_renderer_sdk.md`
- `.codemaker/codeindex/Project/Project_character.md`
- `.codemaker/codeindex/Project/Project_blackhole.md`
- `.codemaker/codeindex/Project/Project_build_release.md`

### 文件组成与归属

> 本表用于确认覆盖率：每个源文件至少属于一个子文档。符号级定位由 Codemap 实时提供。

| 范围 | 文件 | 归属子文档 |
|------|------|-----------|
| 应用与平台 | `src/main.cpp`、`src/app/AzureRenderApp.*`、`AzureRenderFrame.cpp`、`AzureRenderCapture.cpp`、`AzureRenderDescriptors.cpp`、`AzureRenderPipeline.cpp`、`AzureRenderResources.cpp`、`AzureRenderSupport.cpp`、`AzureRenderInternal.hpp`、`AzureRenderOptions.hpp`、`CommandLine.*`、`src/platform/GlfwFrontend.*`、`SurfaceLifecycle.hpp`、`BinaryFile.hpp`、`src/diagnostics/RuntimeDiagnostics.*`、`GpuCapabilityReport.*`、`schemas/gpu_capability_report.schema.json` | `Project_host.md` |
| 渲染核心与 RHI | `src/render/RenderGraph.*`、`RenderContext.hpp`、`RenderSettings.*`、`RendererCore.*`、`ComputePass.*`、`TransientResourcePool.*`、`FrameTaskScheduler.hpp`、`ClusteredLightGrid.hpp`、`CascadedShadow.hpp`、`LightBuffer.hpp`、`EnvironmentAsset.*`、`src/rhi/Rhi.hpp`、`IGpuAllocator.hpp`、`GpuAllocator.*`、`VulkanMemoryAllocatorImpl.cpp`、`VulkanRhi.*`、`NullRhi.*`、`RingFrameAllocator.hpp`、`UploadRingBuffer.*`，以及公共 Shader `background.*`、`hud.*`、`inner_outline.*`、`clear.comp`、`bloom_downsample.comp`、`bloom_composite.comp` | `Project_render_core.md` |
| 场景数据与编辑器 | `src/scene/SceneDescription.hpp`、`SceneComponents.hpp`、`TransformSystem.hpp`、`TransformMath.hpp`、`Frustum.hpp`、`RenderBatching.hpp`、`src/ecs/Entity.hpp`、`World.hpp`、`IComponentArray.hpp`、`ComponentArray.hpp`、`Components.hpp`、`src/editor/SceneModel.*`、`EditorContext.*`、`EditorSession.*`、`EditorCameraController.*`、`ImGuiEditorLayer.*`、`IEditorPanel.hpp`、`azurerender-editor.ini`（ImGui dock 布局，本机文件） | `Project_scene_editor.md` |
| 扩展与注册 | `src/extensions/ISceneRenderer.hpp`、`ExtensionRegistry.hpp`、`SceneType.hpp`、`IRenderFeature.hpp`、`IAssetImporter.hpp`、`src/scenes/BuiltinRendererCatalog.*`、`SampleSceneRenderer.*` | `Project_renderer_sdk.md` |
| 角色与资产 | `src/scenes/CharacterSceneRenderer.*`、`src/assets/GltfLoader.*`、`schemas/azure_render_material.schema.json`、`assets_public/toon_ramp_profiles.json`、`toon_ramp_atlas.ppm`、`face_sdf_v1.png`、`showcase_looks.json`、`test_model.gltf`、`test_env*.hdr`、`toon_ramp_atlas.ppm`、`assets_public/scenes/*.azscene`，Shader `mesh.*`、`shadow.*`、`outline.*`、`skin.comp`、`ibl_prefilter.comp` | `Project_character.md` |
| 黑洞 | `src/scenes/BlackholeSceneRenderer.*`、Shader `blackhole.vert/frag`、`blackhole_taa.frag`、`blackhole_composite.frag` | `Project_blackhole.md` |
| 构建与交付 | `CMakeLists.txt`、`CMakePresets.json`、`vcpkg.json`、`requirements-docs.txt`、`mkdocs.yml`、`THIRD_PARTY_NOTICES.md`、`src/resources/ResourceLocator.*`、`tools/*`（含 `configure_windows.ps1`、`msvc_env.bat`、`fix_msvc_deps_prefix.py`、`check_windows_surface.py`、`run_release_gate.cmake`、`write_rc_manifest.cmake`、`write_install_manifest.cmake`、`verify_install_manifest.cmake`、`test_install_manifest_roundtrip.cmake`、`run_performance_baseline.py`、`run_clustered_light_performance.py`、`run_blackhole_compute_smoke.ps1`、`run_character_ibl_smoke.ps1`、`run_character_skinning_smoke.ps1`、`run_character_qa.ps1`、`run_clustered_lighting_smoke.py`、`run_morph_compute_regression.py`、`build_image_comparison_sheet.py`、`build_face_sdf.py`、`bc5_normal_png.js`、`inject_gltf_textures.js`、`inject_gltf_idle_animation.js`、`unreal_export_laevat.py`、`unreal_export_laevat_matcap.py`、`unreal_export_laevat_skinned.py`、`unreal_extract_laevat_textures.py`、`unreal_list_laevat_animations.py`、`unreal_trace_cloth_material.py`、`unreal_trace_hair_material.py`）、`tests/*`、`portfolio/`、`assets_placeholder/`、`assets_private/`、`showcase/`、`Testing/`（CTest 临时目录，不入库） | `Project_build_release.md` |
| 文档 | `docs/architecture.md`、`getting-started.md`、`reference.md`、`development-and-release.md`、`character-rendering.md`、`blackhole-rendering.md`、`assets-and-editor.md`、`documentation-standard.md`、`index.md`、`docs/runtime/*.md`、`docs/acceptance/{r0,r1,r2}/*.md`、`docs/audits/*.html`、`docs/plans/*.md`、`CHANGELOG.md`、`README.md` | 按主题归入对应子文档的 `## 附：内置文档摘要` |

### 符号索引

- 由 **Codemap MCP** 实时提供（`find_symbol` / `search_code` / `get_symbol_detail`）

## 子文档速览

| 子文档 | 覆盖内容 | 关键实体 |
|--------|---------|---------|
| `Project_host.md` | Vulkan 宿主、帧循环、Swapchain 重建、Capture、GPU Timing、HUD、诊断退出码 | `kMaxFramesInFlight`, `kShadowMapSize`, `CommandLineErrorCode`, `DiagnosticCode` |
| `Project_render_core.md` | RenderGraph 编译、RHI 双后端、同步屏障、GPU 内存与上传环、RenderSettings v7 | `RenderSettings::kSchemaVersion`, `ImageBarrierDesc`, `UploadRingBuffer`, `TransientResourcePool` |
| `Project_scene_editor.md` | `.azscene` 序列化与迁移、层级变换解析、ECS 快照、ImGui 编辑器与 Undo/Redo | `SceneDocument::kSchemaVersion`, `kHistoryCapacity`, `kInvalidEntity`, `EditorCommand` |
| `Project_renderer_sdk.md` | `ISceneRenderer` 生命周期与能力声明、Registry 校验、Shader Feature 目录 | `SceneRendererCapabilities::kApiVersion`, `ExtensionRegistry::kApiVersion`, `SceneType` |
| `Project_character.md` | 材质分类与 Profile、聚簇光照、级联阴影与 PCSS、Face SDF、Hair KK、描边、眉毛 Overlay | `kClusterGridX/Y/Z`, `kMaxSceneLights`, `kShadowCascadeCount`, `AssetMaterialClass`, `MaterialPushConstants` |
| `Project_blackhole.md` | Schwarzschild 近似积分、吸积盘周期噪声、多普勒与红移、双 History TAA、质量档位 | `BlackholeQuality`, `BlackholeCamera`, `kBloomLevelCount`, `BlackholeQualityParameters` |
| `Project_build_release.md` | CMake 目标与安装树、资源定位、视觉回归基线、发布门禁与私有资产边界 | `AZURERENDER_VERSION`, `AZURERENDER_ENABLE_VALIDATION`, 视觉回归容差 |

## 模块概述

本模块承载 AzureRender 引擎的全部可交付实现：让风格化角色与黑洞模拟共用同一个 Vulkan 宿主，使画面结果可被确定性捕获、被结构化诊断解释、并被安装树与发布门禁复现，解决的是"同一套宿主如何长期稳定产出可验证画面"的问题，而不是提供一组可复用的图形代码。
上游：`main.cpp` 解析命令行（`--scene-type`、`--scene`、`--editor`、`--capture-*`、`--qa-*`）后经 `SceneRendererRegistry` 选中一个 Renderer；窗口输入、编辑器命令与 CI 的 smoke/回归脚本共同触发帧推进。
下游：决定 `RenderSettings`/`.azscene`/Material Profile/Renderer SDK 等数据契约的兼容性，决定 `portfolio/` 证据与安装树内容是否成立；任何 Shader Binding、公共 Attachment 或颜色空间改动的可见后果直接体现在角色与黑洞画面上。

## 架构简析

**分层结构（单行）：** CLI/窗口/编辑器输入:`src/main.cpp` → 应用宿主:`src/app/AzureRenderApp.cpp` → 帧循环与公共后处理:`src/app/AzureRenderFrame.cpp`（`drawFrame`）→ Pass 顺序编译与屏障:`src/render/RenderGraph.cpp:compile/execute`（`execute`）→ 资源与录制后端:`src/rhi/VulkanRhi.cpp:recordScene` → 场景 Pass:`src/scenes/CharacterSceneRenderer.cpp:recordScene` / `BlackholeSceneRenderer.cpp:recordScene` → 最终 Composite/HUD/Capture:`src/app/AzureRenderFrame.cpp:drawFrame`

- 宿主与 Renderer 之间只允许 `ISceneRenderer` + `RenderContext` + `SceneFrameData` 三个接口面，主帧循环不含场景分支。
- 全部 GPU 显存经宿主持有的 `rhi::IGpuAllocator`（VMA 实现）；Renderer 借用，不建私有分配器。
- 每个 Vulkan Handle 必须能回答：谁创建、谁拥有、GPU 何时不再使用、由谁销毁；销毁顺序与创建依赖相反。
- 核心文件：`AzureRenderApp.hpp`（宿主句柄与帧槽）、`RenderContext.hpp`（借用契约与能力声明）、`RenderGraph.cpp`（Pass 顺序与屏障）、`SceneModel.cpp`（`.azscene` 读写与原子替换）、`RenderSettings.cpp`（v7 设置、迁移与 Look Catalog）。

## 上下游关系

> `extracted` = 静态分析可信；`inferred` = Agent 推断待复核

| 方向 | 对端 | 说明 | confidence |
|------|------|------|------------|
| 上游 | `.`（仓库根） | 根 README 指向本工程；根 `.gitignore` 决定 `build/`、`captures/`、`assets_private/` 不入库 | extracted |
| 上游 | `.github` | 仓库根 `ci.yml` / `documentation.yml` 与工程内 `docs.yml` 触发严格构建、mkdocs `--strict` 与 CTest | extracted |
| 下游 | `assets_public` / `portfolio` | 视觉基线、Capture 与作品集证据由本工程生成 | extracted |
| 下游 | `docs/` 文档站 | `documentation-standard.md` 约束文档同步，接口变更后文档漂移即为缺陷 | inferred |

## 质量门槛提示

单元测试不创建完整可见 Vulkan 场景，Shader 与驱动路径必须由 Debug Validation Smoke、固定 Capture 与人工视觉对照共同覆盖；`ctest` 通过不代表 GPU 验收通过。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/README.md`、`Project/AzureRender/docs/index.md`、`Project/AzureRender/docs/architecture.md`（原文已提炼，非完整转录）
