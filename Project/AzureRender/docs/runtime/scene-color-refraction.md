# 场景颜色折射

阶段索引为 NR2，公共入口为 glTF 材质配置。
透明表面可读取同帧不透明场景的 HDR 颜色。
偏移由独立线性贴图的 RG 通道控制。
具体材质来源和参数由项目资产持有。

## 配置

在材质的 `extras.azureRenderMaterial` 中配置：

```json
{
  "schemaVersion": 2,
  "class": "generic",
  "features": ["scene-refraction"],
  "sceneRefraction": {
    "texture": 0,
    "uvTransform": [0.7, 0.7, 0, 0],
    "tint": [1, 1, 1, 1],
    "contribution": 0.8,
    "brightness": 0.271,
    "distortionStrength": 0.008
  }
}
```

材质的 `alphaMode` 使用 `BLEND`。
`texture` 引用同一 glTF 的纹理索引。
默认使用 UV0，变换依次为缩放与偏移。
特征组合支持表面遮蔽和发光通道。

| 字段 | 默认值 | 合法范围 |
| --- | --- | --- |
| `texture` | 必填 | 有效整数纹理索引 |
| `uvTransform` | `[1,1,0,0]` | 四个有限值，绝对值至多 65536 |
| `tint` | `[1,1,1,1]` | 四个有限非负值，至多 65536 |
| `contribution` | `0.8` | 零至一 |
| `brightness` | `1` | 零至 64 |
| `distortionStrength` | `0` | 零至一，单位为屏幕 UV |

色调的 RGB 用于场景颜色，Alpha 保留供格式扩展。
表面输出透明度采用源基色纹理与因子的 Alpha。
材质对象、队列、索引及数值错误进入导入诊断。

## 消费公式

```text
offset = (sampleLinear(distortionMap, uv0 * scale + shift).rg * 2 - 1) * strength
weight = saturate((1 - alpha) / max(1 - contribution, 0.01))
background = sampleSceneColor(depthSafeUV) * tint.rgb * brightness
surface = diffuseLighting + lerp(specularLighting, background, weight) + emission
```

深度保护先尝试完整偏移，再尝试四分之一偏移。
背景线性深度大于等于片元深度时采用该坐标。
两个偏移均被前景遮挡时使用原屏幕坐标。
场景颜色采用双线性过滤与镜像寻址。
深度快照使用最近点采样及镜像屏幕坐标。

调色板保持既有 128 字节材质常量布局。
折射特征使用独立的常量解释规则。
导入侧将命名参数映射至该规则。
项目配置以命名对象表达参数。

| GPU 存储 | 折射语义 |
| --- | --- |
| `styleParameters` | UV 缩放与偏移 |
| `hairParameters.xyz` | 贡献、亮度与偏移强度 |
| `matcapColor` | 折射色调 |
| 纹理槽 4 | 线性偏移图 |

## 生命周期与边界

场景渲染器拥有不透明颜色和线性深度快照。
可见折射材质触发快照绘制。
渲染图记录写入到采样的依赖和布局切换。
透明绘制完成后随场景资源统一释放。

折射读取不透明表面，不包含透明表面递归。
透明混合与排序使用既有透明队列。
物理折射率、厚度和壳体模式属于后续扩展。
雾、曝光及原生表面光照由各自机制验收。

## 验证

`AzureRender.SurfaceResponseGpu` 包含 38 个数值探针。
折射部分覆盖权重、混色、深度退让和镜像坐标。
导入测试覆盖线性像素保留及非法参数拒绝。
透明队列限制也进入导入回归。

`AzureEngine.SceneRefractionGpu` 使用公开平面夹具。
四组捕获覆盖无遮挡、透射、亮度响应及固定描述符。
无偏移且亮度为一时，透射与无遮挡背景逐像素相等。
固定描述符与索引描述符的四帧检查均无校验错误。

本机证据根为 `build/runs/20261017-175206-nr-playable-integration-d4e789/`。
公开夹具收据为 `public-scene-refraction-v2/summary.json`。
原生公式依据见[玻璃来源调查](../research/2026-10-18-native-glass-consumer.md)。
