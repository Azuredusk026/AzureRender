---
type: "Fragment"
id: Project/blackhole
title: "黑洞模拟渲染路径"
description: "黑洞如何用全屏光线积分、程序化吸积盘、多普勒与引力红移近似以及双 History 时间累积产出可确定的 HDR 画面。"
parent: /Project/_overview.md
fragment: blackhole
entity_names:
  constants:
    - name: BlackholeQuality::Performance
      value: "0"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: BlackholeQuality::Balanced
      value: "1"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: BlackholeQuality::Cinematic
      value: "2"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: Performance 档 maxTraceSteps / samplesPerPixel / nearStepScale
      value: "600 / 1 / 0.85"
      source: Project/AzureRender/src/render/RenderSettings.cpp
    - name: Balanced 档 maxTraceSteps / samplesPerPixel / nearStepScale
      value: "1100 / 1 / 0.65"
      source: Project/AzureRender/src/render/RenderSettings.cpp
    - name: Cinematic 档 maxTraceSteps / samplesPerPixel / nearStepScale
      value: "1800 / 4 / 0.48"
      source: Project/AzureRender/src/render/RenderSettings.cpp
    - name: BlackholeCamera::Front / OrbitLeft / High / Close / OverShoulder
      value: "0 / 1 / 2 / 3 / 4"
      source: Project/AzureRender/src/render/RenderSettings.hpp
    - name: 默认 physics（rs, escapeRadius, maxSteps, simTime）
      value: "1.0 / 40.0 / 900.0 / 0.0"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: 默认吸积盘参数（diskInner, diskOuter, temperatureScale, shiftMax）
      value: "2.1 / 12.0 / 1.0 / 1.25"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: 默认相机（fovRadians, aspect, supersampleLevels, renderWidth）
      value: "0.9 / 1.7777 / 4.0 / 1280.0"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: 默认 quality（nearStepScale, 保留位）
      value: "0.48 / 0 / 0 / 0"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: TaaUniform::blendWeight
      value: "0.35"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: TaaUniform::bloomThreshold / bloomIntensity
      value: "1.2 / 0.30"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: BloomPushConstants 尺寸
      value: "8 bytes"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: kBloomLevelCount
      value: "4"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: 每帧在飞帧数
      value: "2"
      source: Project/AzureRender/src/scenes/BlackholeSceneRenderer.hpp
    - name: 质量档位默认
      value: "Cinematic"
      source: Project/AzureRender/src/render/RenderSettings.hpp
retrieval_hints:
  - "黑洞画面两侧亮度不对称是靠什么保证的？"
  - "吸积盘出现一圈固定径向接缝，问题在哪？"
  - "什么时候必须重置时间累积 History？"
  - "黑洞的质量档位分别改了什么参数？"
  - "为什么黑盘的画面不能提前 clamp 到 0-1？"
  - "⚠️ 你要找的是角色材质、描边与阴影，不在这里，在 Project_character"
  - "⚠️ 你要找的是公共后处理 Bloom/曝光/色调映射的实现，不在这里，在 Project_render_core 与 Project_host"
  - "本子系统也叫 Blackhole Renderer / 黑洞模拟，需求里的『事件视界』『光子环』『吸积盘』都归这里"
  - "黑洞已冻结：新的着色或视觉调整不能混入角色任务，必须单独立项并先保存确定性基线"
architectural_role: "全屏程序化场景渲染器，已冻结的视觉基准，禁止在同主题改动中顺带调整"
---

## 业务意图

黑洞路径解决的是"在没有黑洞网格、也没有科研级求解预算的前提下，稳定产出一个可复核的引力透镜画面"这个问题：它以全屏程序化积分替代几何建模，用受控近似产生光子环、盘面透镜像与两侧亮度不对称，再用双 History 时间累积把逐像素噪声压到可用水平，并让同一组固定参数在重复运行中得到一致结果。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `--blackhole-quality` | CLI→设置 | `performance` / `balanced` / `cinematic` | 直接决定最大步数、每像素 Trace 数与近场步长比例 | `src/app/CommandLine.cpp`, `src/render/RenderSettings.cpp` |
| `--blackhole-camera` | CLI→设置 | `front` / `orbit-left` / `high` / `close` / `over-shoulder` | 预设只改位置、目标与运动路径，不改画面比例 | `src/render/RenderSettings.cpp` |
| `BlackholeUniform` | CPU→Shader | `cameraPosition/Right/Up/Forward`、`physics`、`cameraFov`、`diskParameters`、`quality` | Raw Trace 的完整输入（相机基向量、rs/escape/maxSteps/时间、盘内外半径、温度尺度、shift 上限、步长比例） | `src/scenes/BlackholeSceneRenderer.hpp` |
| `TaaUniform` | CPU→Shader | `blendWeight`、`bloomThreshold`、`bloomIntensity`、`renderWidth` | 时间累积与内部高亮提取参数 | `src/scenes/BlackholeSceneRenderer.hpp` |
| 渲染器私有资源 | Renderer 内部 | Raw Trace Image、两个 History Image、Framebuffer、Pipeline、Descriptor | 只在 `onLoad` 创建、`onUnload` 释放；宿主只接收标准 HDR 场景输出 | `src/scenes/BlackholeSceneRenderer.cpp` |
| `capabilities()` | Renderer→引擎 | `requiresSceneDepth=false`、`requiresSceneNormal=false`、视图 `Beauty/Photon Ring/Gravitational Lens` | 全屏路径不声明几何缓冲 | `src/scenes/BlackholeSceneRenderer.cpp` |
| Capture Manifest 扩展 | Renderer→磁盘 | 质量档位、相机预设、时间状态 | 让一张图能还原生成条件 | `appendCaptureManifestFields` |
| `ShaderFeatureDescriptor("blackhole.trace"/"blackhole.temporal")` | 构建→校验 | `blackhole.vert/frag`、`blackhole_taa.frag`、`blackhole_composite.frag` | Shader 归属登记 | `src/scenes/BuiltinRendererCatalog.cpp` |

## 跨模块依赖

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project_render_core` | Compute Bloom、环境资源、上传环与图屏障 | `ComputePass`, `loadEnvironmentImage` | extracted |
| `Project_host` | HDR Scene Color、最终 Composite、GPU Timing、Capture | `RenderContext::sceneColorFormat`, `writeTimestamp` | extracted |
| `Project_renderer_sdk` | 生命周期、能力声明与 `onSwapchainRecreate` | `ISceneRenderer` | extracted |
| `Project_scene_editor` | 黑洞不使用几何场景，但需响应设置变更触发 History 重置 | `RenderSettings::blackhole` | extracted |
| `shaders/blackhole*.frag` | 私有 Pipeline 的光线积分与累积实现 | `blackhole.frag` | extracted |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|--------|---------|---------|
| `Project_host` | 公共后处理采样黑洞写出的 HDR Scene Color | `PostProcessPushConstants` |
| 冻结回归 | 正面与近距离确定性 Capture 与性能采集 | `--capture-dir`, `--gpu-timing` |
| 性能脚本 | 使用 `balanced` 做 Timing 对比 | `BlackholeQualityParameters` |

## 典型调用链

```
--scene-type blackhole --blackhole-quality cinematic --blackhole-camera front
  → sceneTypeFromString → SceneRendererRegistry::create("blackhole")      ← 跨子系统：renderer_sdk
  → BlackholeSceneRenderer::capabilities（不要求 Depth/Normal）
  → onLoad：创建 Raw Trace / History A / History B / Framebuffer / Pipeline / Descriptor
每帧 recordScene：
  → recordShadowClear（仅为兼容公共 Shadow 附件）
  → recordTrace：全屏 Trace → Raw Trace Image
      → 相机基向量 + 引力曲率项逐步推进 → 盘体前向辐射累积 / Event Horizon 终止 / 远场采样环境
  → recordTemporal：Raw + History(prev) → History(cur)（blendWeight）
      → 四级亮部金字塔 Compute Bloom（kBloomLevelCount=4）                ← 跨子系统：render_core
  → recordComposite → 宿主 HDR Scene Color
resize / 相机 / 质量 / Capture 时间线跳变 → History 失效 → onSwapchainRecreate
```

## 实现约束清单

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------------------|
| `BlackholeQualityParameters`（Performance） | `600 / 1 / 0.85` | `RenderSettings.cpp` | 快速预览 | 性能对照必须写明档位，不能跨档混比 |
| `BlackholeQualityParameters`（Balanced） | `1100 / 1 / 0.65` | `RenderSettings.cpp` | 交互与 Timing 的默认对照档 | 正式性能比较优先使用该档 |
| `BlackholeQualityParameters`（Cinematic） | `1800 / 4 / 0.48` | `RenderSettings.cpp` | 截图与视频交付 | 多 Trace 通过像素内扰动降噪，成本近似随采样数增长 |
| `BlackholeCamera` | `0`–`4` | `RenderSettings.hpp` | 五个预设 | 预设只改位置/目标/路径，产出比例不变 |
| `BloomPushConstants` | `8` bytes | `BlackholeSceneRenderer.hpp`（`static_assert`） | 阈值 + 提取开关 | 与 `blackhole_composite`/Bloom Compute 的 push block 必须一致 |
| `kBloomLevelCount` | `4` | `BlackholeSceneRenderer.hpp` | 亮部金字塔层数 | 层数与图像尺寸绑定，图由 Render Graph 按级声明 |
| `TaaUniform::blendWeight` | `0.35` | `BlackholeSceneRenderer.hpp` | 时间混合权重 | `w=1` 表示完全丢弃 History；每帧重置会让 TAA 失效 |
| History 尺寸/格式 | 与渲染分辨率、HDR 格式一致 | `BlackholeSceneRenderer.hpp` | 两张 History 轮流读写 | 尺寸或格式变化必须重置，否则旧样本会按新格式解释 |

### 必须包含的协议字段

| 契约 | 字段 | 类型 | 说明 |
|------|------|------|------|
| Raw Trace Descriptor | binding 0 Uniform（Camera/Physics/Disk/Quality） | UBO | 与 `BlackholeUniform` 逐字段一致 |
| Raw Trace Descriptor | binding 1 Environment Texture | Sampler | 等距柱状或由六面 Cubemap 转换后的统一表示 |
| Temporal Descriptor | binding 0 Blend/Bloom/Render Width | UBO | 对应 `TaaUniform` |
| Temporal Descriptor | binding 1 Current Raw Frame | Sampler | 当帧 Trace 输出 |
| Temporal Descriptor | binding 2 Previous History | Sampler | 上一帧累积结果 |

### 必须实现的函数

| 函数名 | 所在文件 | 说明 |
|--------|---------|------|
| `recordShadowClear` | `BlackholeSceneRenderer.cpp` | 公共 Shadow 附件必须留下确定状态，即使本路径不使用它 |
| `recordTrace` / `recordTemporal` / `recordComposite` | `BlackholeSceneRenderer.cpp` | 三个 Pass 的录制顺序即历史帧正确性的前提 |
| `blackholeHistoryNeedsReset` | `RenderSettings.cpp` | History 失效判断的单一入口；质量、相机、尺寸变化都经它 |
| `onSwapchainRecreate` | `BlackholeSceneRenderer.cpp` | 重建尺寸相关资源并使 History 失效 |

### 边界约束（能做 / 禁止）

- 禁止：用按距离切分的固定步长档位替代连续自适应步长 → 会产生同心色块与采样边界（来源：`docs/blackhole-rendering.md`）。
- 禁止：把 `atan2` 角度直接送入普通 Perlin/Simplex 噪声；必须使用 `(cosθ·k, sinθ·k, r·k_r + h·k_h)` 的周期坐标，且厚度、云层、旋臂与尘埃全部使用同一周期坐标，只修一处仍会留接缝。
- 禁止：每帧生成独立的随机密度 → TAA 无法稳定；时间只允许推进同一个连续噪声场。
- 禁止：在 Trace 阶段提前 clamp 到 0-1 → 内盘与光子环高亮会变成无层次色块。
- 禁止：通过模糊、降低对比度或把接缝转到镜头背面来"规避"接缝回归。
- 禁止：把 Doppler 上限调得过低以"稳定画面" → 盘面两侧重新变得对称，验收不通过。
- 禁止：改盘面旋转方向而不更新速度符号 → 视觉运动与颜色不对称互相矛盾。
- 边界：黑洞已作为 P1 冻结；角色或宿主改动不得改变其 Shader、质量参数、相机语义与 History Reset（来源：`docs/archive/plans/engine-roadmap.md`、`docs/blackhole-rendering.md`）。
- 边界：公共 Attachment 或后处理变化后，必须重跑正面与近距离黑洞回归。
- 边界：Android 未适配不是黑洞路径的当前版本承诺；本实现是视觉近似，不是 Kerr/Schwarzschild 测地线求解器，文档与作品集不得宣称更高物理精度。

### 设计决策

| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 几何表达 | 全屏程序化积分，无黑洞网格 | 建模 + 光追管线 | 与宿主共用同一全屏路径，Renderer 只依赖标准 HDR 输出 |
| History 数量 | 两张 History 轮流读写 | 单张 History 原地更新 | 避免同一张图在一帧内既作输入又作输出 |
| 环境输入 | 统一内部等距柱状表示，GPU 侧共享采样函数 | 按输入类型分支采样 | 输入类型只在加载期选择，运行期不依赖文件名猜测 |
| 内部 Bloom | 复用 Compute 四级金字塔 | 使用宿主公共后处理 Bloom | 黑洞内部高亮是算法的一部分，最终仍经宿主 Tone Mapping 输出 |

## 变更风险

- 改 Trace 循环、步长函数或噪声坐标：正面与近距离画面对比同时失败，需先保存确定性基线，涉及着色算法调整必须单独立项。
- 改 History Reset 条件：少一处会留下旧画面拖影，多一处会让 TAA 完全失效；相机切换首帧混入旧视角是最典型的回归症状。
- 改质量档位参数或档位枚举：Capture Manifest 与性能报告中的档位语义随之后移，历史数据无法对比。
- 改环境解码或相机基向量：引力透镜方向与背景参照同时改变，纯黑或低信息背景会掩盖算法错误，验收至少需要正面静态图与近距离移动序列。
- 改公共 Attachment/后处理：黑洞回归必须重跑，否则可能出现"角色正常、黑洞漏光"的跨场景缺陷。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/blackhole-rendering.md`、`Project/AzureRender/docs/reference.md`、`Project/AzureRender/docs/runtime/compute-passes.md`、`Project/AzureRender/docs/plans/azure-engine-plan.md`（原文已提炼，非完整转录）
