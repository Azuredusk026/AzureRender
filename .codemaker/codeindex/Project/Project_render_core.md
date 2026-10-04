---
type: "Fragment"
id: Project/render_core
title: "渲染核心与 RHI"
description: "RenderGraph 如何声明资源读写并生成屏障，RHI 双后端如何承接资源创建与命令录制，RenderSettings 携带哪些版本化画面参数。"
parent: /Project/_overview.md
fragment: render_core
entity_names:
  constants:
    - name: RenderSettings::kSchemaVersion
      value: "7"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: kShowcasePresetVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: FaceSdfSettings::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: BloomSettings::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: OutlineSettings::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: GradeSettings::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: ShadowSettings::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: BlackholeSettings::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: RenderPath
      value: "Traditional=0 / Subpasses=1 / DynamicRendering=2"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: bloom.threshold / bloom.strength
      value: "1.05 / 0.16"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: outline.strength / depthThreshold / normalThreshold
      value: "0.40 / 0.18 / 0.20"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: shadow.maximumFilterRadiusTexels
      value: "8.0"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: styleMaskStrength / diffuseBandThreshold
      value: "1.0 / 0.40"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: RenderGraph::ResourceId / PassId
      value: "std::uint32_t"
      source: Project/AzureRender/src/render/RenderGraph.hpp
    - name: SceneView::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RendererCore.hpp
    - name: CaptureRequest::kSchemaVersion
      value: "1"
      source: Project/AzureRender/src/render/RendererCore.hpp
    - name: RendererCoreBoundary::kApiVersion
      value: "1"
      source: Project/AzureRender/src/render/RendererCore.hpp
    - name: CaptureRequest 尺寸与帧率值域
      value: "64..7680 × 64..4320；fps 1..240；默认 1280×720×1 帧 @60"
      source: Project/AzureRender/src/render/RendererCore.cpp
    - name: ImageBarrierDesc::mipLevels
      value: "1（最小合法值）"
      source: Project/AzureRender/src/rhi/Rhi.hpp
    - name: maxFramesInFlight（上下文默认）
      value: "2"
      source: Project/AzureRender/src/render/RenderContext.hpp
    - name: shadowMapSize（上下文默认）
      value: "2048"
      source: Project/AzureRender/src/render/RenderContext.hpp
    - name: kEnvironmentMipLevels 用途基格式
      value: "RGBA16F（RGBA16G16B16A16_SFLOAT）"
      source: Project/AzureRender/src/render/EnvironmentAsset.hpp
retrieval_hints:
  - "一帧里的 Pass 顺序和图像屏障是怎么算出来的？"
  - "为什么改一个 Shader Binding 要同时改 GLSL、Descriptor Layout、Pool 与参考表？"
  - "渲染设置的默认值、有效范围与 v6→v7 迁移写在哪里？"
  - "CPU 到 GPU 的每帧上传是怎么分段复用的？"
  - "⚠️ 你要找的是帧槽、Acquire/Present 与 Capture，不在这里，在 Project_host"
  - "⚠️ 你要找的是聚簇光照、级联阴影的具体参数含义，不在这里，在 Project_character"
  - "本子系统的对外名字是 RHI / RenderGraph / RenderSettings，需求里的『画质设置』『渲染路径』对应这里"
  - "新增帧内资源声明只能经 `RenderGraph::addResource/importImage`，不得在 Renderer 内私自律托生命周期"
architectural_role: "渲染核心层，画面参数的唯一权威定义与 GPU 操作的唯一出入口"
---

## 业务意图

这一层保证"画面上出现的每一个参数都有唯一权威定义，GPU 资源的每一次创建与交接都有显式声明"。RenderGraph 把帧内资源与 Pass 的读写关系编译成执行顺序和屏障；RHI 把资源创建与命令录制收敛成一个可被 `NullRhi` 无 GPU 复刻的窄接口；`RenderSettings` 集中保存带版本的画质参数与 Look。它解决的业务问题是：让画面回归可以在没有 GPU 的环境检查录制顺序，也让跨帧资源状态不再依赖调用者记忆。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `RenderGraph::addResource/importImage/importBuffer` | Renderer/宿主→图 | `name`, 初始 `ImageBarrierDesc`/`BufferBarrierDesc` | 返回稳定的图内 `ResourceId`；调用方提供初始状态，图不接管 Vulkan 所有权 | `src/render/RenderGraph.cpp` |
| `RenderGraph::addPass/read/write/attachment/use/dependsOn` | Renderer/宿主→图 | `PassId`, `ResourceId`, `RenderGraphUsage`, `finalLayout` | 声明读写与附件用途；非法 ID 抛 `std::out_of_range` | `src/render/RenderGraph.cpp:write` |
| `RenderGraph::compile` | 图→帧 | `error` 出参 | 成功返回 `true` 并清空错误；失败返回 `false` 并保留诊断文本；下次编译覆盖上次结果 | `src/render/RenderGraph.cpp:compile` |
| `RenderGraph::execute` | 帧→RHI | `ICommandRecorder*` | 按执行顺序应用图像与缓冲区屏障后运行 Pass 回调 | `src/render/RenderGraph.cpp:execute` |
| `rhi::IRhi` | Renderer→后端 | Pipeline、Layout、Sampler、ImageView、RenderPass、Framebuffer、一次性上传 | 生产为 `VulkanRhi`，测试为 `NullRhi` | `src/rhi/Rhi.hpp:IRhi` |
| `rhi::ICommandRecorder` | Renderer→后端 | `beginRenderPass`、`bind*`、`draw*`、`dispatch`、`imageBarrier`、`bufferBarrier`、`copyImageToBuffer`、`writeTimestamp` | 只在所属录制线程、`recordScene` 期间有效 | `src/rhi/Rhi.hpp:ICommandRecorder` |
| `ImageBarrierDesc` / `BufferBarrierDesc` | 调用方→录制器 | 阶段掩码、访问掩码、`baseMipLevel`/`mipLevels`/`aspectMask`、`offset`/`size` | `validateImageBarrier` 拒绝"只给一侧阶段""空子资源范围""无阶段的显式访问掩码" | `src/rhi/Rhi.hpp:validateImageBarrier` |
| `rhi::IGpuAllocator` | 全模块→VMA | `GpuBuffer`/`GpuImage`/`GpuAllocatorStatistics` | Host Visible 分配保持持久映射且 Coherent，调用方不配对 map/unmap | `src/rhi/IGpuAllocator.hpp` |
| `UploadRingBuffer` | 帧→GPU | `Slice{buffer, mapped, offset, size}` | 一整块持久映射缓冲按帧分段，`beginFrame` 后切窗，帧 Fence 退休窗口 | `src/rhi/UploadRingBuffer.cpp` |
| `TransientResourcePool` | 宿主→GPU | `TransientResourceKey(name,width,height,format,usage)` | 捕获回读等帧内瞬态资源复用；复用前必须先确认 GPU 完成 | `src/render/TransientResourcePool.cpp` |
| `validateRenderSettings` / `migrateRenderSettings` | 数据→运行 | `sourceSchemaVersion` | 旧版本按迁移读取，未知未来版本直接拒绝 | `src/render/RenderSettings.cpp` |
| `loadShowcasePresetCatalog` / `applyShowcasePresetLook` | JSON→设置 | Grade/Bloom/Outline 三组 | Look Catalog 只拥有这三类数据 | `src/render/RenderSettings.cpp` |
| `loadEnvironmentImage` | 文件→RGBA16F | HDR / PNG / JPG / OpenEXR | LDR 转线性，EXR 按线性浮点读取且 Alpha 固定 1，异常信息含文件路径 | `src/render/EnvironmentAsset.cpp` |
| `RendererCoreBoundary::validateSceneView` | 调用方→核心 | `assetPath`, `cameraPreset`, `lightPreset`, `renderSettings` | 空路径与空预设抛 `std::invalid_argument`；设置同时校验 | `src/render/RendererCore.cpp` |
| `RendererCoreBoundary::validateCaptureRequest` | 调用方→核心 | `outputDirectory`, `width`, `height`, `frameCount`, `fps` | 尺寸必预 `64..7680 × 64..4320`，`fps` 必预 1..240 | `src/render/RendererCore.cpp` |
| `ComputePass::record` | Renderer→后端 | `ComputePassDesc{width,height,localSizeX/Y,Z,enabled}` | 工作组数按 `(extent + localSize - 1) / localSize` 向上取整；尺寸为空或禁用时空录制 | `src/render/ComputePass.cpp` |

## 跨模块依赖

| 依赖 | 引用原因 | 关键符号 | confidence |
|---------|---------|---------|------------|
| `src/rhi/VulkanMemoryAllocatorImpl` | `IGpuAllocator` 生产实现基于 VMA | `VulkanMemoryAllocatorImpl` | extracted |
| `third_party`（Vulkan Headers、tinygltf、stb、nlohmann/json、OpenEXR/Imath/libdeflate/OpenJPH） | 环境资源解码、JSON 读写 | `EnvironmentAsset` | extracted |
| `Project_renderer_sdk` | `registerPasses` 由 Renderer 提供 Pass 回调与资源声明 | `ISceneRenderer::registerPasses` | extracted |
| `Project_scene_editor` | 编辑器会话修改的画质参数经 `RenderSettings` 落到渲染 | `RenderSettings` | extracted |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|-----------|---------|---------|
| `Project_host` | 每帧编译并执行公共帧图，创建上传环与分配器 | `RenderGraph::execute`, `UploadRingBuffer::beginFrame` |
| `Project_character` / `Project_blackhole` | 声明场景 Pass、创建 Pipeline/Descriptor、按帧切上传片 | `IRhi::createGraphicsPipeline`, `ICommandRecorder::dispatch` |
| `tests/*` | `NullRhi` 与 `NullGpuAllocator` 在无 GPU 环境断言调用序列 | `NullRhi::calls` |

## 典型调用链

### 一帧的图编译与执行

```
AzureRenderFrame.cpp:drawFrame
  → RenderGraph 重建：addResource/importImage（HDR Color、Depth、Normal、Shadow）
    → ISceneRenderer::registerPasses(graph, resources, context)     ← 跨子系统：renderer_sdk
      → RenderGraph::read / write / attachment
      → RenderGraph::compile                                        ← 本子系统入口
        → 依赖排序 + 生成 barriers_ / bufferBarriers_
      → RenderGraph::execute(recorder)
        → rhi::ICommandRecorder::imageBarrier / bufferBarrier       ← 跨子系统：rhi
        → Pass 回调 → CharacterSceneRenderer::recordScene
```

### 材质参数生效

```
--showcase / .azscene renderSettings → validateRenderSettings → migrateRenderSettings(v→7)
  → applyShowcasePresetLook → Grade/Bloom/Outline → PostProcessPushConstants → composite 采样
```

## 实现约束清单

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------------------|
| `RenderSettings::kSchemaVersion` | `7` | `RenderSettings.hpp` | 画面设置当前版本，`.azscene` 内持久化 | 语义不兼容才递增；改动需同步 Loader、Schema、迁移与文档 |
| `RenderPath` | `0/1/2` | `RenderSettings.hpp` | 三种执行模型对照 | 性能报告必须写明所选值，混用即无效对照 |
| `diagnosticView` | `0` = Beauty | `RenderSettings.hpp` | Renderer 诊断索引，第 0 项固定为最终合成 | `validateSceneRendererCapabilities` 拒绝其它首项 |
| `morphWeights` | `[0,0]` | `RenderSettings.hpp` | 两个当前 Morph Target 权重 | 与 `skin.comp` 的 Morph Push Constant 一一对应 |
| `ImageBarrierDesc::mipLevels` | ≥1 | `Rhi.hpp` | 0 直接抛 `std::invalid_argument` | 两个后端共用同一校验，防止测试通过而真机黑屏 |
| `BufferBarrierDesc::size` | 可用 `VK_WHOLE_SIZE` | `Rhi.hpp` | 阶段为 0 时回退 `TOP_OF_PIPE`/`BOTTOM_OF_PIPE` | 单图形队列契约，`queueFamily` 固定 `VK_QUEUE_FAMILY_IGNORED` |
| `RenderContext::bindlessTextures` | bool | `RenderContext.hpp` | true 走全局纹理数组，false 走逐材质固定表 | 两条路径共用同一份 Shader 源码条件编译变体 |
| `RenderContext::computeSkinning` / `rgba16fStorageImage` | bool | `RenderContext.hpp` | false 时分别回退顶点蒙皮与线性 Blit 生成 Mip | 设备能力降级必须可被 QA 参数强制复现 |

### 必须包含的协议字段（数据契约）

| 契约名 | 关键字段 | 说明 |
|--------|---------|------|
| `RenderSettings` v7 | `sceneType`, `renderPath`, `stylizedLightingEnabled`, `styleMaskStrength`, `diffuseBandThreshold`, `showcasePreset`, `innerOutlineEnabled`, `silhouetteOutlineEnabled`, `diagnosticView`, `morphWeights`, `faceSdf`, `bloom`, `outline`, `shadow`, `grade`, `blackhole`, `characterPresentation` | 新增字段必须给默认值，禁止靠"忽略未知字段"伪造兼容 |
| Material Push Constant | Class、Feature Bit、Profile Version、Alpha、Emissive、AO、Lam Shadow、Matcap、Hair/Style/Feature Parameters | 合计 `128` bytes = Vulkan 规范保证的最小 Push Constant 容量，扩展须迁 UBO/SSBO |
| 最终 Composite Descriptor | binding 0 Normal / 1 Depth / 2 Shadow / 3 HDR Scene Color | 与 `shaders/inner_outline.frag` 等逐条一致 |

### 必须实现的函数

| 函数名 | 所在文件 | 说明 |
|--------|---------|------|
| `RenderGraph::compile` | `render/RenderGraph.cpp` | 返回 `false` 时保留错误文本供诊断层记录，不得改为静默成功 |
| `validateImageBarrier` | `rhi/Rhi.hpp` | 两后端共用；新增屏障字段时同步 `NullRhi` 与 `VulkanRhi` 的断言 |
| `migrateRenderSettings` | `render/RenderSettings.cpp` | 唯一旧版本入口；未识别的未来版本拒绝加载 |
| `ComputePass::dispatchSize` | `render/ComputePass.hpp` | 向上取整规则集中于此，禁止在调用点手算工作组数 |
| `computeCascadeSplits` | `render/CascadedShadow.hpp` | 非正 near/far、cascade 为 0 或权重越界均抛 `invalid_argument`；`splits.back()` 强制等于 `farPlane` |
| `ClusteredLightGrid::ClusteredLightGrid` | `render/ClusteredLightGrid.hpp` | 任一轴为 0、`nearPlane ≤ 0` 或 `farPlane ≤ nearPlane` 即拒绝构造 |
| `validateSceneView` / `validateCaptureRequest` | `render/RendererCore.cpp` | 进程内 Renderer SDK 的参数入口，与 CLI 同序验证 |

### 边界约束（能做 / 禁止）

- 允许：图内资源用名字做诊断标识。禁止：把资源名当持久化标识或跨帧句柄保存。
- 允许：只在资源加载期调用 `IRhi` 的 `copyBuffer/transitionImageLayout/copyBufferToImage/generateMipmaps` 与 `executeOneShot`（各自内部提交并等待）。禁止：每帧调用它们 → 退化成同步等待，帧率骤降。
- 允许：命令录制器在所属录制线程内使用。禁止：跨线程共享录制器或跨帧保存 `commands`/`sceneFramebuffer`。
- 禁止：跨队列所有权转移当前不开放；新增队列必须同时补 `queueFamily` 字段与验收，否则屏障契约不成文地失效。
- 禁止：`TransientResourcePool` 在未确认 GPU 完成前退休或销毁资源 → 使用中资源被回收导致画面撕裂与 Validation 报错。
- 禁止：以“与创建参数类似”为由省略 `CaptureRequest` 的重复校验；捕获与重开捕获走同一入口，尺寸/帧数/fps 值域必须逐项列在契约里。
- 禁止：给 `RingFrameAllocator` 传非 2 的幂对齐或 0 容量/帧数 → 必抛 `invalid_argument`，静默回退会让上传偏移错乱。
- 禁止：在 Shader 内再次执行 `pow(color, 1/2.2)` → 场景侧已是线性 HDR，重复 Gamma 会造成整体洗白（来源：`docs/archive/milestones/HDR_TONEMAPPING_DESIGN_CN.md`）。
- 禁止：Normal/HN/打包数据纹理走 sRGB 解码，只有 Base Color 与 Emissive 允许 sRGB（来源：`docs/assets-and-editor.md`）。

### 设计决策

| 决策点 | 存储粒度 | 备选 | 选定理由 |
|--------|---------|---------|---------|
| 每帧上传 | 单一持久映射大缓冲按帧分段（`UploadRingBuffer`） | 每帧多个小 Uniform Buffer | 降低分配压力与句柄数，帧 Fence 即退休整窗，无需逐分配生命周期 |
| RHI 抽象程度 | 直接镜像 Vulkan 类型（format/layout/compare op） | 自定义枚举体系 | 目标是一个调用漏斗、显式所有权与可录制 mock，不做 API 中立 |
| 屏障来源 | RenderGraph 依声明生成 | Pass 内手写 `vkCmdPipelineBarrier` | 顺序与可见性可被 `NullRhi` 单测断言；手写屏障易漏 |

## 变更风险

- 改 `RenderGraph` 的 ID 语义或 `compile` 排序：所有 Pass 声明同时受影响，Character/Blackhole/公共后处理需重跑，`AzureRender.RenderGraph` 断言（写者先于读者、冲突状态被拒）必须继续通过。
- 改 Descriptor Binding 或 Push Constant 而不同步 GLSL、Descriptor Pool 数量、写入与回退资源：部分驱动表现为黑图或错误采样，`tests` 通过也发现不了。
- 改 `RenderSettings` 默认值或版本：`.azscene` 中已持久化的设置与视觉基线图会一起漂移，需一并说明并更新基线。
- 改屏障阶段/访问掩码推导：表现为偶发闪烁或"只在某些 GPU 出错"，需 Debug Validation + RenderDoc 复核。

## 附：内置文档摘要

`docs/runtime/render-graph.md`、`docs/runtime/rhi-synchronization.md`、`docs/runtime/compute-passes.md`、`docs/runtime/environment-assets.md` 与 `docs/reference.md` 提供本层接口表、症状排查表与 Blackhole/Character 已接入的 Compute Pass 清单；本页只收敛约束与契约。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/runtime/render-graph.md`、`Project/AzureRender/docs/runtime/rhi-synchronization.md`、`Project/AzureRender/docs/runtime/compute-passes.md`、`Project/AzureRender/docs/runtime/environment-assets.md`、`Project/AzureRender/docs/reference.md`（原文已提炼，非完整转录）
