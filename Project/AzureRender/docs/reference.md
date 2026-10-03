# 参数与接口参考

本页集中保存适合查表的稳定契约。设计说明放在其他主题文档中。如果两处内容冲突，以 `CommandLine.cpp`、`RenderSettings.hpp`、Schema、Shader 和自动化测试为准。

## 命令行

### 通用与资源

| 参数 | 值 | 说明 |
| --- | --- | --- |
| `--help` | 无 | 输出帮助并以 0 退出 |
| `--version` | 无 | 输出版本并以 0 退出 |
| `--check-resources` | 无 | 检查安装/开发资源树 |
| `--resource-root` | 目录 | 显式资源根 |
| `--asset` | `.gltf/.glb` | Character 资产。创建场景时必需 |
| `--environment` | 图片或目录 | 等距柱状图或六面 Cubemap 目录 |
| `--smoke-frames` | 正整数 | 渲染固定帧数后退出 |

非法参数输出 Usage 并返回退出码 2。参数值不能缺失或以下一个 `--option` 代替。

### 场景与编辑器

| 参数 | 值 | 说明 |
| --- | --- | --- |
| `--scene-type` | `character/blackhole/sample` | 选择内置 Renderer |
| `--scene` | `.azscene` | 加载场景运行 |
| `--create-scene` | `.azscene` | 从 `--asset` 创建场景 |
| `--editor` | `.azscene` | 打开编辑器 |

`--scene`、`--create-scene` 和 `--editor` 互斥。`--create-scene` 必须配合 `--asset`。

### 输出、Capture 与 Timing

| 参数 | 值 | 说明 |
| --- | --- | --- |
| `--width` | 64-7680 | 输出宽度，默认 1280 |
| `--height` | 64-4320 | 输出高度，默认 720 |
| `--capture-dir` | 空目录 | 确定性 PNG 输出目录 |
| `--capture-frames` | 正整数 | 捕获帧数 |
| `--capture-fps` | 1-240 | 固定模拟帧率，默认 60 |
| `--portfolio` | 无 | 使用作品集演示时间线 |
| `--technical-sequence` | 无 | 五章技术序列并启用 Portfolio/Timing |
| `--gpu-timing` | 无 | 启用 Timestamp Query |
| `--gpu-timing-output` | JSON 路径 | 启用 Timing 并写 JSON |
| `--hud` | 无 | 显示 HUD，同时启用 Timing |

`--capture-dir` 与 `--capture-frames` 必须一起使用。Technical Sequence 至少需要 5 帧，总帧数还要能被 5 整除。它不能与 `--qa-*` 组合。

### Character QA

| 参数 | 可选值 |
| --- | --- |
| `--qa-camera` | `full-body-front`、`face-front`、`face-three-quarter`、`back-detail`、`lighting-sweep` |
| `--qa-light` | `neutral-material`、`stylized-key`、`specular-rim`、`rear-emissive` |
| `--qa-effect` | `toon`、`shadow`、`hair-kk`、`rim`、`specular`、`emissive`、`outline`、`face-sdf`、`overlay`、`bloom` |
| `--qa-effect-state` | `enabled`、`disabled`、`isolation` |
| `--qa-isolation` | 见下表 |
| `--no-stylized` | 关闭风格化光照 |
| `--no-inner-outline` | 关闭内部描边 |
| `--diagnostic-view` | `beauty`、`normal`、`outline`、`shadow` |

`--qa-effect-state` 必须配合 `--qa-effect`。

QA Isolation：

```text
beauty             albedo              world-normal
depth              diffuse-band        shadow-visibility
hair-kk            rim                 specular
emissive           outline             shadow-map
material-id        style-mask          ambient
direct-diffuse     shadow-tint         face-sdf
overlay            bloom
```

### Blackhole

| 参数 | 可选值 |
| --- | --- |
| `--blackhole-quality` | `performance`、`balanced`、`cinematic` |
| `--blackhole-camera` | `front`、`orbit-left`、`high`、`close`、`over-shoulder` |

质量参数：

| 档位 | Max Steps | Samples/Pixel | Near Step Scale |
| --- | ---: | ---: | ---: |
| Performance | 600 | 1 | 0.85 |
| Balanced | 1100 | 1 | 0.65 |
| Cinematic | 1800 | 4 | 0.48 |

### Render Path

`--render-path` 接受：

- `traditional`：独立 Render Pass。
- `subpasses`：Subpass 路径选择。
- `dynamic`：Dynamic Rendering 路径选择。

它们属于渲染路径对照契约。性能报告必须记录所选值，不能混合比较。

## 角色快捷键

| 按键 | 行为 |
| --- | --- |
| `1`-`4` | 固定全身方向机位 |
| `5` | 脸部近景 |
| `6` | Portfolio 环绕镜头与风格化展示预设 |
| `Left/Right` | 微调模型旋转 |
| `Space` | 暂停/继续自动旋转 |
| `R` | 切换自动旋转 |
| `F1` | Azure Gallery |
| `F2` | Endfield Industrial |
| `F3` | Neutral Material Check |
| `F4` | 暂停/继续动画 |
| `F5/F6` | 调整 Diffuse Band |
| `F7/F8` | 调整 Style Mask |
| `F9` | 切换风格化光照 |
| `F10` | 切换内部描边 |
| `F11` | 重启动画 |
| `F12` | 保存 PNG |
| `H` | 显示/隐藏 HUD |

编辑器另使用 `Ctrl+Z`/`Ctrl+Y` 执行 Undo/Redo。

## RenderSettings v7

| 字段 | 默认值 | 有效范围/语义 |
| --- | --- | --- |
| `sceneType` | Character | Character/Blackhole/Sample |
| `renderPath` | Traditional | Traditional/Subpasses/Dynamic |
| `stylizedLightingEnabled` | true | Character 风格化总开关 |
| `styleMaskStrength` | 1.0 | 0-2 |
| `diffuseBandThreshold` | 0.40 | 0.05-0.95 |
| `showcasePreset` | 0 | Look Catalog 索引 |
| `innerOutlineEnabled` | true | 屏幕空间描边 |
| `silhouetteOutlineEnabled` | true | 几何背面壳描边 |
| `diagnosticView` | 0 | Renderer 诊断索引，0 必须是 Beauty |
| `morphWeights` | `[0,0]` | 两个当前 Morph Target Weight |

### FaceSdfSettings

| 字段 | 默认值 | 范围 |
| --- | ---: | ---: |
| `enabled` | true | Boolean |
| `mirrorHorizontal` | false | Boolean |
| `threshold` | 0.50 | 0-1 |
| `softness` | 0.08 | 0.001-0.5 |
| `noseShadowStrength` | 0 | 0-1 |
| `jawShadowStrength` | 0 | 0-1 |
| `shadowColor` | `[0.55,0.36,0.34,0.55]` | 每通道 0-1 |

### Shadow、Bloom、Outline 与 Grade

| 字段 | 默认值 | 范围 |
| --- | ---: | ---: |
| `shadow.maximumFilterRadiusTexels` | 8 | 1-16 |
| `bloom.enabled` | true | Boolean |
| `bloom.threshold` | 1.05 | 0-8 |
| `bloom.strength` | 0.16 | 0-2 |
| `outline.strength` | 0.40 | 0-2 |
| `outline.depthThreshold` | 0.18 | 0.001-2 |
| `outline.normalThreshold` | 0.20 | 0.001-1 |
| `outline.color` | `[0.008,0.013,0.022]` | 每通道 0-8 |
| `grade.exposureEv` | 0 | -8 至 8 |
| `grade.saturation` | 1 | 0-2 |
| `grade.contrast` | 1 | 0-2 |
| `grade.tint` | `[1,1,1]` | 每通道 0-2 |
| `grade.toneMappingEnabled` | true | Boolean |

`characterPresentation` 还包含 Background 和 Platform 开关。`blackhole` 保存 Quality 和 Camera。

## Showcase Look Catalog v1

| 索引 | 名称 | 用途 |
| ---: | --- | --- |
| 0 | Azure Gallery | 默认公共展示与回归 |
| 1 | Endfield Industrial | 低饱和、冷环境角色展示 |
| 2 | Neutral Material Check | 降低 Bloom/轮廓干扰的材质检查 |
| 3 | Specular Rim | 镜面和 Rim 检查 |
| 4 | Rear Emissive | 背面与自发光检查 |

Catalog 文件只拥有 Grade、Bloom 和 Outline 数据。

## Material Profile v1

### Material Class

| 数值 | 名称 |
| ---: | --- |
| 0 | `generic` |
| 1 | `skin` |
| 2 | `face` |
| 3 | `hair` |
| 4 | `fabric` |
| 5 | `metal` |
| 6 | `eye` |
| 7 | `overlay` |
| 8 | `emissive` |
| 9 | `showcase` |

### Feature Bit

| Bit | 名称 |
| ---: | --- |
| `1 << 0` | `stylized-shadow` |
| `1 << 1` | `hair-anisotropy` |
| `1 << 2` | `face-sdf-eligible` |
| `1 << 3` | `emissive-mask` |
| `1 << 4` | `overlay` |
| `1 << 5` | `neutral-fallback` |
| `1 << 6` | `brow-overlay` |

### Alpha Mode

| 数值 | glTF 名称 |
| ---: | --- |
| 0 | Opaque |
| 1 | Mask |
| 2 | Blend |

### Face SDF Profile

必需字段包括 `schemaVersion`、`texture`、`texCoord=0`、`channel` 和 `maskChannel`。还需要 `shadowOnLowValues`、`horizontalAxis` 和 `headNode`。通道可以是 `r/g/b/a`。水平方向可以是 `left-to-right` 或 `right-to-left`。

## Character Descriptor Set 0

| Binding | 类型 | 语义 |
| ---: | --- | --- |
| 0 | Uniform Buffer | Model、MVP、Light MVP、Camera、QA、Face SDF |
| 1 | Combined Image Sampler | Base Color |
| 2 | Combined Image Sampler | Normal |
| 3 | Combined Image Sampler | Metallic/Roughness/Packed P |
| 4 | Combined Image Sampler | Environment |
| 5 | Combined Image Sampler | Specular/Emissive |
| 6 | Combined Image Sampler | Style Mask |
| 7 | Combined Image Sampler | Matcap |
| 8 | Combined Image Sampler | Hair HN/Data |
| 9 | Combined Image Sampler | Shadow Map |
| 10 | Storage Buffer | Joint Matrix（Vertex Stage） |
| 11 | Combined Image Sampler | Toon Ramp Atlas |
| 12 | Combined Image Sampler | Face SDF |

Material Push Constant 保存 Alpha、Emissive、AO、Lam Shadow 和 Matcap。它还保存 Hair、Style、Feature Parameters、Class、Feature Bit 和 Profile Version。Vertex Push Range 另有 Morph Weight 与 Gizmo Transform。

## 最终 Composite Descriptor

| Binding | 语义 |
| ---: | --- |
| 0 | Normal Texture |
| 1 | Depth Texture |
| 2 | Shadow Texture |
| 3 | HDR Scene Color |

Push Constant 保存 Outline Strength/Threshold、Diagnostic View、Exposure、Tone Mapping、Bloom、Outline Color、Grade 参数和 Tint。

## Blackhole Descriptor

Raw Trace：

| Binding | 语义 |
| ---: | --- |
| 0 | Camera、Physics、Disk 和 Quality Uniform |
| 1 | Environment Texture |

Temporal：

| Binding | 语义 |
| ---: | --- |
| 0 | Blend/Bloom/Render Width Uniform |
| 1 | Current Raw Frame |
| 2 | Previous History |

## Renderer SDK v1

生命周期：

```text
capabilities -> onLoad -> (updateFrame -> recordScene)*
                         -> onSwapchainRecreate -> ... -> onUnload
```

`SceneRendererCapabilities`：

- `requiresSceneDepth`，默认 true。
- `requiresSceneNormal`，默认 true。
- `diagnosticViewNames`，不能为空且第 0 项必须为 `Beauty`。

`SceneFrameData` 提供时间、RenderSettings、相机、旋转、QA、Capture、编辑器选择/Gizmo 和 Swapchain 尺寸。

`RenderContext` 提供 Device、Physical Device、Graphics Queue/Family 和 Command Pool。它也提供当前 Command Buffer、格式、Extent、公共 Render Pass 和 Framebuffer。其他字段包括 Shadow Map、环境、Shader 目录、Ramp 和 Timing Query。Renderer 只借用这些对象，不能销毁它们。

内置 Renderer：

| ID | API | Capabilities |
| --- | ---: | --- |
| `character` | 1 | geometry、editor、capture |
| `blackhole` | 1 | fullscreen、temporal、capture |
| `sample` | 1 | sdk-example、capture |

## `.azscene` v2

场景包含 `sceneId`、`resources[]`、`nodes[]` 和 `renderSettings`。节点字段：

```text
id, name, parentId, resourceId, visible
translation[3], rotation[3], scale[3]
prefabSource, instanceOf
```

保存时先写入同目录临时文件，再原子替换目标。v1 可以迁移。程序会拒绝未知的未来版本。

## 版本索引

| 契约 | 版本 |
| --- | ---: |
| 应用 | 0.1.0-rc1 |
| Renderer SDK / Registry API | 1 |
| RenderSettings | 7 |
| `.azscene` | 2 |
| Material Profile | 1 |
| Face SDF Profile | 1 |
| Showcase Look Catalog | 1 |
| Toon Ramp Profiles | 1 |
| `.azureproject` / `.azurelevel` / `.azureprefab` | 1 |
| `game_manifest.json`（AzureGame） | 1 |

## 权威代码位置

| 主题 | 位置 |
| --- | --- |
| CLI | `src/app/CommandLine.cpp` |
| App 与帧循环 | `src/app/AzureRenderApp.*`、`AzureRenderFrame.cpp` |
| Vulkan 资源 | `src/app/AzureRenderResources.cpp`、`AzureRenderPipeline.cpp`、`AzureRenderDescriptors.cpp` |
| Capture/Timing | `src/app/AzureRenderCapture.cpp` |
| Renderer 契约 | `src/extensions/ISceneRenderer.hpp`、`src/render/RenderContext.hpp` |
| Registry | `src/extensions/ExtensionRegistry.hpp`、`src/scenes/BuiltinRendererCatalog.cpp` |
| Settings | `src/render/RenderSettings.*` |
| glTF | `src/assets/GltfLoader.*` |
| Character | `src/scenes/CharacterSceneRenderer.*`、`shaders/mesh.*` |
| Blackhole | `src/scenes/BlackholeSceneRenderer.*`、`shaders/blackhole*.frag` |
| Composite | `shaders/inner_outline.frag` |
| Scene | `src/editor/SceneModel.*` |
| Material Schema | `schemas/azure_render_material.schema.json` |
