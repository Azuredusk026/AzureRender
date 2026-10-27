# 无光照纹理图层

本页定义资产模块的图层解析与数值评价。
公共类型为 `UnlitTextureLayers`。
材质加载、GPU 绑定及实际绘制按独立阶段验收。
阶段结果见[图层契约验收](../acceptance/nr/unlit-texture-layer-contract.md)。

## 数据契约

`parseUnlitTextureLayers` 读取完整的版本二表面对象。
调用方明确提供纹理数量。
解析结果拥有自己的参数数据。
未知字段、无效类型和非法纹理索引产生异常。

| 字段 | 内容 |
| --- | --- |
| `schemaVersion` | 整数二 |
| `lighting` | `unlit` |
| `layers` | 一至两层，首层角色为 `base` |
| `layers[].role` | 第二层采用 `mask` 或 `blend` |
| `layers[].texture` | 非负整数，位于调用方纹理范围内 |
| `layers[].sampleMode` | `rgba` 或 `red-as-alpha` |
| `layers[].uv.set` | 整数零 |
| `layers[].uv.scaleOffset` | 缩放 XY 与偏移 XY |
| `layers[].uv.rotation` | 四个旋转矩阵分量 |
| `layers[].uv.speed` | 每秒 UV 位移 |
| `tint`、`blendTint` | 独立 RGBA 倍率 |
| `clock` | `rate` 与 `offsetSeconds` |
| `softIntersection` | 正数 `distance` 及有限 `bias` |
| `highlightExpansion` | 非负 `threshold` 与 `gain` |
| `postExposureWeight` | 零至一的曝光权重 |
| `globalColorWeight` | 零至一的全局颜色权重 |

全部字段明确提供，数值须有限且能由单精度表示。
每一层独立拥有 UV、角色及采样模式。
纹理资源、采样器及材质兼容性由调用方校验。

## 表现时间与坐标

材质时间为表现时间乘时钟速率，再加相位偏移。
`evaluateUnlitMaterialTime` 使用调用方提供的时间。
暂停与回放采用相同表现时间即可得到相同结果。
主机时钟及时间原点由宿主管理。

`evaluateUnlitLayerUv` 按以下顺序评价每层坐标。
旋转分量的排列与公式共同构成公开契约。
函数输出采样前坐标，寻址方式由纹理采样器决定。

```text
d = uv0 + speed * materialSeconds - 0.5
rotated.x = d.x * rotation.x + d.y * rotation.z + 0.5
rotated.y = d.x * rotation.y + d.y * rotation.w + 0.5
sampleUv = rotated * scaleOffset.xy + scaleOffset.zw
```

## 颜色与交界

输入采样和输出均采用线性、独立 RGB 与透明度。
`red-as-alpha` 将采样解释为白色 RGB 和红通道透明度。
基础采样先乘 `tint`。
Mask 将两个采样的四个通道分别相乘。

Blend 权重为基础透明度与第二层透明度之和。
该和先乘 `blendTint.a`，再限制到零至一。
第二层 RGB 按该权重及颜色倍率累加。
基础透明度继续独立保存。

亮部扩展增加超过阈值的 RGB，再应用曝光权重。
结果先限制到零至一千，再进行全局颜色评价。
全局颜色使用亮度、饱和度和 RGB 增益。
曝光与颜色参数由 `UnlitEffectFrame` 提供。

`evaluateSoftIntersection` 使用同单位的视图深度。
交界权重为深度绝对值之差，加偏移后除以距离。
该值限制到零至一。
最终透明度恰好乘一次交界权重与节点透明度。

```text
soft = saturate((abs(sceneViewZ) - abs(effectViewZ) + bias) / distance)
alpha = baseAlpha * soft * nodeOpacity
luminance = dot(rgb, [0.212673, 0.715152, 0.072175])
mapped = (luminance + saturation * (rgb - luminance)) * colorGain
rgb = mix(rgb, mapped, globalColorWeight)
```

节点透明度和交界权重的输入域为零至一。
曝光及饱和度为有限非负数。
评价函数明确拒绝非有限时间、采样与结果。
调用方负责将独立透明度交给相应的混合管线。
