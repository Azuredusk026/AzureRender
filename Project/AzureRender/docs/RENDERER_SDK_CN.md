# AzureRender Renderer SDK

> SDK 契约版本：1；适用于进程内 C++ renderer，不承诺跨 DLL ABI。

## 1. 最小接入

参考 `SampleSceneRenderer`。一个 renderer 只实现 `ISceneRenderer`，并在 `BuiltinRendererCatalog::createRegistry()` 注册稳定小写 ID。宿主帧循环不需要为新场景增加分支。

```text
capabilities -> onLoad -> (updateFrame -> recordScene)*
                         -> onSwapchainRecreate -> ... -> onUnload
```

`capabilities()` 的第 0 个诊断视图必须是 `Beauty`。`onLoad` 创建 renderer 自有资源；`onSwapchainRecreate` 只重建尺寸或 render-pass 相关资源；`onUnload` 释放它们。renderer 不得销毁 `RenderContext` 中的 device、queue、command pool、render pass、framebuffer、sampler 或 query pool。

## 2. 宿主与 renderer 边界

- 宿主拥有 window、swapchain、HDR Scene Color、depth/normal、最终合成、capture、timing 和 HUD。
- renderer 拥有场景 pipeline、descriptor、uniform、私有 attachment 和算法状态。
- `RenderContext` 在回调期间只读；每帧 handle 只能在当前帧使用。
- 普通帧不得调用 `vkQueueWaitIdle`；resize 通过 `onSwapchainRecreate` 处理。
- `onUnload` 应可在部分初始化清理路径安全调用，不应抛出异常。

## 3. 内置 catalog

| ID | 能力 | Shader feature |
|---|---|---|
| `character` | geometry、editor、capture | material、outline |
| `blackhole` | fullscreen、temporal、capture | trace、temporal |
| `sample` | sdk-example、capture | clear（无 shader） |

`BuiltinRendererCatalog::shaderFeatures()` 是组合清单。新 feature 在 catalog 中声明归属和 shader stage，避免把组合判断散落到 `AzureRenderApp`。

## 4. 设置与兼容

`RenderSettings::kSchemaVersion` 是设置契约版本。`.azscene` 保存 `renderSettingsVersion`；旧 v1 文件没有该字段时按版本 1 迁移。未知未来版本必须拒绝，未知 renderer ID 必须在 CLI 或场景加载阶段给出明确错误。

## 5. 验收清单

1. registry 重复 ID、缺失依赖、未知 ID 和 API 版本测试通过。
2. capabilities、load/update/record/recreate/unload 顺序契约通过。
3. 旧场景迁移和新场景 round-trip 通过。
4. Debug/Release CTest 通过。
5. 新 renderer、Character、Blackhole 各运行 120 帧；Debug 无 Validation 错误。
