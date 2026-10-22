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

上述双叶高光使用引擎的幂次与偏移模型。
二维投影高光通过独立特征选择。
具体原生候选的绑定状态见[场景角色验收](../acceptance/nr/character-scene-lighting.md)。

## 二维投影高光

`hair-projected-ramp` 选择版本 1 的投影模型。
材质须使用版本 2、`hair` 类别及 `hair-anisotropy`。
`hair.projectedRamp` 提供参数与纹理。
`hair.dataTexture` 提供两组独立法线。

`parameterTexture` 保留原始 RGBA 参数图。
R 控制各向异性轴，G 控制主叶，A 控制次叶。
`rampTexture` 保留至少 2×2 的二维线性色带。
色带按双坐标、LOD 0、线性钳制采样。

```json
{
  "schemaVersion": 1,
  "parameterTexture": 5,
  "rampTexture": 6,
  "primaryShift": -0.02,
  "secondaryShift": 0.02,
  "intensity": 1.3,
  "secondaryRange": 0.5,
  "edgeFade": 5,
  "specularNormalScale": 1,
  "surfaceNormalScale": 1,
  "directionX": 0,
  "secondaryColor": [0.672, 0, 0.004363678],
  "objectBasis": [1, 0, 0, 0, 1, 0, 0, 0, 1],
  "surfaceResponse": "strand-diffuse",
  "diffuseScale": 0.96
}
```

`surfaceResponse` 与 `diffuseScale` 为可选字段。
其余字段必须出现，未知字段和版本在加载时拒绝。
偏移使用有符号值，源值转换式为 `2 * value - 1`。
`objectBasis` 为列主序的正交单位基。
对象基随实例和图元变换进入世界空间。

`surfaceResponse` 默认为 `layered`。
该响应将专用发丝高光叠加到通用表面。
`strand-diffuse` 消费漫反射与专用发丝高光。
其通用直射镜面与环境镜面贡献为零。

`diffuseScale` 为 0 至 1 的有限数，默认为 1。
该值独立控制漫反射能量。
`strand-diffuse` 的场景灯漫反射倍率为 1。
该模式通过显式缩放表达材质的能量系数。

固定源着色器画像使用 `strand-diffuse` 与 0.96。
转换器保留原始参数图，移除通用 ORM 绑定。
该画像的参数 B 通道仍待原生阴影与色带支持。
兼容粗糙度占位值为 1，金属度为 0。

| 字段 | 范围 |
| --- | --- |
| `primaryShift`、`secondaryShift` | -1 至 1 |
| `intensity` | 0 至 16 |
| `secondaryRange` | 0 至 1 |
| `edgeFade` | 0 至 64 |
| 两项法线缩放 | 0 至 8 |
| `directionX` | -16 至 16 |
| `secondaryColor` 各分量 | 0 至 64 |

两个法线均先重建 Z，再缩放 XY。
表面法线消费背面翻转，高光法线保留原方向。
投影视线与法线决定色带第二坐标。
次叶指数为 `trunc(max(1 - secondaryRange, 0) * 200)`。

方向灯消费主叶和次叶，局部灯消费主叶。
半向量与光能来自宿主的实际场景灯光。
源主灯虚拟方向和全局增益具有独立验收范围。
独立条纹分支及原生采样器仍须运行证据核验。

该模式独占材质纹理槽 5、6、7。
分别存放二维色带、双法线与原始参数图。
`hair-ramp`、脸部 SDF 与场景覆盖特征存在槽位冲突。
加载器拒绝相应特征及输入对象的组合。

材质表由渲染器加载并持有，卸载时统一释放。
固定与索引路径使用同一表和稳定材质索引。
片元阶段需要六个存储缓冲区。
每个描述符集合需要十一个存储缓冲区。

通用双叶高光、合成表面和原生候选分别验证。
零投影及退化切线使用宿主的确定性保护。
原生画面等价性须由真实场景对照建立。

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
显式外轮廓使用每实例独立的实面覆盖遮罩。
壳片元在自身覆盖内部被丢弃，外缘保留描边。
后景实例的覆盖保持前景实例的外缘。

遮罩包含同实例可见的非透明表面。
透明覆盖层与展示底座按各自绘制规则筛除。
单面剔除、镜像绕序与双面状态跟随正式表面。
节点渐隐继续使用完整覆盖及独立透明度。

每次外壳绘制均设置有效瓦片或无瓦片哨兵。
传统全局描边的全不透明实例保持无瓦片合批。
遮罩图集受设备尺寸与每帧 128 MiB 预算约束。
布局按实际渲染分辨率计算容量。

内部边缘继续消费场景的 `innerOutline` 设置。
通用场景表面使用其原有轮廓规则。
场景角色特征可独立于全局 `stylized` 开关消费。

`AzureRender.CharacterRepair` 验证画像加载和拒绝条件。
`AzureTools.NativeCharacter` 验证原生材质转换。
`AzureRender.OutlineCoverageRecording` 验证瓦片归属与剔除。
`AzureEngine.SilhouetteCoverageGpu` 验证内部覆盖与实例重叠。
场景灯光图像验证使用 `test_scene_character_gpu.py`。
测试证据按程序与着色器指纹关联到验收批次。
