# 角色材质画像

角色材质通过 glTF 的 `extras.azureRenderMaterial` 配置。
画像版本 2 使用显式类别和特征选择公共着色能力。
场景光照下，皮肤、脸、发、衣料、金属和眼分别计算。
光源颜色、能量与阴影来自场景灯光。

## 漫反射色带

`diffuse-ramp` 特征需要 `diffuseRamp` 对象。
对象字段为 `schemaVersion: 1` 与整数 `texture`。
`texture` 引用 glTF 纹理表，纹理必须指向有效图像。
图像宽度至少为 2，高度必须为 1。

色带按线性数据采样，边界采用钳制。
加载器关闭 mip，并将各向异性设置为 1。
色带占用风格遮罩槽。
`face` 类别及带 `faceSdf` 的画像使用独立脸部规则。

```json
{
  "schemaVersion": 2,
  "class": "fabric",
  "features": ["diffuse-ramp", "silhouette-outline"],
  "diffuseRamp": {"schemaVersion": 1, "texture": 3}
}
```

版本、未知色带字段、非法纹理及槽位冲突均被拒绝。
带特征的材质必须提供完整色带对象。
纹理采样与场景逐灯照明共用运行解释。

## 头发数据

`hair` 类别配合 `hair-anisotropy` 特征消费头发数据。
`hair.dataTexture` 为可选的 glTF 整数纹理索引。
四通道数据保存两组切线空间法线。
RG 用于表面，BA 用于独立高光法线。

`hair.dataNormalY` 可取 `positive` 或 `negative`。
默认值为 `negative`，表示加载器使用现有解码方向。
`positive` 输入由加载器反相 G、A 后进入统一解码。
声明方向时必须同时提供 `dataTexture`。

```json
{
  "schemaVersion": 2,
  "class": "hair",
  "features": ["hair-anisotropy", "silhouette-outline"],
  "hair": {
    "power": 200,
    "strength": 1.3,
    "rampRow": 3,
    "strandShift": 0.1,
    "dataTexture": 4,
    "dataNormalY": "positive"
  }
}
```

| 字段 | 约束 |
| --- | --- |
| `power` | 有限数，1 至 2048 |
| `strength` | 有限数，0 至 4 |
| `rampRow` | 整数，0 至 9 |
| `strandShift` | 有限数，绝对值至多 1 |
| `cameraOffset` | 可选，三个有限数，绝对值至多 16 |
| `upperLimit` | 可选，有限数，大于 0 且至多 16 |

当前双叶高光使用引擎的幂次与偏移模型。
原生二维高光色带及专有眼睛通道具有独立实现范围。
具体原生候选的绑定状态见[场景角色验收](../acceptance/nr/character-scene-lighting.md)。

## 乘法合成

画像版本 2 支持 `multiply-composite` 特征。
`composite` 对象使用版本 1 及 `multiply` 模式。
材质类别允许 `generic` 与 `overlay`。
glTF 的 `alphaMode` 必须为 `BLEND`。

```json
{
  "schemaVersion": 2,
  "class": "overlay",
  "features": ["overlay", "multiply-composite"],
  "composite": {"schemaVersion": 1, "mode": "multiply"}
}
```

线性颜色按 `Cdst * mix(1, tint, alpha)` 合成。
`tint` 来自基础颜色纹理及其材质因子。
`alpha` 同时消费纹理、材质因子与节点透明度。
染色因子使用无光照颜色，场景光照作用于接收表面。

该模式仅写主颜色附件，并保留目标透明度。
法线附件和深度保持接收表面的值。
覆盖层参与深度测试，阴影绘制跳过该模式。
深度比较为 `LESS_OR_EQUAL`，支持同面滤片。
透明实例及图元保持内容规定的提交顺序。

特征表允许 `overlay` 与 `neutral-fallback` 配合使用。
其他特征以及非法类别、版本和模式在加载时拒绝。
配置对象与显式特征必须同时出现。
原生模板、预深度和屏幕遮罩具有独立的验收范围。

## 轮廓与验证

`silhouette-outline` 按材质开启背面壳外轮廓。
内部边缘继续消费场景的 `innerOutline` 设置。
通用场景表面使用其原有轮廓规则。
场景角色特征可独立于全局 `stylized` 开关消费。

`AzureRender.CharacterRepair` 验证画像加载和拒绝条件。
`AzureTools.NativeCharacter` 验证原生材质转换。
场景灯光图像验证使用 `test_scene_character_gpu.py`。
测试证据按程序与着色器指纹关联到验收批次。
