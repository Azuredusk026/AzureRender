# AzureRender

AzureRender 是一个原生 Vulkan 可扩展实时渲染器。项目的目标不是包装单个 Shader Demo，而是建立一套能够承载不同场景、不同 Shader 组合和不同资源契约的长期渲染宿主。

当前两个主要场景代表了相反的工作负载：Character 是由大量网格、材质、透明层和阴影组成的传统光栅路径；Blackhole 是以全屏数值积分、时间累积和 HDR 合成为主的程序化路径。二者共享窗口、设备、Swapchain、公共 Attachment、最终合成、Capture、GPU Timing 和编辑器基础设施，但各自拥有 Pipeline、Descriptor 和算法状态。

![AzureRender 黑洞场景](https://raw.githubusercontent.com/Azuredusk026/AzureRender/main/Project/AzureRender/portfolio/images/blackhole/blackhole_temporal_beauty_v1_1280x720.png)

## 能力总览

| 层级 | 当前实现 |
| --- | --- |
| Vulkan 宿主 | Instance、Validation、设备与 Queue、Swapchain、Render Pass、Pipeline、Descriptor、同步、查询池、资源销毁 |
| 场景扩展 | `ISceneRenderer` 生命周期、能力声明、Registry、Shader Feature Catalog |
| 公共图像管线 | HDR Scene Color、Depth、Normal、阴影贴图、Bloom、描边、Tone Mapping、最终 Present |
| Character | glTF、蒙皮动画、材质分类、Toon Ramp、Face SDF、Hair KK/AO、PCSS、眉毛 Overlay |
| Blackhole | 光线积分、吸积盘噪声、多普勒、Beaming、引力红移、双 History TAA |
| 工具 | 编辑器、命令行 QA、确定性 PNG、GPU Timing JSON、图像比较、发布门禁 |

## 原生 Vulkan 的边界

AzureRender 直接调用 Vulkan API 管理渲染资源和帧执行，不依赖 Unreal、Unity、bgfx 或现成渲染框架。第三方库只负责外围基础能力：GLFW 创建窗口和 Surface，Dear ImGui 提供编辑器控件，tinygltf/stb 解析资产，nlohmann/json 处理数据文件。Shader 使用 GLSL 编写，由 Vulkan SDK 的 `glslc` 编译为 SPIR-V。

因此，“原生 Vulkan 自研”描述的是渲染后端、场景架构和算法实现，而不是声称项目没有任何第三方依赖。

## 系统概览

```mermaid
flowchart TB
    CLI[CLI / .azscene / Editor] --> App[AzureRenderApp]
    App --> Host[Vulkan Host]
    Host --> Registry[Renderer Registry]
    Registry --> Character[Character Renderer]
    Registry --> Blackhole[Blackhole Renderer]
    Registry --> Sample[Sample Renderer]
    Character --> HDR[HDR Scene Color + Depth + Normal]
    Blackhole --> HDR
    Sample --> HDR
    HDR --> Composite[Outline / Bloom / Grade / Tone Mapping]
    Composite --> Swapchain[Swapchain / Capture / Present]
    App --> QA[QA + GPU Timing + Manifest]
```

宿主只选择一个活动 Scene Renderer。新增场景通过 Registry 注册，不要求在帧循环中持续增加场景特判；场景通过只读 `RenderContext` 使用宿主提供的 Vulkan 对象，并负责释放自己创建的资源。

## 两个场景

### 风格化角色

角色路径重点处理非写实材质的可控性，而不是简单套用 PBR。材质分类决定 Ramp、AO、镜面和轮廓行为；Face SDF 提供稳定的脸部明暗形状；Hair HN/P 数据驱动发束法线与双层 Kajiya-Kay 高光；2048 阴影贴图通过 PCSS 产生随遮挡距离变化的软阴影。

[阅读角色渲染原理](character-rendering.md)

### 黑洞模拟

黑洞路径在 Fragment Shader 中积分光线路径，采样程序化吸积盘和环境背景。吸积盘包含旋转体密度、周期噪声和稀疏结构，并利用多普勒频移、相对论增亮和引力红移建立明显不对称。当前帧通过双 History 时间累积稳定高成本采样。

[阅读黑洞模拟原理](blackhole-rendering.md)

## 阅读路线

第一次运行项目：

1. [构建与使用](getting-started.md)
2. [资产、场景与编辑器](assets-and-editor.md)
3. [参数与接口参考](reference.md)

理解实现：

1. [渲染器架构与 Vulkan 实现](architecture.md)
2. [风格化角色渲染](character-rendering.md)
3. [黑洞模拟](blackhole-rendering.md)

继续开发或准备发布：

1. [开发、测试与发布](development-and-release.md)
2. [参数与接口参考](reference.md)

## 当前状态与边界

- Character 和 Blackhole 均可通过同一可执行文件运行，并支持确定性捕获。
- Renderer SDK 当前是进程内 C++ 接口，不承诺跨 DLL 的二进制 ABI。
- `.azscene` 当前 Schema 为 v2，`RenderSettings` 当前 Schema 为 v7。
- 黑洞是视觉导向的 Schwarzschild 近似模拟，不是科研级广义相对论求解器。
- 私有角色用于本机美术验收，公共 CI 和作品集必须使用可再分发资产。
- Android、动态插件 ABI 和完整资产生产管线不属于 `0.1.0-rc1` 的发布承诺。

文档只描述当前可验证实现。历史阶段计划、旧验收记录和源 DOCX 保留在仓库归档或 Git 历史中，不作为当前接口依据。
