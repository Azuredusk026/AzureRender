---
type: "Fragment"
id: Project/host
title: "Vulkan 宿主与帧循环"
description: "宿主如何持有 Vulkan 对象、推进帧槽、重建交换链，并把渲染结果交付给 Capture、Timing 与 HUD。"
parent: /Project/_overview.md
fragment: host
entity_names:
  constants:
    - name: kMaxFramesInFlight
      value: "2"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: kShadowMapSize
      value: "2048"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: kTimestampQueryCount
      value: "4"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: kMaxHudVertices
      value: "24576"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: PostProcessPushConstants size
      value: "80"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: HudVertex size
      value: "12"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: EasyFontVertex size
      value: "16"
      source: Project/AzureRender/src/app/AzureRenderFrame.cpp
    - name: kEnableValidation
      value: "true（Debug）/ false（Release）"
      source: Project/AzureRender/src/app/AzureRenderApp.hpp
    - name: CommandLineErrorCode::UnknownOption
      value: "1"
      source: Project/AzureRender/src/app/CommandLine.hpp
    - name: CommandLineErrorCode::MissingValue
      value: "2"
      source: Project/AzureRender/src/app/CommandLine.hpp
    - name: CommandLineErrorCode::InvalidValue
      value: "3"
      source: Project/AzureRender/src/app/CommandLine.hpp
    - name: CommandLineErrorCode::InvalidCombination
      value: "4"
      source: Project/AzureRender/src/app/CommandLine.hpp
    - name: DiagnosticCode::InvalidArguments
      value: "2"
      source: Project/AzureRender/src/diagnostics/RuntimeDiagnostics.hpp
    - name: DiagnosticCode::Asset
      value: "3"
      source: Project/AzureRender/src/diagnostics/RuntimeDiagnostics.hpp
    - name: DiagnosticCode::VulkanInitialization
      value: "4"
      source: Project/AzureRender/src/diagnostics/RuntimeDiagnostics.hpp
    - name: DiagnosticCode::Runtime
      value: "5"
      source: Project/AzureRender/src/diagnostics/RuntimeDiagnostics.hpp
retrieval_hints:
  - "一帧是怎么从 Acquire 走到 Present 的？"
  - "窗口 resize 或 VK_ERROR_OUT_OF_DATE_KHR 之后哪些对象要重建？"
  - "Capture 的 PNG 是在哪一步编码的，Manifest 写了哪些字段？"
  - "GPU Timing 的 Timestamp Query 池在哪里创建，为什么不含 CPU 与 Present？"
  - "命令行参数校验失败返回哪个退出码？"
  - "⚠️ 你要找的是 Pass 执行顺序与屏障生成，不在这里，在 Project_render_core"
  - "⚠️ 你要找的是角色描边、Toon Ramp 或 Face SDF 效果，不在这里，在 Project_character"
  - "本模块也叫宿主 / Vulkan Host / App 层，需求里的『运行时』『截图』『诊断视图开关』归这里"
  - "新增帧内公共 Pass（后处理、HUD、Present 前的复制）必须写在 AzureRenderFrame.cpp 的公共流程，不得放进某个 Scene Renderer"
architectural_role: "引擎宿主层，持有全部 Vulkan 对象，禁止场景 Renderer 销毁其句柄"
---

## 业务意图

宿主层解决的是"多个画面风格完全不同的场景共用一套 GPU 资源与帧节奏，并且输出结果可复现"的问题。设备、交换链、HDR Scene Color、Depth、Normal、Shadow Map、最终合成、Capture、GPU Timing、HUD 与编辑器 UI Pass 都由它统一持有，主帧循环里不含任何角色或黑洞分支，新增一种场景不需要修改这里。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `AzureRenderApp::run` | main→宿主 | `AzureRenderOptions` | 完成 Instance 到帧循环的全部初始化并按 `--smoke-frames`/Capture 条件退出 | `src/app/AzureRenderApp.cpp:run` |
| `SceneFrameData` | 宿主→Renderer | `deltaSeconds`, `timeSeconds`, `renderSettings`, `camera*`, `qaIsolationMode`, `captureFps`, `technicalSequenceChapter`, `swapchainWidth/Height` | 每帧下发的主输入，宿主是唯一持有者 | `src/render/RenderContext.hpp:SceneFrameData` |
| `RenderContext` | 宿主→Renderer（借用） | `device`, `graphicsQueue`, `commandPool`, `allocator`, `rhi`, `commands`, `sceneFramebuffer`, `shadowRenderPass`, `timestampQueryPool`, `scene` | 只读借用面；除 `commandBuffer`/`commands`/`sceneFramebuffer` 外均跨帧有效 | `src/render/RenderContext.hpp:RenderContext` |
| `RendererSceneState` | Renderer→宿主（可选 Hook） | `asset`, `modelMatrix`(16 float), `selectedPrimitiveIndex`, `primitiveCount` | 供编辑器 Picking/Gizmo/HUD 读取；无可拾取几何的 Renderer 返回 `nullptr` | `src/render/RenderContext.hpp:RendererSceneState` |
| `SceneSubmissionCounters` | Renderer→宿主（可选） | `drawCalls`, `descriptorSetBinds`, `pipelineBinds`, `pushConstantUpdates` | 性能基线用的 CPU 侧提交计数，指针为 null 表示未采集 | `src/render/RenderContext.hpp:SceneSubmissionCounters` |
| Capture Manifest | 宿主→磁盘 | 尺寸、帧、场景、RenderSettings、诊断状态、Renderer 自定义字段 | 让一张 PNG 可还原生成条件 | `src/app/AzureRenderCapture.cpp:writeCaptureManifest` |
| GPU Timing JSON | 宿主→磁盘 | 各 Pass 毫秒、帧样本序列 | 仅统计 GPU 时间，报告必须写明 Debug/Release 与 Render Path | `src/app/AzureRenderCapture.cpp` |
| `parseCommandLine` | CLI→宿主 | `ParsedCommandLine` | 非法输入抛 `CommandLineError`，Usage 输出并以退出码 2 结束 | `src/app/CommandLine.cpp:parseCommandLine` |
| `RuntimeDiagnostics` | 全模块→日志 | `DiagnosticLevel`, `DiagnosticCode` | Validation 消息与资产/Vulkan 错误统一成结构化事件流 | `src/diagnostics/RuntimeDiagnostics.cpp` |

## 跨模块依赖

> 本子系统的直接依赖（`Project` 内部按 ownership 计）：

| 依赖 | 引用原因 | 关键符号 | confidence |
|---------|---------|---------|------------|
| `Project_render_core` | 公共 Pass 的屏障与执行顺序由 RenderGraph 编译产出 | `RenderGraph::compile`, `RenderGraph::execute` | extracted |
| `Project_render_core` | 设置与 Look Catalog 决定合成参数 | `RenderSettings`, `applyShowcasePresetLook` | extracted |
| `Project_renderer_sdk` | 每帧按 `sceneType` 取一个 Renderer 并驱动生命周期 | `SceneRendererRegistry::create`, `ISceneRenderer::recordScene` | extracted |
| `Project_scene_editor` | 编辑器视口纹理、会话命令与拾取状态由宿主转发 | `EditorSession::execute`, `pickPrimitive` | extracted |
| `src/platform` | GLFW 只提供窗口、输入与 Surface 扩展，不接管帧同步 | `GlfwFrontend`, `waitForDrawableSurface` | extracted |
| `src/rhi` | `VulkanRhi` 是生产后端，`UploadRingBuffer`/`GpuAllocator` 由宿主创建 | `rhi::VulkanRhi`, `rhi::GpuAllocator` | extracted |
| `third_party/imgui`（1.92.8，docking） | 编辑器 UI Pass | `ImGuiEditorLayer` | extracted |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|-----------|---------|---------|
| `src/main.cpp` | 进程入口，先校验 CLI 再建宿主 | `parseCommandLine`, `AzureRenderApp::run` |
| 工具与 CI | smoke、Capture、性能与视觉回归全部通过宿主参数驱动 | `--smoke-frames`, `--capture-dir`, `--gpu-timing` |
| 场景 Renderer | 通过 `RenderContext` 借用宿主对象，不反向持有 | `RenderContext::allocator`, `RenderContext::commands` |

## 典型调用链

### 普通帧

```
src/main.cpp:main → parseCommandLine → AzureRenderApp::run
  → AzureRenderApp.cpp:createInstance/createSurface/pickPhysicalDevice/createLogicalDevice
  → AzureRenderApp.cpp:createSwapchain → create*Resources → createRenderPass → createGraphicsPipeline
  → BuiltinRendererCatalog::createRegistry → SceneRendererRegistry::create   ← 本模块入口
  → ISceneRenderer::capabilities → validateSceneRendererCapabilities → onLoad
  → AzureRenderFrame.cpp:drawFrame
    → vkWaitForFences → vkAcquireNextImageKHR → vkResetFences
    → RecordFrameTasks（FrameTaskScheduler 确定性顺序）
    → uploadRing.beginFrame(currentFrame)
    → ISceneRenderer::updateFrame(SceneFrameData)
    → RenderGraph::compile → RenderGraph::execute                            ← 跨子系统：render_core
    → ISceneRenderer::recordScene → 公共 Composite/HUD 录制
    → vkQueueSubmit → vkQueuePresentKHR → currentFrame_ = (currentFrame_+1) % kMaxFramesInFlight
```

### 交换链重建

```
framebufferResizeCallback / VK_ERROR_OUT_OF_DATE_KHR
  → AzureRenderApp.cpp:recreateSwapchain
    → waitForDrawableSurface(frontend)            ← 尺寸非零才继续；期间应关闭则退出
    → vkDeviceWaitIdle → cleanupSwapchain
    → createSwapchain → create*Resources → create*Framebuffers → recreateEditorViewportResources
    → ISceneRenderer::onSwapchainRecreate          ← 黑洞 History 在此失效
```

## 实现约束清单

> 改动宿主前逐条核对；每条都是接口契约或验收后果。

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------------------|
| `kMaxFramesInFlight` | `2` | `AzureRenderApp.hpp` | 帧槽数量；Uniform/Joint/HUD/上传环按此分段 | 缺省即默认在飞帧数，改动会连锁影响 `UploadRingBuffer` 分段与 Renderer 私有帧数组 |
| `kShadowMapSize` | `2048` | `AzureRenderApp.hpp` | 宿主阴影图固定边长并写入 `RenderContext::shadowMapSize` | 级联图集与 PCSS texel 半径以此为单位 |
| `kTimestampQueryCount` | `4` | `AzureRenderApp.hpp` | 每帧时间戳数：Renderer 写 0..2，宿主写最终 Composite | 少于 4 会让 Timing 报告缺 Pass 边界 |
| `kMaxHudVertices` | `24576` | `AzureRenderApp.hpp` | HUD 顶点上限，超出即截断 | 防止诊断文本挤占上传环容量 |
| `PostProcessPushConstants` | `80` bytes | `AzureRenderApp.hpp` | `static_assert` 锁定合成 Push Constant 尺寸 | 与 `shaders/*frag` 的 push block 必须逐字段一致 |
| `CommandLineErrorCode` | `1..4` | `CommandLine.hpp` | Usage/值缺失/值非法/组合冲突分类 | CLI 契约测试与 `docs/reference.md` 按这些语义断言 |
| `DiagnosticCode` | `2/3/4/5` | `RuntimeDiagnostics.hpp` | 参数、资产、Vulkan 初始化、运行期分类 | `diagnosticExitCode` 据此映射退出码，CI 依赖其稳定性 |

### 必须实现的函数

| 函数名 | 所在文件 | 说明 |
|--------|---------|------|
| `validateSceneRendererCapabilities` | `render/RenderContext.hpp` | `diagnosticViewNames` 非空且第 0 项必须是 `Beauty`，否则 `onLoad` 前抛异常 |
| `waitForDrawableSurface` | `platform/SurfaceLifecycle.hpp` | 最小化时以 `waitEvents` 等待有效尺寸，收到关闭请求即终止重建，不做忙等 |
| `cleanup` / `cleanupSwapchain` | `app/AzureRenderApp.cpp` | 销毁顺序必须是创建的依赖倒序：Framebuffer → ImageView → Image/DeviceMemory；Pipeline 先于其 Layout |
| `writeCaptureManifest` | `app/AzureRenderCapture.cpp` | Manifest 必须可还原尺寸、帧、设置与诊断状态，且不得覆盖已有输出 |

### 边界约束（能做 / 禁止）

- 允许：在帧边界、退出、Swapchain 重建与编辑器显式热重载处等待 Device Idle。
- 禁止：普通帧调用 `vkQueueWaitIdle` → 破坏 CPU/GPU 并行，性能基线失真（来源：`docs/development-and-release.md`，同见 `docs/runtime/rhi-synchronization.md`）。
- 禁止：Renderer 销毁 `RenderContext` 借出的 Device、Queue、Command Pool、Render Pass、Framebuffer、Sampler、Query Pool，或缓存跨帧有效的 `sceneFramebuffer` 与 `commandBuffer` → 会造成 use-after-destroy 或 Validation 报旧对象。
- 禁止：宿主按场景类型写分支（`if sceneType == character`）→ 场景数增长时帧循环退化为特判集合，破坏"多场景共用宿主"的架构目标。
- 禁止：运行时资源依赖源码绝对路径；EXE 必须能在 `build/install-<config>/bin` 下自定位（`--check-resources` 验证），单独复制 EXE 不受支持。
- 禁止：Release 默认启用 Validation Layer，也禁止为消除警告而全局降低编译等级。
- 边界：`capabilities()` 返回值对象，`diagnosticViewName` 返回的 `string_view` 不得指回该临时量（来源 commit：`fix(build): 修复MSVC工具链构建与诊断视图名悬空引用`）。

### 设计决策

| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 帧同步粒度 | 帧槽（`currentFrame_`）+ Fence/Semaphore | 每帧 `vkDeviceWaitIdle` | 保留 CPU/GPU 并行；`currentFrame_` 与 `imageIndex` 语义独立，Framebuffer 按 imageIndex 选、缓冲按帧槽选 |
| Vulkan 对象持有 | `AzureRenderApp` 直接持有，实现拆到多个 `.cpp` | 拆出独立 Device/Swapchain 封装类 | 保持"谁创建谁销毁"的单点可追溯；`app` 的多文件不改变所有权 |
| HUD 顶点 | `HudVertex`(12B)/`EasyFontVertex`(16B) 两套布局 | 统一一套 | 编辑器与运行时字体度量不同，`static_assert` 锁定后可独立调整 |
| Validation 开关 | 编译期常量 `kEnableValidation` | 运行期参数 | 避免发布包要求用户安装 SDK Layer |

## 变更风险

- 改公共 Attachment 格式、`SceneRendererCapabilities` 或 `RenderContext` 字段 → 三个内置 Renderer 同时受影响，必须回归 Character、Blackhole、Sample 三场景。
- 改 `ISceneRenderer` 生命周期顺序或新增必经 Hook → Renderer SDK 契约版本需提升（当前 API 1），否则已注册场景在新宿主动作下静默不生效。
- 改帧槽数量、上传环分段或 Push Constant 布局 → 表现为部分驱动上的黑图/错采样/Validation Error，单元测试无法覆盖，需 Debug Validation + 固定 Capture。
- 改 `CommandLine.cpp` 的参数值域或互斥关系 → 视觉回归与发布门禁脚本按既定 CLI 契约调用，CI 会失败。

## 附：内置文档摘要

- 帧执行序列、Vulkan 对象职责表与同步症状对照见 `docs/architecture.md`（本文只保留约束，未转录 API 教程）。
- 宿主/Renderer 责任划分、Debug 与 Release 差异、发布验收矩阵见 `docs/development-and-release.md`。
- 阅读入口顺序：`AzureRenderApp.cpp` → `AzureRenderFrame.cpp::drawFrame` → `AzureRenderDescriptors.cpp` → `AzureRenderPipeline.cpp`。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/architecture.md`、`Project/AzureRender/docs/development-and-release.md`、`Project/AzureRender/docs/runtime/rhi-synchronization.md`（原文已提炼，非完整转录）
