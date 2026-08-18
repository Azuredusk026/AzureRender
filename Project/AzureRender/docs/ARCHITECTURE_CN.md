# AzureRender 架构规范

## 1. 设计原则

- `AzureRenderApp` 拥有 Vulkan 公共生命周期和共享渲染设施。
- 场景渲染器拥有场景专属资源与 pipeline，不反向拥有 swapchain。
- 编辑器通过场景模型和上下文工作，不直接承担文件或 GPU 生命周期。
- CLI、场景文件、运行时 UI 和 capture 使用同一份 `RenderSettings` 语义。
- 扩展接口是进程内 C++ 边界，不承诺 DLL ABI。
- 内置和未来场景统一通过 `SceneRendererRegistry` 的稳定 ID/factory catalog
  创建；`AzureRenderApp` 不维护按场景类型构造对象的分支。

## 2. 公共渲染生命周期

`AzureRenderApp` 负责：

- instance/device/queue/command pool
- swapchain 和 frame synchronization
- HDR Scene Color、depth、normal、shadow 和最终 composite
- capture、GPU timing、HUD 和 technical sequence
- editor viewport 及 ImGui backend
- 创建 `RenderContext` 并调度活动场景渲染器

公共代码分布：

| 文件 | 职责 |
|---|---|
| `AzureRenderApp.cpp` | 生命周期与高层调度 |
| `AzureRenderSupport.cpp` | device/swapchain/helper |
| `AzureRenderResources.cpp` | image/buffer/environment |
| `AzureRenderDescriptors.cpp` | descriptor 生命周期 |
| `AzureRenderPipeline.cpp` | 公共 render pass/pipeline |
| `AzureRenderFrame.cpp` | 每帧 command recording |
| `AzureRenderCapture.cpp` | capture/manifest/timing |

## 3. 场景渲染器契约

所有场景实现 `ISceneRenderer`：

| 回调 | 责任 |
|---|---|
| `capabilities()` | 声明 depth/normal/diagnostic 需求 |
| `onLoad()` | 创建场景资源，读取稳定 context |
| `onSwapchainRecreate()` | 重建尺寸和 render-pass 相关资源 |
| `updateFrame()` | 更新 CPU 状态和当前帧 UBO |
| `recordScene()` | 只向给定 command buffer 录制命令 |
| `onUnload()` | 按依赖逆序释放场景资源 |
| manifest/HUD hooks | 追加场景专属诊断字段 |

场景渲染器不得：

- 销毁引擎拥有的 handle。
- 缓存 swapchain framebuffer 并跨 recreate 使用。
- 在 `recordScene()` 内提交 queue 或等待 device idle。
- 静默改变其他场景的 `RenderSettings` 默认值。
- 依赖私有资产才能启动。

## 4. 当前实现

### CharacterSceneRenderer

拥有角色资产 GPU 数据、材质 descriptor、shadow/main/outline 绘制和角色专属 QA。迁移后的目标是保持 S36 Beauty 哈希稳定。

### BlackholeSceneRenderer

使用 fullscreen triangle 执行每像素测地线追踪。它不需要几何 depth/normal，但仍需让公共 shadow diagnostic 获得有效布局。场景私有 raw trace 先写入单颜色附件 render pass，TAA/bloom 将它与双缓冲 history 累积，最后由 composite pass 写回公共 Scene Color。按 in-flight frame 与 history 写入索引预分配的 descriptor 始终保持不可变。

## 5. Vulkan 资源规则

- 资源所有者同时负责创建、recreate 和销毁。
- 与 render pass 兼容性相关的 pipeline 必须随对应 render pass 重建。
- 两帧并行时，CPU 每帧写入的数据和可能被更新的 descriptor 必须按 frame 分离。
- history 图像在首次使用、resize、场景切换和 capture 重启时显式失效。
- 图像布局变化必须由 render pass dependency 或明确 barrier 覆盖。
- 不使用 `vkQueueWaitIdle` 处理普通帧同步；初始化和受控重建除外。

## 6. 数据边界

- `RenderSettings`：版本化渲染配置。
- `.azscene`：编辑场景、资源引用、节点树和 renderer 类型。
- capture manifest：确定性输入、版本、设备、场景和输出证据。
- GPU capability report：符合 `schemas/gpu_capability_report.schema.json`。
- glTF extras：保留已有 legacy 字段兼容，新增字段必须有 fallback。

## 7. 新增场景的最小流程

1. 在 `SceneType` 和 CLI 中增加稳定名称。
2. 实现 `ISceneRenderer`，明确资源所有权和 capabilities。
3. 注册到 `SceneRendererRegistry`；App 只按稳定 ID 请求实例。
4. 把 shader 加入 CMake 编译目标。
5. 增加 CLI/scene round-trip 测试和公共资产 smoke。
6. 验证切换、resize、capture、GPU timing 和卸载。

当前开发计划未授权新增场景；本节只是接口规范。
