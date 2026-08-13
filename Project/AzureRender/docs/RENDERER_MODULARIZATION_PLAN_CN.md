# AzureRender 发布化与模块化路线

> 状态：AR-0 Complete；AR-1 在 CQ-3 Face SDF 视觉闭环后开始，不取代角色画质主线。

## 1. 目标

AzureRender 将逐步从单体作品集应用演进为可发布、可嵌入、可扩展的风格化
Vulkan 渲染器，并在稳定核心接口之上增加轻量编辑器。当前不直接引入完整 ECS、
脚本系统或跨 DLL C++ ABI。

## 2. 固定边界

```text
Application / CLI / Future Editor
        |
        v
RenderSettings + SceneView + CaptureRequest
        |
        v
Renderer Core
        |
        +-- Vulkan Context / Resources / Pipelines
        +-- Render Features
        +-- Capture / Diagnostics
```

- CLI、键盘、Capture 和未来 GUI 必须读写同一 `RenderSettings`；
- GUI 不得直接修改 Descriptor、Pipeline 或 Vulkan 资源；
- Asset Pipeline 只接受显式声明的纹理通道与方向，不在 Shader 中猜测；
- Portfolio Render Path 与 Benchmark Workload 继续隔离；
- 每次结构拆分必须保持公共/私有资产的既有画面与 Validation 行为。

## 3. 增量路线

### AR-0 设置与契约基线

- `RenderSettings v1` 收口风格光照、Ramp、Outline、诊断视图和 Face SDF 参数；
- Capture Manifest 保存设置版本和可复现参数；
- Face SDF 使用版本化 glTF 元数据契约；
- 提供资产兼容性审计，不把普通 Face Base Color 误判为 SDF。

退出条件：CLI、键盘和 Capture 共用同一设置对象；旧资产画面不变；错误 SDF 元数据
在加载阶段失败。

### AR-1 Renderer Core Boundary

- 从 `AzureRenderApp` 抽出 `VulkanContext`、`Renderer` 和 `FrameCapture`；
- 窗口/输入只负责平台事件；
- Renderer 接收 `SceneView`、Camera 与 `RenderSettings`；
- 保持 GLFW 前端为默认可执行程序。

退出条件：Renderer 不读取命令行、不处理 GLFW 键盘事件，并可由第二个前端调用。

### AR-2 Scene 与 Asset Model

- 建立轻量 Scene Graph：Node、Transform、Mesh、Camera、Light、Animator；
- 使用稳定 `AssetId` 和 Asset Registry，不以裸绝对路径作为运行时身份；
- 定义版本化 `.azscene`，保存资源引用、节点关系和可覆盖设置；
- 暂不引入通用 ECS。

退出条件：场景能够保存、重新加载并生成相同 Capture Manifest。

### AR-3 Editor Preview

- 使用 Dear ImGui Docking；
- 首版只包含 Viewport、Scene Outliner、Inspector、Asset Browser、Console；
- 支持模型加载、节点选择、相机查看、灯光与材质参数编辑、Capture；
- GUI 状态不复制 Renderer 状态。

退出条件：用户可在 GUI 中打开场景、选择对象、修改参数、保存并重新打开。

### AR-4 Feature Registry 与发布加固

- 先提供进程内 `IRenderFeature`、`IAssetImporter`、`IEditorPanel` Registry；
- 接口稳定后才增加版本化 C ABI 动态插件；
- 补齐 Windows/Linux CI、安装布局、日志、GPU 能力报告和公共 Demo；
- 运行时资源不得依赖源码绝对路径。

退出条件：新面板或导入器无需修改主循环；Windows/Linux 干净环境可运行发布包。

## 4. 与画质主线的执行顺序

```text
CQ-3 Face SDF + AR-0
-> AR-1 Renderer Core Boundary
-> CQ-4 Hair KK
-> AR-2 Scene/Asset Model
-> CQ-5 Rim/Bloom
-> AR-3 Editor Preview
-> CQ-6 Final Grade
-> M2 Gate
-> M3 Industrial Scene
-> AR-4 Feature Registry / Release Candidate
```

AR 工作包只允许实现当前画质节点确实需要的接口，不提前扩张为通用游戏引擎。
