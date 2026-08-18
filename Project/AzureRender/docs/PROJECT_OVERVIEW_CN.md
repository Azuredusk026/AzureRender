# AzureRender 项目概览

> 最后核对：2026-08-18
> 版本：`0.1.0-rc1`
> 当前任务状态见 [当前开发计划](ACTIVE_DEVELOPMENT_PLAN_CN.md)。

## 1. 项目定位

AzureRender 同时承担两个目标：

- 作品集产品：桌面 Vulkan 风格化角色渲染器、编辑器和可插拔场景演示。
- FYP 研究平台：在等价 NPR workload 下比较 Traditional Multi-pass、Subpasses 和 Dynamic Rendering Local Read。

产品侧已具备稳定演示能力；正式论文实验仍未获准启动，不能把现有三路径原型描述成完整研究结论。

## 2. 当前工程状态

- 语言与构建：C++17、CMake、Ninja、vcpkg。
- 图形与窗口：Vulkan 1.3、GLFW、GLSL/SPIR-V。
- UI：vendored Dear ImGui Docking。
- 资产：glTF/GLB、tinygltf、stb。
- 平台：Windows 为正式发布基线；Linux CI 使用 Ubuntu 24.04。
- 测试：12 个 CTest；2026-08-18 Debug 记录为 12/12 通过。
- P0 整理前的实现基线为 `fefbac4`。

## 3. 已交付能力

### 渲染核心

- Vulkan instance/device/swapchain、Validation、双帧并行和 resize/recreate。
- HDR RGBA16F Scene Color、ACES fitted tone mapping、Shadow/Main/Post-process/HUD 链路。
- GPU timestamp、诊断视图、确定性 PNG/video capture 和 manifest。
- `ISceneRenderer` 场景扩展边界及 `character`、`blackhole` 两种实现。

### 角色场景

- glTF 节点、材质、动画、GPU skinning、morph target。
- Base Color、Normal、Metallic-Roughness、Emissive、Style Mask、Matcap、Hair 数据。
- Toon ramp、材质阴影色、Hair KK、Shadow Map、倒壳与屏幕空间描边。
- HDR environment、specular prefilter、逐三角形透明排序。
- 固定机位、展示灯光、技术 HUD 和作品集捕获。

### 编辑器与工程化

- Dear ImGui Docking、离屏 Viewport、相机、拾取和三轴 Gizmo。
- Outliner、Inspector、Scene Graph、`.azscene` 原子保存和 session 脏状态。
- ECS entity/component/system 与场景桥接。
- 类型化 CLI、统一诊断、资源定位、GPU 报告 schema。
- Windows/Linux CI、安装树、合规 manifest 和 RC 包。

### 黑洞场景

- Schwarzschild 黑洞全屏追踪。
- 连续球对称步长、欧拉偏折、体积吸积盘、温度和相对论颜色变化。
- 4x stratified supersampling、诊断视图、capture manifest 和 GPU timing。
- TAA/bloom 私有资源已经创建但尚未接入稳定渲染路径，属于当前 BH-2.2 工作。

## 4. 架构总览

```text
main / CommandLine
        |
GlfwFrontend ---- AzureRenderApp
                       |
        +--------------+----------------+
        |              |                |
 Renderer Core    Editor Layer    Diagnostics/Capture
        |
 ISceneRenderer + SceneRendererRegistry
        |
   +----+------------------+
   |                       |
CharacterSceneRenderer  BlackholeSceneRenderer
```

主要所有权：

| 区域 | 职责 |
|---|---|
| `src/app` | Vulkan 生命周期、公共 render pass、资源、帧录制和捕获 |
| `src/scenes` | 场景专属资源、pipeline、update 和 draw |
| `src/render` | RenderSettings、RenderContext 和 Vulkan helper |
| `src/editor` | 编辑器状态、UI、相机、SceneModel |
| `src/assets` | glTF 数据加载与动画 |
| `src/ecs` | Entity/Component/System |
| `src/diagnostics` | 日志与 GPU 能力报告 |
| `src/resources` | 开发树与安装树资源定位 |
| `shaders` | 构建期编译的 GLSL |

详细边界见 [架构规范](ARCHITECTURE_CN.md)。

## 5. 测试覆盖

当前 CTest 注册：

1. CommandLine
2. EditorCameraController
3. EditorSession
4. Ecs
5. PickMath
6. SceneModel
7. RuntimeDiagnostics
8. GpuCapabilityReport
9. ResourceLocator
10. ExtensionRegistry
11. ReleaseGateMissingBuild
12. InstallManifestRoundTrip

仍缺少黑洞 shader 输出和 TAA history 的自动化专项测试，因此黑洞改动必须保留 Validation、截图和确定性捕获检查。

## 6. 已知限制

- 黑洞 TAA/bloom 尚未启用，当前以 4x supersampling 抑制噪声。
- 私有 Laevat 资产许可不允许公开分发。
- 动态插件只存在进程内注册抽象，没有跨 DLL ABI。
- Android 和正式论文实验无限期 Deferred，只有用户主动启用后才恢复。
- 旧工业科幻场景路线已删除；P2 只提升现有角色场景的终末地式美术表现。
- 编辑器没有完整 Undo/Redo、资产热重载和依赖图。
- 一些 legacy capture/glTF 字段保留兼容，不能仅因品牌重命名而删除。

## 7. 当前权威基准

- 角色 HDR Beauty：`captures/s36_hdr_beauty_v1/frame_000000.png`
- SHA-256：`5E8BF8B507FE07F385EAADF563DF40CD3C23FA6A2433156DEFD1BFD6AB829357`
- BH-2.1 记录性能：约 14.1 ms main scene，配置为 1800 steps、4 samples。

生成目录通常不进入版本控制；基准哈希和复现参数必须同时记录，不能只引用本机绝对路径。
