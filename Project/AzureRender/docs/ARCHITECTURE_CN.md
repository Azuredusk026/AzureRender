# AzureRender 当前架构

> 架构版本：2026-08-18 / `0.1.0-rc1`
> 本文只描述当前已经存在的系统，不承载未来任务或历史过程。

## 1. 系统定位

AzureRender 是 C++17/Vulkan 1.3 桌面渲染器。公共核心提供 Vulkan 生命周期、HDR 合成、编辑器、诊断和确定性捕获；场景通过进程内 `ISceneRenderer` 插件边界接入。目前内置三个稳定场景 ID：

- `character`：glTF 风格化角色渲染、材质、动画和展示预设。
- `blackhole`：Schwarzschild 黑洞测地线追踪、时序累积和 HDR bloom。
- `sample`：无私有 GPU 资源的最小 SDK 场景，用于验证扩展契约。

正式论文实验、Android 和跨 DLL 插件 ABI 不属于当前产品能力。

## 2. 仓库结构

```text
Project/AzureRender/
  assets_public/       发布和 CI 可用的自有测试资产
  assets_private/      本地受限资产，禁止打包
  docs/                当前文档；archive/ 只用于历史追溯
  portfolio/           可公开视觉证据与 SHA-256 manifest
  schemas/             材质与 GPU 报告 schema
  shaders/             构建期编译的 GLSL
  src/
    app/               Vulkan 生命周期和公共帧调度
    assets/            glTF 加载与动画数据
    diagnostics/       日志、GPU 能力和 timing
    ecs/               Entity/Component/System
    editor/            SceneModel、编辑器状态和 ImGui UI
    extensions/        场景 renderer 接口与注册表
    platform/          GLFW 前端
    render/            公共设置、context 与 Vulkan helper
    resources/         开发树/安装树资源定位
    scenes/            内置 catalog 与场景实现
  tests/               CPU 单元和契约测试
  tools/               发布、捕获、资产和验证工具
```

`build/` 与 `captures/` 是可删除的本地生成目录，不是源码结构。仓库不再包含 Vulkan Tutorial 副本或文档生成工作区。

## 3. 运行时组成

```text
main + CommandLine
        |
GlfwFrontend
        |
AzureRenderApp -------------------------------------+
  |              |               |                  |
  |        EditorSession    Diagnostics/Capture   ResourceLocator
  |
RendererCore + RenderContext
  |
SceneRendererRegistry -- factory(stable scene id)
  |
  +--------------------------+
  |                          |
CharacterSceneRenderer   BlackholeSceneRenderer   SampleSceneRenderer
```

`BuiltinRendererCatalog` 集中登记内置 factory 与 shader feature；`AzureRenderApp` 只按稳定 ID 请求 renderer，不使用场景构造 `switch`。场景 renderer 可以有完全不同的 GPU 资源与 pass，但不能接管 swapchain 或 queue 生命周期。

## 4. 公共核心所有权

| 模块 | 所有内容 |
|---|---|
| `AzureRenderApp.cpp` | 初始化顺序、窗口、renderer 注册与高层调度 |
| `AzureRenderSupport.cpp` | instance、device、swapchain、同步和 helper |
| `AzureRenderResources.cpp` | 公共 image/buffer/environment |
| `AzureRenderDescriptors.cpp` | 公共 descriptor 生命周期 |
| `AzureRenderPipeline.cpp` | Scene Color、post-process、HUD 等公共 pipeline |
| `AzureRenderFrame.cpp` | 每帧 command recording 和 pass 顺序 |
| `AzureRenderCapture.cpp` | PNG、manifest、GPU timing 输出 |

公共输出以 RGBA16F Scene Color 为 HDR 交换边界，最终经过 ACES fitted tone mapping 输出到 swapchain。双帧并行时，CPU 写入数据和可变 descriptor 按 in-flight frame 分离。

## 5. 场景插件契约

所有场景实现 `ISceneRenderer`：

| 回调 | 责任 |
|---|---|
| `capabilities()` | 声明 depth、normal 和 diagnostic 需求 |
| `onLoad()` | 创建场景资源并绑定公共 context |
| `onSwapchainRecreate()` | 重建尺寸/render-pass 相关资源 |
| `updateFrame()` | 更新模拟、相机和当前帧 UBO |
| `recordScene()` | 向宿主提供的 command buffer 录制命令 |
| `onUnload()` | 按依赖逆序释放场景资源 |
| HUD/manifest hooks | 追加场景专属诊断字段 |

场景不得销毁宿主 handle、跨 recreate 缓存 framebuffer、在普通帧内 `vkQueueWaitIdle`，也不得依赖 `assets_private/` 才能启动。

## 6. Character 渲染路径

```text
glTF/GLB
  -> GltfLoader / material profile / skin / animation
  -> shadow map
  -> HDR main pass (color + normal + depth)
  -> internal/silhouette outline
  -> shared post-process + HUD
  -> swapchain or deterministic capture
```

角色场景的引擎级 2048×2048 方向光 Shadow Map 由场景渲染器写入、材质通路采样。阴影过滤使用两阶段 PCSS：12 点 Poisson blocker search 估计平均遮挡深度，16 点 Poisson PCF 按接收面与遮挡面距离扩大半影。最大半影半径来自 `RenderSettings.shadow.maximumFilterRadiusTexels`，场景序列化、编辑器和捕获 manifest 使用同一参数源；黑洞渲染器仍只清空公共阴影资源，不受角色过滤策略影响。

当前支持 Base Color、Normal、Metallic-Roughness、Emissive、Style Mask、Matcap、Hair Data、Toon Ramp、Face SDF、GPU skinning、morph target 和透明排序。

展示 look 的结构由 `RenderSettings` 管理，数值来自版本化的 `assets_public/showcase_looks.json`。启动时由 `ResourceLocator` 解析并校验 catalog；编辑器、F 键、Portfolio 和 QA 显式切换都走同一应用入口。背景与展示地台是 Character renderer 内的独立模块，加载 `.azscene` 时不会无条件覆盖用户保存的开关或调色。

## 7. Blackhole 渲染路径

```text
blackhole trace pass
  -> private RGBA16F raw trace
  -> TAA + history clamp + single-pass HDR bloom
  -> two private RGBA16F ping-pong history images
  -> blackhole composite
  -> shared HDR Scene Color
  -> tone mapping / capture
```

每个 in-flight frame/history-write 组合拥有不可变 descriptor set。首帧、resize、capture 开始和相机非连续变化会使 history 失效。静态 1280x720 基准在两次 36 帧捕获中逐帧一致。

`performance`、`balanced`、`cinematic` 三档通过最大积分步数、trace 数和连续近场步长细化控制成本；`front`、`orbit-left`、`high`、`close`、`over-shoulder` 五个相机档位可由 CLI 和场景设置持久化。吸积盘密度、多普勒色移和相机属于 Blackhole renderer 内部模块，不污染 Character renderer。详细协议见 [Blackhole 质量与视觉回归](BLACKHOLE_QUALITY_CN.md)。

## 8. 数据契约

- `RenderSettings`：版本化运行配置，包含 renderer、材质表现、角色展示模块、grade、bloom 和 outline；当前 schema 为 v6。
- `.azscene v2`：资源引用、多节点树、transform、prefab/instance 引用、renderer 类型和可序列化设置；读取器兼容 v1。
- capture manifest：固定时间步、设备、设置、场景状态和输出模式。
- `portfolio_manifest.json`：经过筛选的公共图片、字节数和 SHA-256。
- `gpu_capability_report.schema.json`：设备能力报告契约。
- glTF extras：AzureRender 材质 profile；未知/缺失字段必须有明确 fallback。

## 9. 扩展新场景

1. 为 `SceneType` 和 CLI 定义稳定小写 ID。
2. 实现 `ISceneRenderer` 并写清资源所有权。
3. 在 composition root 注册 factory，不向 App 主循环添加场景逻辑。
4. 把新 shader 加入 CMake 编译列表。
5. 增加 CLI、scene round-trip、registry 和公共资产 smoke 测试。
6. 验证 load/unload、resize、capture、timing、Debug Validation 和其他场景回归。

新增能力的优先级与准入条件见 [未来开发路线](DEVELOPMENT_ROADMAP_CN.md)。

## 10. 已知架构边界

## 场景环境资源

`RenderContext::environment` 只传递场景无关的 `SceneEnvironmentSource`。`render/EnvironmentAsset` 负责解码 HDR/PNG/JPG，以及识别包含 `_Right/_Left/_Up/_Down/_Front/_Back` 六张图的目录；六面资源在 CPU 侧转换成最大 2048×1024 的线性 RGBA16F 等距柱状图。Character 与 Blackhole 分别拥有自己的 Vulkan image、sampler 和 descriptor 生命周期，不共享或互相销毁 GPU handle。

Blackhole 使用逃逸光线方向采样环境，因此天空也参与引力透镜偏折；Character 使用同一方向约定进行背景、漫反射和粗糙度 mip 采样。外部私有环境只用于本机视觉 QA，不进入 Git 和发布包；环境为空时两种 renderer 均保留程序 fallback。

当前构建依赖 `stb_image`，支持 Radiance HDR、PNG 和 JPG，但不支持 OpenEXR。传入 `.exr` 会明确失败，不会按 LDR 静默读取。

- 当前插件是进程内 C++ factory，不保证二进制 ABI。
- 黑洞画面仍缺少离屏 GPU 图像自动化测试。
- prefab 当前是持久化引用与实例覆盖契约，尚不包含跨文件展开器。
- Traditional/Subpasses/Dynamic Rendering 的论文实验代码不是已完成研究结论。
- 私有角色只用于本地 QA，发布和 CI 必须完全依赖公共资产。
