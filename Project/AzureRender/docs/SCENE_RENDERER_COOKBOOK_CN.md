# AzureRender 场景渲染器 Cookbook

> 版本：1（与 AR-10 / v6 队列一同冻结）
> 适用：任何希望新增一个完整场景类型（角色、黑洞、工业场景、自定义 demo）的开发者
> 配合阅读：`docs/ACTIVE_DEVELOPMENT_PLAN_CN.md`、`docs/RENDERER_MODULARIZATION_PLAN_CN.md`、ISceneRenderer.hpp 头文件

## 1. 5 分钟概念

AzureRender 引擎核心只关心**帧循环、交换链、HDR Scene Color + ACES 合成、capture/timing、 HUD/editor**——所有这些**与场景内容无关**。"我画什么"完全由 `ISceneRenderer` 决定：

```
┌─────────────┐    drawFrame     ┌──────────────────────┐    recordScene    ┌──────────────┐
│  Engine Core │ ─────────────► │  currentSceneRenderer │ ─────────────► │ HDR Scene Color │
│  (swapchain, │ ◄───────────── │  (ISceneRenderer*)   │ ◄───────────── │   附件              │
│   HUD, …)   │  updateFrame    └──────────────────────┘  per-frame UBO  └──────────────┘
└─────────────┘
```

引擎维护一个 `std::unique_ptr<ISceneRenderer> sceneRenderer_`；选择哪一个由
`RenderSettings::sceneType` 决定。`--scene-type character|blackhole` 或 `.azscene` 里的
`sceneRenderer` 字段改变它。当前已实现的两个渲染器：
- `CharacterSceneRenderer`：现有角色管道的可插拔封装（ISceneRenderer 协议下的实现）
- `BlackholeSceneRenderer`：施瓦西零测地线追踪 + 吸积盘（演示用模板）

## 2. 新增一个场景渲染器的 5 步

假设你要加 `MandelbulbSceneRenderer`：实时迭代 Mandelbulb 三维分形追踪。

### 步骤 1：定义渲染器头

```cpp
// src/scenes/MandelbulbSceneRenderer.hpp
#pragma once
#include "extensions/ISceneRenderer.hpp"
#include <array>
#include <vector>

namespace azurerender {

class MandelbulbSceneRenderer final : public ISceneRenderer {
public:
    MandelbulbSceneRenderer() = default;
    ~MandelbulbSceneRenderer() override = default;

    std::string_view name() const noexcept override { return "mandelbulb"; }
    SceneRendererCapabilities capabilities() const override;
    void onLoad(const RenderContext& context) override;
    void onSwapchainRecreate(const RenderContext& context) override;
    void updateFrame(const SceneFrameData& frame) override;
    void recordScene(const RenderContext& context) override;
    void onUnload(const RenderContext& context) override;
    void appendHudText(std::ostringstream& text) const override;

private:
    // 与 Character/Blackhole 相同的"per-frame UBO + pipeline + descriptor set"
    // 拥有模式...
};

}  // namespace azurerender
```

### 步骤 2：在 SceneType 加上枚举值

```cpp
// src/extensions/SceneType.hpp
enum class SceneType : std::uint32_t {
    Character = 0,
    Blackhole = 1,
    Mandelbulb = 2,          // 新增
    Count = 3,
};

inline SceneType sceneTypeFromString(const std::string& name) {
    if (name == "character")  return SceneType::Character;
    if (name == "blackhole")  return SceneType::Blackhole;
    if (name == "mandelbulb") return SceneType::Mandelbulb;
    throw std::invalid_argument(...);
}
```

### 步骤 3：实现渲染器（关键接口）

每个方法的契约：

- **`onLoad(const RenderContext&)`**：引擎已创建 scene render pass、post-process render pass、shadow render pass、HDR/depth/normal attachments、uniform/per-frame buffers 容器等。`context.assetPath` 是已解析的资源绝对路径；`context.shaderDirectory` 是编译好的 `.spv` 目录；`context.swapchainExtent/renderExtent/sceneColorFormat/...` 是当前交换链与附件配置。你的 `onLoad` 只需做"创建 GPU 资源并使用 context 提供的句柄"。
  **重要**：不要在 `onLoad` 里销毁任何 `context.*` 资源；它们是引擎拥有的。
- **`onSwapchainRecreate`**：swapchain 重建后 `context.sceneRenderPass`、`sceneFramebuffer` 句柄变了，依赖它们的管线必须重建。其他不依赖 swapchain 的资源（你的 pipeline 不依赖 swapchain 时）可保留。
- **`updateFrame(SceneFrameData&)`**：CPU 侧每帧工作。引擎把相机、QA、capture、编辑状态打包成 `SceneFrameData` 推给你；你决定用多少。Character 接收 `cameraPosition/cameraTarget/rotationAngle/QA/...`；Blackhole **有意忽略**引擎相机（自管框定），只接 `swapchainWidth/Height` 计算宽高比。
- **`recordScene(RenderContext&)`**：在 `context.commandBuffer` 上录制你的场景 pass，必须写到 `context.sceneFramebuffer`（引擎给的）。可以使用 `context.timestampQueryPool` 写时间戳（参考 BlackholeSceneRenderer 的 ts1/ts2 写入方式）。
- **`onUnload(RenderContext&)`**：销毁你在 `onLoad` 里创建的所有 GPU 资源。`context.*` 仍由引擎拥有。
- **可选钩子**：`appendHudText`（追加场景特定 HUD 行）、`sceneState`（暴露给编辑器拾取/Gizmo 的几何与模型矩阵）、`onAnimationKey`（动画键转发）、`restartPlayback/setPlaybackPlaying`（播放控制）、`appendCaptureManifestFields`（capture manifest JSON 字段）。

### 步骤 4：在引擎 createSceneRenderer 里分发

```cpp
// src/app/AzureRenderApp.cpp 的 createSceneRenderer
switch (renderSettings_.sceneType) {
case azurerender::SceneType::Blackhole:
    sceneRenderer_ = std::make_unique<azurerender::BlackholeSceneRenderer>();
    break;
case azurerender::SceneType::Mandelbulb:
    sceneRenderer_ = std::make_unique<azurerender::MandelbulbSceneRenderer>();
    break;
case azurerender::SceneType::Character:
default:
    sceneRenderer_ = std::make_unique<azurerender::CharacterSceneRenderer>();
    break;
}
```

### 步骤 5：CMake 注册

```cmake
# CMakeLists.txt 的 AzureRender 可执行源列表
src/scenes/MandelbulbSceneRenderer.cpp
src/scenes/MandelbulbSceneRenderer.hpp
```

如果带 shader（必须 fullscreen triangle 而非顶点缓冲），把它加入：

```cmake
compile_shader("${SHADER_SOURCE_DIR}/mandelbulb.vert")
compile_shader("${SHADER_SOURCE_DIR}/mandelbulb.frag")
```

## 3. 渲染器契约的"应当"和"不应当"

| 应当 | 不应当 |
|---|---|
| 拥有自己的 GPU 资源并在 onLoad 创建 / onUnload 销毁 | 销毁或修改 `context.*` 句柄（它们属于引擎） |
| `recordScene` 把场景写入 `context.sceneFramebuffer` | 写其他 framebuffer（除非像 Blackhole 那样先做 empty shadow pass） |
| `updateFrame` 决定是否消费 `SceneFrameData`（如 Blackhole 忽略相机） | 假设 `SceneFrameData` 已填充你的专属字段 |
| `appendCaptureManifestFields` 输出场景特有的 JSON 字段 | 直接构造 JSON 文档根（那是引擎的责任） |
| `sceneState` 提供稳定的 `RendererSceneState` 指针 | 返回内部可变状态的裸指针（编辑器读，不写） |
| 仅依赖引擎提供的 `RenderContext` 字段（`device`/`graphicsQueue`/`commandPool`/render passes/extensions） | 创建额外的 Vulkan 实例、surface、队列家族——这些属于引擎 |

## 4. 自带 Shader 的合约

- **所有 shader 必须 `glslc` 编译为 `.spv` 并放入 `context.shaderDirectory`**。这是 `vk::readBinaryFile` 唯一读取的来源。
- **场景渲染器的顶点输入**：要么提供完整顶点缓冲（Character 用 `mesh.vert`/`AssetVertex`），要么用 fullscreen triangle（Blackhole 用 `blackhole.vert` 的 `gl_VertexIndex` 技巧——免顶点缓冲）。
- **场景 pass 的 color blend attachments 数量必须匹配 scene render pass**：当前引擎的 scene render pass 有 **2 个 color attachment**（Scene Color + World Normal）。即使你只写 Scene Color，`colorBlendState.attachmentCount` 也必须是 2，并把第二个 `colorWriteMask=0`（Blackhole 的做法）。
- **`independentBlend`**：引擎已在 `createLogicalDevice` 启用该特性，所以两个 color blend attachment 状态可不同。
- **深度附件**：场景 render pass 有 depth attachment。如果你的渲染器不做深度测试（如 Blackhole 全屏追踪），把 `depthTestEnable=VK_FALSE`、`depthWriteEnable=VK_FALSE`，depth 保持 clear 值即可。

## 5. 测试新场景渲染器

- 把场景渲染器加到 `tests/` 下（参考 `tests/CommandLineTests.cpp` 表驱动风格）。
- 用公共资产 + 至少 120 帧 Debug Validation（`vulkaninfo`/`--debug-validation`）。
- capture 1 帧 + 验证 manifest 含你的字段。
- 启用 `--gpu-timing` 检查你的 scene pass 时间合理（角色 ~0.15ms，黑洞 ~3.5ms）。

## 6. 编辑器与多场景

- 编辑器启动时按 `SceneView::sceneType` 选 renderer；运行时切换 renderer（AR-10.2 范围外）是 `sceneRenderer_.reset() → createSceneRenderer()` 重启路径。
- 编辑器面板里目前可通过 CLI `--scene-type` 启动选场景；动态切换留作 P2（参见 `RENDERER_MODULARIZATION_PLAN_CN.md` 后续）。
- `.azscene` 文件保存 `renderSettings.sceneType`，加载时由 `main.cpp` 自动写入 `options.renderSettings`。

## 7. 黑洞场景作为参考模板

`src/scenes/BlackholeSceneRenderer.*` 是一个完整的"非几何"（无可拾取几何）渲染器实现：
- 全部 pipeline 用 fullscreen triangle（`gl_VertexIndex`）
- 不消费引擎相机（自管）
- 写空 shadow pass 满足布局依赖
- 启动后无顶点缓冲、无 ECS 实体

如果你的场景是纯过程化（raymarching、shader ray casting、SDF），直接以它为模板。

## 8. 角色场景作为参考模板

`src/scenes/CharacterSceneRenderer.*` 是完整的"几何+资产"渲染器：
- 加载 glTF 资产并创建全部 GPU 资源（vertex/index/uniform/joint/oit buffers、8 通道材质纹理、descriptor pool/sets、pipeline）
- 接管 animation 状态（animationIndex/Time/Playing）
- 实现 `sceneState`/`appendHudText`/`appendCaptureManifestFields`/`onAnimationKey`/`restartPlayback`/`setPlaybackPlaying`
- 处理编辑器交互：`RendererSceneState` 暴露 asset 与 modelMatrix 给引擎的 pick/gizmo

如果你的场景是几何资产类（自定义资产格式、procedural mesh、多 primitive 物体），直接以它为模板。