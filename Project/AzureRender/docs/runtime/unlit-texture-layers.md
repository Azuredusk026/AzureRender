# 无光照纹理图层

本页定义公共无光照图层及实际材质消费。
公共类型为 `UnlitTextureLayers`。
数值入口见[图层契约验收](../acceptance/nr/unlit-texture-layer-contract.md)。
实际绑定见[图层材质验收](../acceptance/nr/unlit-texture-material.md)。
来源转换与实际截帧输入见[物流图层来源](../acceptance/nr/captured-vfx-layers.md)。
层级采样、覆盖率和活动实例见[绘制验收](../acceptance/nr/vfx-mip-and-coverage.md)。
普通不透明无光照内容按基色与自发光评价最终颜色。
实际资源、四倍采样成本和附件对照见[预算验收](../acceptance/nr/unlit-opaque-budget.md)。

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
| `viewDepthFade` | 可选对象，`near` 和 `far` 各含起止深度 |
| `vertexColor` | 可选布尔值，默认假，选择线性顶点 RGBA 乘法 |
| `viewCoverage` | 可选视角覆盖率对象 |
| `composite` | 可选 `alpha` 或 `additive`，默认 `alpha` |
| `fogMode` | 可选 `none` 或 `scene`，默认 `none` |
| `coverageLayer` | 可选覆盖率纹理、独立 UV、阈值及增益 |
| `baseUvDistortion` | 可选基础坐标扰动纹理、独立 UV 及线性映射 |

必需字段明确提供，数值须有限且能由单精度表示。
每一层独立拥有 UV、角色及采样模式。
纹理资源、采样器及材质兼容性由调用方校验。
图层材质通过完整表面文档提供参数。
通用 PBR 参数覆盖对该模型产生明确的失败诊断。

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
启用顶点颜色时，再乘插值后的线性 RGBA。
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
材质透明度先乘交界权重，再限制到零至一。
最终透明度恰好乘一次节点透明度。
启用视图深度淡出时，交界权重再乘近端与远端权重。
每段范围非负且严格递增，单位与视图深度一致。
近端逐渐显现，远端逐渐消隐。

```text
soft = saturate((abs(sceneViewZ) - abs(effectViewZ) + bias) / distance)
nearFade = saturate((abs(effectViewZ) - nearStart) / (nearEnd - nearStart))
farFade = saturate((farEnd - abs(effectViewZ)) / (farEnd - farStart))
alpha = saturate(baseAlpha * soft * nearFade * farFade) * nodeOpacity
luminance = dot(rgb, [0.212673, 0.715152, 0.072175])
mapped = (luminance + saturation * (rgb - luminance)) * colorGain
rgb = mix(rgb, mapped, globalColorWeight)
```

节点透明度和交界权重的输入域为零至一。
曝光及饱和度为有限非负数。
评价函数明确拒绝非有限时间、采样与结果。
调用方负责将独立透明度交给相应的混合管线。
默认画像的深度淡出权重为一。
CPU 评价在启用该字段时要求明确的视图深度。
实际片元从世界位置及相机前向计算相同深度。

## 视角覆盖率与合成

`viewCoverage` 提供四个有限参数。
`bias` 为法线视线点积的偏置。
`exponent` 为正数，`invertWeight` 和 `weight` 位于零至一。
缺少该对象时，视角覆盖率为一。

输入法线采用顶点归一化后的片元插值。
视线在透视相机下指向相机位置。
正交相机采用相机前向的反方向。
法线保留表面来源方向。

```text
power = pow(saturate(dot(interpolatedNormal, viewDirection) + bias), exponent)
response = mix(1 - power, power, invertWeight)
coverage = mix(1, response, weight)
baseAlpha = saturate(baseAlpha) * coverage
```

CPU 评价在启用该对象时要求明确的点积。
`softIntersection.enabled` 默认为真。
该字段为假时，场景深度交界权重为一。
近远淡出继续使用独立的视图深度。

`alpha` 合成按覆盖率混合前景与背景。
`additive` 合成将颜色乘覆盖率后加到背景。
加色路径保留颜色目标的透明度。
节点透明度在两种路径中均消费一次。

## 覆盖率纹理

`coverageLayer` 提供独立的透明度输入。
该对象必需提供 `texture`、`sampleMode` 和 `uv`。
其 UV 字段与颜色图层采用相同契约。
`threshold` 为有限阈值，`gain` 为有限正数。

`rgba` 使用采样透明度，`red-as-alpha` 使用红通道。
覆盖率计算如下。

```text
coverage = saturate((sampledAlpha - threshold) * gain)
baseAlpha *= coverage
```

覆盖率在图层组合后乘入透明度。
视角响应、深度交界及节点透明度按既有次序消费。
该纹理的 RGB 用于通道解释，颜色结果保持独立。
CPU 评价在启用该字段时要求明确的覆盖采样。

纹理使用既有槽八及独立采样器设置。
作者 mip、层级偏移及颜色空间按图像元数据消费。
完整材质文档保存相同的纹理依赖和参数。
可选输入缺省时，覆盖率为一。

## 材质接入

根画像使用 `azureRenderMaterial.schemaVersion=2`。
能力列表同时声明 `unlit-surface` 与 `unlit-texture-layers`。
材质类别采用 `generic` 或 `emissive`，透明模式采用 `BLEND`。
公开格式见[图层模式](../../schemas/unlit_texture_layers.schema.json)。

标准基色纹理可引用相同的第一层纹理及 UV0。
基色倍率为一，其他标准纹理通道保持空引用。
颜色、照明、折射及描边参数由各自明确的模型拥有。
加载器校验图层模型与其他模型的字段所有权。

每层独立读取 glTF 纹理和采样器。
各轴支持重复、钳制和镜像寻址。
缩小与放大采用相同的单层过滤方式。
层级过滤可选最近邻或线性，单层纹理可关闭层级采样。

第一层使用固定槽零，第二层使用固定槽四。
图像导入设置可明确颜色空间。
图像元数据的 `settings.texture.mipBias` 提供层级偏移。
该值为负十六至十六的有限数，默认零。
每层 glTF 采样器确定过滤、寻址及层级选择。
冲突的图像采样设置产生异常。

完整材质文档通过 `azure.profile` 提供同一表面对象。
文档图像表引用资源 UUID，纹理表引用图像索引。
文档和模型共用解释器、图像解码及采样器校验。
解析失败保留已有输入和绑定状态。

## 帧参数与绘制

关卡渲染设置版本为二十。
`unlitEffects` 的内部版本为一。
该对象明确提供 `exposureGain`、`saturation` 和 `colorGain`。
字段解析和渲染设置验证共同检查有限值及输入域。

表现时间直接消费 `SceneFrameData::timeSeconds`。
帧常量末尾保存时间、曝光、饱和度及颜色增益。
摄像机常量大小为 688 字节，帧布局版本为一。
材质常量在加载期拥有，表现时间由宿主每帧提供。

材质表版本为十七，每项二十一行，共 336 字节。
图层使用前十行，记录两层坐标及颜色参数。
第十一行承载可选深度范围。
第十二行承载活动标记及顶点颜色标记。
第十三行承载可选视角覆盖率参数。
第十四至十七行承载可选覆盖纹理及重映射。
第十八至二十一行承载基础坐标扰动及映射。

## 场景雾

`fogMode: scene` 选择场景的预积分视图雾场。
相机匹配和资源规则见[雾效传输](fog-transport.md)。
图层颜色完成采样、扩张和颜色映射后评价雾效。
覆盖率和节点透明度按既有顺序消费。

透明材质采用颜色乘透射率，再加散射。
加色材质采用颜色乘透射率。
相机失配及关闭的场景绑定提供恒等传输。
材质表记录雾模式和散射选择。

雾图层使用独立的无光照片元入口。
入口共享图层采样与相机常量。
照明能量分离视图输出零，阴影可见性输出一。
基础颜色、发光、类别及法线视图保留对应语义。

编译后的资源操作检查覆盖四种雾图层程序。
固定与索引描述符分别覆盖单采样和四倍采样。
GI 和反射资源由消费对应语义的程序评价。

## 基础坐标扰动

`baseUvDistortion` 定义基础图层的采样位移。
字段为 `texture`、`uv`、`channels`、`scale` 和 `bias`。
纹理及独立 UV 使用既有图层的范围规则。
二维缩放和偏移均要求有限单精度数值。

`channels` 采用 `red` 或 `rg`。
`red` 将红通道同时用于两个坐标分量。
`rg` 分别采用红、绿通道。
输入值使用纹理导入声明的颜色空间。

```text
noiseUv = evaluateLayerUv(uv0, distortionUv, materialSeconds)
signal = channels == red ? sample(noiseUv).rr : sample(noiseUv).rg
offset = signal * scale + bias
baseSampleUv = evaluateLayerUv(uv0, baseUv, materialSeconds) + offset
```

扰动在基础图层的坐标变换后应用。
遮罩与覆盖率独立评价自己的采样坐标。
该纹理独立持有采样器、作者 mip 及导入偏置。
槽位三通过无光照材质的排他所有权承载该图像。
资产读取与流绑定见[顶点颜色输入](vertex-color-inputs.md)。
深度需求按材质能力进入现有不透明快照流程。
绑定十八提供同相机、同单位的线性视图深度。

透明混合使用 `SrcAlpha/OneMinusSrcAlpha`。
加色合成使用 `SrcAlpha/One` 并保留目标透明度。
着色器输出独立 RGB 与透明度。
节点透明度在现有最终透明合成中消费一次。
固定与索引描述符使用相同的材质表和评价函数。
