---
type: "Fragment"
id: Project/renderer_sdk
title: "Renderer SDK 与内置场景"
description: "新增一个场景渲染器要满足的接口、能力声明与注册校验，以及内置 character/blackhole/sample 三者的职责边界。"
parent: /Project/_overview.md
fragment: renderer_sdk
entity_names:
  constants:
    - name: SceneRendererCapabilities::kApiVersion
      value: "1"
      source: Project/AzureRender/src/render/RenderContext.hpp
    - name: ExtensionRegistry::kApiVersion
      value: "1"
      source: Project/AzureRender/src/extensions/ExtensionRegistry.hpp
    - name: ExtensionDescriptor::apiVersion 默认
      value: "1"
      source: Project/AzureRender/src/extensions/ExtensionRegistry.hpp
    - name: SceneType（枚举全量）
      value: "Character=0, Blackhole=1, Sample=2, Count=3"
      source: Project/AzureRender/src/extensions/SceneType.hpp
    - name: character 能力标签
      value: "geometry, editor, capture"
      source: Project/AzureRender/src/scenes/BuiltinRendererCatalog.cpp
    - name: blackhole 能力标签
      value: "fullscreen, temporal, capture"
      source: Project/AzureRender/src/scenes/BuiltinRendererCatalog.cpp
    - name: sample 能力标签
      value: "sdk-example, capture"
      source: Project/AzureRender/src/scenes/BuiltinRendererCatalog.cpp
    - name: requiresSceneDepth / requiresSceneNormal 默认
      value: "true / true"
      source: Project/AzureRender/src/render/RenderContext.hpp
    - name: diagnosticView 0 名称
      value: "\"Beauty\""
      source: Project/AzureRender/src/render/RenderContext.hpp
    - name: technical sequence 章节数
      value: "5"
      source: Project/AzureRender/src/app（`--technical-sequence` 校验）
retrieval_hints:
  - "我要新增一种场景渲染器，需要实现哪些方法、注册到哪里？"
  - "为什么 diagnostic view 第 0 项必须是 Beauty？"
  - "Registry 会拒绝什么样的扩展声明？"
  - "Renderer 在 onLoad 与 onUnload 之间可以持有哪些 GPU 对象？"
  - "Shader Feature 目录是做什么校验的？"
  - "⚠️ 你要找的是角色材质分类或黑洞算法细节，不在这里，分别在 Project_character / Project_blackhole"
  - "⚠️ 你要找的是宿主帧循环与 RenderContext 的填充时机，不在这里，在 Project_host"
  - "本子系统也叫 Renderer SDK / 场景扩展层，需求里的『接入新 Renderer』『插件式渲染』先落到这里"
  - "新的场景类型必须在 BuiltinRendererCatalog 注册并扩展 SceneType，不得在宿主里加类型分支"
architectural_role: "扩展边界层，宿主与场景算法之间的唯一接口，禁止绕过 Registry 直接实例化 Renderer"
---

## 业务意图

这一层让"多种画面风格完全不同的场景"共用同一宿主而不互相污染：引擎按 `RenderSettings::sceneType` 每帧只驱动一个 Renderer，Renderer 只借宿主的对象、只对自己的 Pipeline/Descriptor/Buffer/Image/算法状态负责，销毁顺序与所有权因此始终可回答。`capabilities()` 在 `onLoad` 前把附件与诊断视图需求交给引擎，杜绝"场景暗中依赖引擎未提供的资源"。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `ISceneRenderer::name` | Renderer→Registry | 稳定小写字符串 | Registry ID，如 `character` | `src/extensions/ISceneRenderer.hpp` |
| `ISceneRenderer::capabilities` | Renderer→引擎 | `requiresSceneDepth`, `requiresSceneNormal`, `diagnosticViewNames` | 决定引擎建哪些附件与 Pass，`onLoad` 之前调用 | `src/render/RenderContext.hpp:validateSceneRendererCapabilities` |
| `ISceneRenderer::onLoad` | 引擎→Renderer | `RenderContext` | 创建自有 Pipeline/Descriptor/Buffer/Image/Shader | `src/scenes/*.cpp:onLoad` |
| `ISceneRenderer::updateFrame` | 引擎→Renderer | `SceneFrameData` | CPU 侧动画、Uniform、相机相关状态 | `src/scenes/*.cpp:updateFrame` |
| `ISceneRenderer::recordScene` | 引擎→Renderer | `context.commandBuffer`, `sceneFramebuffer` | 只允许记录到当帧命令缓冲 | `src/scenes/*.cpp:recordScene` |
| `ISceneRenderer::registerPasses` | Renderer→图 | `RenderGraph`, `SceneGraphResources` | 只注册回调；context 必须活过图执行 | `src/extensions/ISceneRenderer.hpp` |
| `ISceneRenderer::onSwapchainRecreate` | 引擎→Renderer | `RenderContext` | 重建尺寸/Render Pass 相关资源；黑洞 History 在此失效 | `src/scenes/BlackholeSceneRenderer.cpp` |
| `ISceneRenderer::onUnload` | 引擎→Renderer | `RenderContext` | 清掉 `onLoad` 创建的一切，也覆盖部分初始化；不得抛异常 | `src/scenes/*.cpp:onUnload` |
| 可选 Hook | Renderer→引擎 | `appendHudText`, `sceneState`, `onAnimationKey`, `restartPlayback`, `setPlaybackPlaying`, `appendCaptureManifestFields`, `diagnosticViewName` | HUD、编辑器拾取、动画键、作品集时间线、Manifest 扩展 | `src/extensions/ISceneRenderer.hpp` |
| `ExtensionRegistry<T>::registerFactory` | 启动→Registry | `ExtensionDescriptor{id, apiVersion, capabilities, dependencies}`, `Factory` | 拒绝空 ID、非 `kApiVersion`、空工厂、重复 ID 与未注册的依赖 | `src/extensions/ExtensionRegistry.hpp` |
| `BuiltinRendererCatalog::createRegistry` | 启动→Registry | 三个 Renderer 工厂 | 内置注册入口 | `src/scenes/BuiltinRendererCatalog.cpp` |
| `BuiltinRendererCatalog::shaderFeatures` | 构建/校验 | `ShaderFeatureDescriptor{id, rendererId, shaderStages}` | 记录 Shader 归属，组合校验无需重复写场景判断 | `src/scenes/BuiltinRendererCatalog.cpp` |
| `IRenderFeature` / `IAssetImporter` / `IEditorPanel` | 扩展位 | `name()` / `supports(path)` | 特征、导入器与面板的注册接口（各自独立 Registry 别名） | `src/extensions/IRenderFeature.hpp`, `IAssetImporter.hpp`, `src/editor/IEditorPanel.hpp` |

## 跨模块依赖

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project_host` | 提供 `RenderContext`/`SceneFrameData` 并驱动生命周期 | `AzureRenderApp`, `SceneFrameData` | extracted |
| `Project_render_core` | Pass 注册与屏障、RenderSettings 读取 | `RenderGraph`, `RenderSettings` | extracted |
| `Project_scene_editor` | 拾取与 Inspector 需要 `RendererSceneState` | `RendererSceneState` | extracted |
| `Project_character` / `Project_blackhole` / `SampleSceneRenderer` | 三个内置实现 | `CharacterSceneRenderer` 等 | extracted |
| `src/assets` | Character 载入 `LoadedAsset` | `GltfLoader` | extracted |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|--------|---------|---------|
| `Project_host` | 每帧选择并驱动唯一 Renderer | `SceneRendererRegistry::create`, `ISceneRenderer::*` |
| `.azscene` 数据 | `renderSettings.sceneType` 决定运行哪个 Renderer | `sceneTypeFromString` |
| 文档与构建校验 | Shader Feature 目录用于组合归属检查 | `shaderFeatures` |

## 典型调用链

```
AzureRenderApp.cpp:createSceneRenderer
  → BuiltinRendererCatalog::createRegistry → registerFactory({"character",1,{"geometry","editor","capture"},{}})
  → SceneRendererRegistry::create(sceneTypeName)          ← 本子系统入口
    → ISceneRenderer::capabilities                        ← onLoad 之前
      → validateSceneRendererCapabilities（第 0 项必须 Beauty）
    → ISceneRenderer::onLoad(RenderContext)               ← 私有资源在此创建
每帧：updateFrame(SceneFrameData) → registerPasses / recordScene(RenderContext)
resize： cleanupSwapchain → createSwapchain* → onSwapchainRecreate → History/缓存失效
退出： onUnload → 只释放 onLoad 创建的对象，允许部分初始化状态
```

## 实现约束清单

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------|
| `SceneRendererCapabilities::kApiVersion` | `1` | `RenderContext.hpp` | Renderer 接口版本 | 不匹配即拒绝注册，防止半兼容实现运行时崩溃 |
| `ExtensionRegistry::kApiVersion` | `1` | `ExtensionRegistry.hpp` | 全部扩展共用 | 见 `tests/ExtensionRegistryTests.cpp` |
| `SceneType::Count` | `3` | `SceneType.hpp` | 内置类型数量 | 枚举刻意保持小且带版本；新增类型才扩，模块内部拆分不新增类型 |
| `diagnosticViewNames[0]` | `Beauty` | 各 Renderer `capabilities()` | 视图 0 = 最终合成 | 技术序列与 HUD 以 0 为默认基线视图 |

### 必须实现的函数

| 函数名 | 所在文件 | 说明 |
|--------|---------|------|
| `name()` / `capabilities()` / `onLoad()` / `updateFrame()` / `recordScene()` / `onSwapchainRecreate()` / `onUnload()` | Renderer 实现 | 七项为必需；缺任一即无法通过生命周期契约 |
| `registerPasses()` | Renderer 实现 | 只注册回调，不得在图执行后引用失效 context |
| `createRegistry()` / `shaderFeatures()` | `BuiltinRendererCatalog.cpp` | 新增 Renderer 的两个登记点，缺一即 SDK/构建校验不同步 |

### 边界约束（能做 / 禁止）

- 禁止：销毁借自 `RenderContext` 的任何 Handle，或缓存跨帧有效的 `sceneFramebuffer`/`commands` → 触发 use-after-destroy 与 Validation 报错。
- 禁止：`onUnload` 抛异常，或只处理"完整初始化"分支（`onLoad` 半路失败时必须能退出）。
- 禁止：把场景私有资源放回 `AzureRenderApp`；需要新的公共附件时先扩 `SceneRendererCapabilities` 与 `RenderContext`，由宿主统一创建。
- 禁止：Registry 接受空 ID、重复 ID、缺失依赖、`nullptr` 工厂或未来 `apiVersion`。
- 禁止：动态二进制插件路径 — 当前 SDK 只在稳定版本号约束下提供进程内 C++ 接口，未建立 ABI、版本协商与卸载隔离。
- 边界：普通场景从 `SampleSceneRenderer` 起步最安全，它不持有任何 GPU 对象，只清空宿主附件，用于验证生命周期与所有权边界。

### 设计决策

| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 能力声明时机 | `onLoad` 前调 `capabilities()`，引擎据此建附件 | Renderer 首次使用时惰性索取（隐式依赖） | 让"场景依赖宿主未提供的资源"在加载期即不可表达 |
| Shader Feature 归属 | 集中 `ShaderFeatureDescriptor` 表 | 在每个 Renderer 里读构建配置 | 构建与文档校验无需重复写场景判断 |
| 场景选择 | `SceneType` 枚举 + Registry 双门（字符串 ID → Factory） | 运行期按场景名动态加载库 | 静态注册可控且不承诺 DLL ABI |

## 变更风险

- 增/删/改 Renderer 生命周期方法或 `RenderContext` 字段：三个内置场景与任何外部 SDK 用例同时受影响，必须回归 Character、Blackhole、Sample，并重跑 Registry/生命周期/Smoke 与安装资源测试。
- 能力标签字符串（如 `editor`、`capture`）被 HUD、技术序列与文档消费，改名会让诊断视图与作品集步骤对不上。
- `shaderFeatures()` 未与实际 `.spv` 同步时，构建校验会放过缺失 Shader，运行期表现为 Pipeline 创建失败。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/architecture.md`、`Project/AzureRender/docs/development-and-release.md`、`Project/AzureRender/docs/reference.md`（原文已提炼，非完整转录）
