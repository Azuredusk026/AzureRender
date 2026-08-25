# 资产、场景与编辑器

AzureRender 把“文件能被解析”和“资源具有正确渲染语义”分开处理。glTF 提供几何、动画和标准材质字段，AzureRender Material Profile 补充风格化类别、Feature、Face SDF 和类别相关参数；`.azscene` 再保存场景节点、资源引用和 RenderSettings。

## 资产边界

| 目录 | 用途 | 可进入公开仓库/CI | 可进入安装包 |
| --- | --- | ---: | ---: |
| `assets_public/` | 自制或可再分发测试资产 | 是 | 是 |
| `assets_private/` | 授权受限的本机美术 QA | 否 | 否 |
| `assets_placeholder/` | 缺失私有资产时的目录与说明 | 是 | 按需 |
| `portfolio/` | 精选公共截图、证据和哈希 | 是 | 默认否 |
| `captures/` | 可再生成的本机输出 | 否 | 否 |

私有角色即使没有直接提交原始模型，其派生截图、视频和重新打包 GLB 仍可能受原许可约束，不能自动视为公开作品集资产。公开 CI 使用 `assets_public/test_model.gltf`，保证任何贡献者都能复现基础渲染路径。

## glTF 导入流程

```mermaid
flowchart LR
    Source[DCC / exported glTF] --> Parse[tinygltf parse]
    Parse --> Standard[mesh / skin / animation / PBR]
    Parse --> Profile[azureRenderMaterial extras]
    Standard --> Validate[loader validation]
    Profile --> Validate
    Validate --> CPU[LoadedAsset]
    CPU --> Upload[GPU buffers / images / descriptors]
    Upload --> Render[Character Renderer]
```

Loader 支持 `.gltf` 和 `.glb`，读取 Buffer、Accessor、Primitive、Node、Skin、Animation、Texture、Sampler 和 Material。导入时检查：

- Accessor 范围、Stride 和 Component Type。
- Index 是否引用合法顶点。
- Normal/Tangent/UV 是否满足当前 Pipeline。
- Joint/Weight 数量与权重归一化。
- Texture 与 Image 引用是否存在。
- Alpha Mode、Double Sided 和材质类别是否兼容。
- AzureRender Profile Schema 和 Face SDF Profile 是否完整。

失败应包含资产、Material 或 Primitive 上下文。对于可选数据可以使用明确回退；结构损坏和未知未来 Schema 必须拒绝。

## Material Profile v1

Profile 位于 glTF Material 的 `extras.azureRenderMaterial`。简化示例：

```json
{
  "schemaVersion": 1,
  "class": "hair",
  "features": ["stylized-shadow", "hair-anisotropy"],
  "styleParameters": [1.0, 1.0, 0.5, 0.0],
  "featureParameters": [1.0, 1.0, 0.0, 0.0]
}
```

Schema 只定义结构和值域；向量各分量的意义由类别决定。这样能保持资产格式紧凑，但也意味着工具和文档必须在写入时知道目标类别。通用 Lit 材质的 Style Vector 表示 Toon、Shadow、Specular、Rim 一类参数；Brow Overlay 则把同一字段解释为局部厚度/偏移和保留值。

Material Class 与 Feature 完整枚举见[参数与接口参考](reference.md)。修改 Profile 时运行：

```powershell
python .\tools\validate_material_profiles.py .\assets_public\test_model.gltf
```

JSON Schema 位于 `schemas/azure_render_material.schema.json`，它是外部工具的权威结构定义；C++ Loader 是运行时权威。两者必须同步。

## 纹理与颜色空间

颜色纹理和数据纹理必须区分：

| 数据 | 典型颜色空间 | 说明 |
| --- | --- | --- |
| Base Color / Emissive | sRGB | 采样后转换到线性空间参与光照 |
| Normal / HN | Linear | 通道是向量数据，禁止 sRGB 解码 |
| Metallic/Roughness/Packed P | Linear | 通道是标量数据 |
| Face SDF / Mask / AO | Linear | 距离或遮罩数据 |
| HDR Environment | Linear HDR | 保留大于 1 的能量 |

Normal Map 使用 Tangent Space。缺失 Tangent 时 Loader 可基于 Position、UV 和 Normal 生成，但镜像 UV、退化 UV 和不连续顶点需要显式处理。BC5 等双通道法线在离线工具中转换或解码，不能当普通彩色 PNG 使用。

角色头发的 HN 与 P 具有专用命名和 Binding，不能只依赖常规 `T_RGBA_P` 规则。导入工具应输出材质、纹理索引和通道审计，证明数据真正进入最终 GLB。

## Face SDF 资产契约

Face 材质除 `face-sdf-eligible` 外，还需要：

```json
{
  "faceSdf": {
    "schemaVersion": 1,
    "texture": 12,
    "texCoord": 0,
    "channel": "r",
    "maskChannel": "a",
    "shadowOnLowValues": true,
    "horizontalAxis": "left-to-right",
    "headNode": "Head"
  }
}
```

运行时会验证 Texture、UV、通道、方向和 Head Node。资产兼容审计：

```powershell
python .\tools\audit_face_sdf_compatibility.py `
  .\path\to\character.glb --require-compatible
```

材质 Feature 只表示“允许使用”；Profile 和有效 Texture 才表示“数据已提供”。这一区分避免 UI 显示 Face SDF 已开启，但 Shader 实际采样 2x2 回退纹理。

## 骨骼、蒙皮和动画

Skin 包含 Joint Node 和 Inverse Bind Matrix。每帧最终矩阵概念为：

\[
M_{joint}=M_{node,current}\,M_{inverseBind}
\]

顶点按最多支持的 Joint/Weight 组合混合。导入审计应确认每个顶点权重和接近 1、Joint Index 合法、左右语义骨骼没有误配、Bind Pose 不产生突跳。

动画 Channel 分别驱动 Translation、Quaternion Rotation 和 Scale，Sampler 决定时间与插值。角色展示可以暂停、重启或按固定 Capture FPS 推进；确定性捕获不能直接使用不稳定的墙钟时间。

眉毛/睫毛等 Overlay 可能是同一 Primitive 内的多个拓扑小岛，且由眼、睫毛和眉毛骨骼分别控制。几何加粗必须识别眉毛顶点或局部小岛，不能对整个 Primitive 做单中心缩放。使用 `tools/audit_brow_mesh.js` 检查拓扑、退化三角形、权重和骨骼影响。

## 环境资产

`EnvironmentAsset` 支持：

- 单张等距柱状 HDR/LDR 图。
- 包含六面的目录。

Cubemap 面会按统一方向约定转换为内部等距柱状图，GPU 侧使用共享采样函数。环境资源输出 RGBA16F，保持 HDR 能量。自动投影只依据输入类型选择，不应依赖本机文件名猜测场景。

开发机可传私有 HDRI 做视觉 QA，但发布默认必须有公共、可定位的回退资源。场景文档不能写入作者电脑的绝对路径作为必需依赖。

## Toon Ramp 与 Showcase Look

`toon_ramp_profiles.json` 是分类 Ramp 的版本化源数据，`toon_ramp_atlas.ppm` 是生成结果：

```powershell
python .\tools\build_toon_ramp_atlas.py --check
```

`showcase_looks.json` 保存五套 Character Look。Look 只包含 Grade、Bloom 和 Outline；它不拥有背景、地台、灯光、材质分类或 Face SDF 数据。运行时校验 Schema 和条目数量，避免 C++ 默认值与 JSON 长期分叉。

## `.azscene` 场景文档

当前 Scene Schema 为 v2。`SceneDocument` 保存：

```text
sceneId
resources[]
  id, type, path
nodes[]
  id, name, parentId, resourceId
  visible
  translation, rotation, scale
  prefabSource, instanceOf
renderSettings
```

`renderSettings.sceneType` 是 Renderer 选择器，因此 Character/Blackhole 属于数据状态，而不是编辑器另存的隐藏开关。v1 文件可迁移到 v2；未知未来版本拒绝加载。

场景保存先写同目录临时文件，再原子替换目标，避免进程中断截断有效文件。`prefabSource` 和 `instanceOf` 当前保存引用与覆盖 Transform，不代表已经实现独立 Prefab 文件展开系统。

## 编辑器组成

Dear ImGui 层提供：

- Scene Outliner：节点层级、选择、可见性和增删。
- Inspector：Transform、Renderer、Look、Face SDF、Shadow、Outline 等设置。
- Asset Browser：资源 ID、路径、存在状态和依赖数。
- Viewport：编辑器相机、Picking、Gizmo 和 Capture。
- Diagnostics/HUD：资源、动画、Renderer、GPU Timing 和错误。

编辑器操作通过 `EditorSession` 和 `EditorContext` 修改 `SceneDocument`，而不是让 Panel 直接持有 Vulkan Handle。Renderer 通过标准化 `RendererSceneState` 暴露可 Pick 资产、模型矩阵、Primitive 数量和选择状态。

## Undo 与 Redo

Undo/Redo 保存完整 `SceneDocument` 快照：

- `Ctrl+Z` 恢复上一状态。
- `Ctrl+Y` 重做被撤销状态。
- 节点、名称、可见性、Transform、Prefab/Instance 和 RenderSettings 都进入历史。
- 新编辑清空 Redo。
- 重新加载场景清空历史。
- 历史上限为 100 个快照。

完整快照比细粒度 Command 简单且可靠，适合当前场景规模；未来大场景需要差量历史和资源级事务。

## 资产热重载

Asset Browser 通过 `last_write_time` 检测变化，但不会启动后台文件监听。用户显式执行 Reload Assets 后：

1. 在主线程记录 Reload Request。
2. 到安全帧边界等待 GPU idle。
3. 调用活动 Renderer `onUnload()`。
4. 重新定位并加载资产。
5. 调用 `onLoad()` 创建新 GPU 资源。

等待 Device Idle 保证正确性，但会产生明显停顿，因此只适合显式低频操作。热重载不会修改 `.azscene` 路径，也不会自动替换 Missing 资源。

## Capture 与视觉证据

编辑器 Capture 标签只允许字母、数字、连字符和下划线，并输出到本地 `captures/`。它是工作图，不自动成为作品集证据。

正式证据应包含：

- PNG 或 MP4。
- 场景、相机、分辨率、帧率和 RenderSettings。
- Capture Manifest。
- GPU/驱动和构建类型（性能证据需要）。
- SHA-256。
- 对比图或预期观察项。

公开媒体建议使用稳定语义名，例如 `character_face-sdf_comparison_2560x1440.png`，而不是 `final_v7_new.png`。原始批量捕获可以使用时间戳目录，但进入 `portfolio/` 时应采用稳定名称并由 Manifest 管理。

## 视觉 QA 矩阵

### Character

- 全身正面、脸部正面、三分之四和背面。
- 完整转台中的动画、透明排序和阴影连续性。
- Stylized On/Off。
- Albedo、Normal、Depth、Shadow Visibility、Hair KK、Face SDF、Outline。
- 近景检查眉毛、眼睛和发丝；远景检查轮廓和材质分区。

### Blackhole

- Front 和 Close/Over-Shoulder。
- Photon Ring、Lens、Doppler 不对称和盘面接缝。
- 时间序列中的噪声、拖影和 History Reset。
- Balanced Timing 与 Cinematic 视觉交付分开记录。

自动化负责格式、Schema、数学、资源定位和确定性数据；人工负责构图、层次、噪声、色带和美术判断。两者不能互相代替。

## 导入新资产的检查表

1. 确认许可证和公开范围。
2. 检查模型单位、坐标、UV、法线、切线和拓扑。
3. 检查 Skin、Weight、Animation 和 Bind Pose。
4. 为每个 Material 写入明确 Class 与 Feature。
5. 标记数据纹理颜色空间和通道。
6. 验证 Face SDF、Hair HN/P、Overlay 等专用资源。
7. 运行 Profile、Face SDF 和眉毛审计工具。
8. 使用 Debug Validation 加载。
9. 生成固定视图与隔离图。
10. 只有公共资产才进入 CI、安装包和作品集。
