# 风格化角色渲染

Character Renderer 的目标是把角色资产中的分层美术信息稳定地还原到实时 Vulkan 管线，而不是把所有表面套入一个统一 PBR 模型。脸、皮肤、头发、布料、金属、眼睛、透明覆盖层和展示地台使用不同的约束；统一之处是它们最终写入同一 HDR Scene Color、Depth 和 Normal，并接受公共 Capture 与诊断系统检查。

## 帧管线

```mermaid
flowchart LR
    Asset[glTF + Material Profile] --> CPU[Animation / skin / sorting]
    CPU --> Shadow[2048 Shadow Pass]
    Shadow --> Main[Character Main Pass]
    Env[HDR / Cubemap] --> Main
    Ramp[Toon Ramp Atlas] --> Main
    Main --> Shell[Silhouette Outline]
    Shell --> HDR[HDR Color + Depth + Normal]
    HDR --> Inner[Depth/Normal Inner Outline]
    Inner --> Post[Bloom + Grade + Tone Mapping]
    Post --> Output[Swapchain / PNG]
```

高层执行顺序：

1. 更新节点动画、Joint Matrix、Morph Weight 和模型转台。
2. 计算透明 Primitive 的视图相关排序索引。
3. 从固定世界主光记录 Shadow Pass。
4. 记录背景、展示地台和角色主材质。
5. 对需要的材质记录背面壳 Silhouette Outline。
6. 公共后处理从 HDR Color、Depth 和 Normal 提取内部边缘并完成显示映射。

角色绕双脚中心旋转，而不是绕模型原点。脚底中心由低高度顶点的稳健统计估计，避免武器尖端或附件改变转轴。世界主光保持不随模型旋转，因此完整转台中能观察到连续的受光、背光和投影变化。

## glTF 数据进入 GPU

`GltfLoader` 读取：

- Position、Normal、Tangent、UV、Color、Joint 和 Weight。
- Index、Primitive、Node 层级、Skin 和 Inverse Bind Matrix。
- Animation 的 Translation、Rotation、Scale 通道。
- glTF 标准 PBR 字段、Alpha Mode、Double Sided。
- `extras.azureRenderMaterial` 中的 Material Profile。

Loader 输出 CPU 侧 `LoadedAsset`；`CharacterSceneRenderer` 创建 Device Local Vertex/Index Buffer、纹理 Image/Sampler、每材质 Descriptor 和每帧 Uniform/Joint Buffer。缺失纹理使用类型正确的回退资源，确保 Descriptor 始终完整。

蒙皮在顶点阶段执行。CPU 根据当前动画计算节点世界矩阵与最终 Joint Matrix，写入当前 in-flight frame 的 Joint Buffer。静态模型走单位 Skin 兼容路径，不要求另外一套 Pipeline。

## 材质分类

Material Profile v1 使用明确类别而不是材质名称猜测：

| 类别 | 主要约束 |
| --- | --- |
| `generic` | 常规 fallback，保持保守材质响应 |
| `skin` | 介电质、暖色阴影、限制金属度和镜面能量 |
| `face` | Skin 约束加 Face SDF |
| `hair` | HN/P、Hair AO、双层 Kajiya-Kay、色相保护 |
| `fabric` | 强 Toon 分区、AO 与较低镜面 |
| `metal` | 保留完整金属响应和环境镜面 |
| `eye` | 高可读镜面与独立 Ramp |
| `overlay` | 透明、非标准深度/混合行为 |
| `emissive` | 自发光主导 |
| `showcase` | 展示地台等场景辅助材质 |

Profile 的 `features` 再启用 `stylized-shadow`、`hair-anisotropy`、`face-sdf-eligible`、`emissive-mask`、`overlay`、`neutral-fallback` 或 `brow-overlay`。类别决定基础规则，Feature 只打开具体能力；两者不能互相替代。

Skin、Face、Hair、Fabric 和 Eye 默认按介电质处理。Packed 纹理中的异常 Metallic 会被类别上限约束，避免皮肤或头发出现白色金属反射。Metal 类才保留完整 Metallic/F0 路径。

## Toon Ramp 与明暗分区

传统 Lambert 项为：

$$
N\!L = \max(\mathbf{n}\cdot\mathbf{l}, 0)
$$

AzureRender 不直接把它作为最终亮度，而是生成 Ramp 坐标：

$$
u_r = \operatorname{clamp}(N\!L + \Delta_{threshold} + \Delta_{style}, 0, 1)
$$

材质类别选择 `toon_ramp_atlas.ppm` 的一行，`u_r` 沿横向采样。Skin/Face 使用连续暖色过渡；Hair/Fabric/Metal 使用更明确的分区。Ramp 数据源是版本化 JSON，生成 Atlas 后由运行时加载；参数校验失败不会静默退回编译期常量。

最终漫反射不是单一乘法：

$$
C_{diffuse}=C_{ambient}\,V_{ambient}+C_{direct}\,R(u_r)\,V_{shadow}
$$

其中 `V_shadow` 来自实时 Shadow Map，`R` 是分类 Ramp，环境可见度也随暗部分区调整。这样环境光能够解释体积，但不能把背光面托成和受光面一样亮。

## 主光与环境光

Character 使用固定世界空间方向光建立可观察的明暗关系，环境贴图提供背景和方向性环境漫反射/镜面。环境图通过世界法线或反射方向采样，不是统一 Ambient 常量。

角色展示 Look 只负责 Grade、Bloom 和 Outline 参数，不取代灯光、材质分类或 Face SDF。当前 Catalog 包含 Azure Gallery、Endfield Industrial、Neutral Material Check、Specular Rim 和 Rear Emissive。

HDR 合成顺序避免用曝光掩盖材质问题：先完成线性光照、AO、Ramp、镜面、Rim 和自发光，再做 Bloom、Exposure、Tone Mapping、Tint、Saturation 与 Contrast。检查皮肤或头发时应同时查看 Beauty 和 Albedo，区分底色错误与光照错误。

## PCSS 软阴影

Shadow Pass 从固定主光记录角色与地台深度，分辨率为 2048。Main Pass 把世界位置变换到 Light Clip Space，得到 Shadow UV 和接收面深度。

PCSS 分两步：

1. 在接收点周围用 Poisson Disk 搜索 Blocker，计算平均遮挡深度 $z_b$。
2. 根据接收面与遮挡面的距离估计 Penumbra，再用扩大后的 Poisson Kernel 计算可见度。

概念公式：

$$
r_p \propto \frac{z_r-z_b}{z_b}\,R_{light}
$$

其中 $z_r$ 是 Receiver 深度，$R_{light}$ 对应设置中的最大过滤尺度。实现将半影限制在 `maximumFilterRadiusTexels` 内，默认 8 texel，可在编辑器的 Shadow Softness 中调节。

接触阴影因为 $z_r\approx z_b$ 而保持收紧，远离遮挡物时自然变软。深度 Bias 同时考虑基础偏移和表面朝向，减少 Acne，但不能用过大的 Bias 隐藏 Peter Panning。

`shadow-visibility` 隔离图用于检查滤波本身；Beauty 用于检查阴影与 Ramp/AO 的合成。两者缺一不可。

## Face SDF

二次元脸部的阴影轮廓通常不是几何法线的等值线。Face SDF 把美术设计的明暗边界编码在纹理中，主光只控制采样方向和阈值。

Material Profile 指定：

- SDF Texture 与 UV Set。
- 距离通道和参与遮罩通道。
- 低值还是高值表示阴影。
- 纹理水平方向的脸部语义。
- 头部节点名称。

仅有 `face-sdf-eligible` Feature 不足以启用效果；资产还必须提供完整 `faceSdf` Profile 和有效头部节点。否则 Renderer 绑定回退纹理并保持兼容，但不能宣称存在真实 SDF 明暗。

### 头部局部光照

导入骨骼矩阵的列向量不一定等于模型语义上的左、上、前。实现保存 Bind Pose 的头部基底，再使用当前头部相对 Bind Pose 的旋转，把世界主光转换到稳定的脸部语义坐标：

$$
R_{relative}=M_{head,current}\,M_{head,bind}^{-1}
$$

$$
\mathbf{l}_{face}=R_{relative}^{-1}\mathbf{l}_{world}
$$

横向分量决定左右采样权重，正面分量参与阈值调整。这样角色转动、骨骼动画和模型坐标约定不会把脸长期锁在全亮状态。

### 左右镜像与连续性

左右脸需要在原始 UV 与镜像 UV 之间切换。直接使用 `lateralLight >= 0` 的硬分支会在光线跨过零点时整脸突变。当前用连续权重混合两个方向：

$$
w_{side}=\operatorname{smoothstep}(-\epsilon,\epsilon,l_x)
$$

$$
d=\operatorname{mix}(d_{mirror},d_{original},w_{side})
$$

随后用阈值和 Softness 得到 Face Illumination：

$$
I_{face}=\operatorname{smoothstep}(t-s,t+s,d)
$$

结果按遮罩和参与权重混入 Ramp 坐标，并使用暖色 `faceSdfShadowColor` 染色。SDF 负责脸部设计阴影，实时 Shadow Map 仍可表达头发、附件或环境对脸的遮挡。

### 验收

Face SDF 的正确性不能只看全局开关：

- Loader 日志应报告真实纹理尺寸和头部节点。
- `face-sdf` 隔离图应出现连续左右灰阶分界。
- 正面、三分之四和完整转台中边界应随光线连续移动。
- Beauty 中脸与同侧肩颈肤色应连续，不能整脸漂白或暗侧发脏。
- 相邻帧不能在侧向光过零时突变。

## 头发数据与 Kajiya-Kay

头发使用三类数据：

| 纹理 | 用途 |
| --- | --- |
| Hair D | Base Color 与透明信息 |
| Hair HN | RG 解码基础发束法线；BA 解码高光方向扰动 |
| Hair P | Metallic/Roughness/Specular 等打包材质数据 |

HN 必须通过独立 `hairDataTexture` Binding 上传。Hair P 的命名与布料 Packed Map 不同，资产注入器必须同时识别；缺失时使用回退值并在 QA 中暴露，不能让公式存在但数据从未绑定。

Kajiya-Kay 使用发束方向 $\mathbf{t}$ 与 Half Vector $\mathbf{h}$：

$$
S=\sqrt{1-(\mathbf{t}\cdot\mathbf{h})^2}
$$

通过 HN 的 BA、材质 Shift 和法线方向构造两条略微错开的发束方向，分别计算窄主高光和较宽次高光。Lobe 经平滑阈值变成稳定条带，并乘主光、Ramp、视角可见度、材质强度和 Style Mask。

两层高光的目的不是增加整体亮度，而是让正面和转台中出现可读、连续、方向正确的发丝条带。`hair-kk` 隔离图应能单独看到条带；若全黑，应先检查材质类别、Feature、HN Binding 和参数，而不是提高曝光。

## Hair AO 与内部轮廓

Hair AO 是独立的风格化体积层，由以下信号组合：

- 材质 AO Color。
- Style Mask。
- 视线掠射关系。
- HN 发束法线与几何法线偏差。
- 当前 Ramp/阴影区域。

它在漫反射层形成发片内部遮蔽，而不是只把已处于阴影的像素再次压黑。过强会产生脏块，过弱则使发片粘成一整块。

内部轮廓 Pass 同时查看 Depth Edge 和 Normal Edge。Hair 提高 Shaded Normal 的参与，让发片之间产生细线；Silhouette Outline 仍只处理外轮廓。二者必须分开调节，不能通过整体加粗外壳补偿内部层次。

为防止环境镜面把红色头发漂成灰白，Hair 漫反射在完成环境和 Toon 合成后按 Base Color 色相进行保守回投，并限制环境镜面能量。该保护不替代 AO、HN 或 KK。

## 眉毛 Overlay

眉毛不是画在 Face Mesh 上的普通不透明纹理，而是独立透明 Primitive。其实现包含：

1. 使用 Face D 对应 UV 区域提供眉毛颜色和形状。
2. 顶点沿 Pixel-to-Camera 方向推出，避免与脸部 Z-Fighting。
3. 使用 Unlit 风格输出和材质常量透明度。
4. 透明 Primitive 按视图更新排序索引。
5. 对真正眉毛顶点做局部、拓扑感知的厚度补偿，保持远景可读。

原始实例中的 `4.679` 使用厘米单位，glTF 米制路径应转换为 `0.04679 m`。直接使用 `4.679 m` 会把眉毛推离角色。

眉毛 Primitive 可能同时包含眉毛、睫毛和眼部小岛，并由不同骨骼控制。不能围绕单个中心整体缩放，否则会拉开小岛和破坏骨骼关系。网格审计应确认：顶点连续、无退化三角形、权重和为 1、左右骨骼分配对称、没有无关骨骼影响。

透明排序必须保留 glTF 全局 Vertex Index。把 Index Buffer 偏移错误地再次应用到顶点索引，会让眉毛读取错误顶点，即使 Descriptor 和 Shader 都已启用也不可见。

## 描边系统

角色包含两类描边：

### Silhouette Outline

对选定 Primitive 绘制背面壳，顶点沿法线外扩，使用 Front Face Culling 和深色材质。宽度在模型/视图尺度中受控，禁止整体缩放 Clip Space `xy`，后者会造成头部过宽、远近不一致和画面拉伸。

透明 Overlay、展示地台和不应产生外壳的材质跳过该 Pass。

### Inner Outline

最终 Composite 在屏幕空间采样 Depth 和 Normal 邻域：

$$
E=\max(E_{depth},E_{normal})\,S_{outline}
$$

Depth Threshold 控制几何前后边界，Normal Threshold 控制同一深度附近的朝向变化。Hair 使用更高的 Shaded Normal 参与以保留内部发束。该 Pass 还负责诊断显示、Bloom、曝光和 Tone Mapping，因此任何 Binding 或颜色空间变更都必须成组测试。

## 透明、双面与排序

Opaque/Mask Primitive 先绘制并写深度；Blend Primitive 使用每帧视图相关索引排序后绘制。眉毛等 Overlay 需要避免错误深度写入和自遮挡，但不能简单关闭所有深度关系。Double Sided 来自 glTF 或材质契约，不能仅根据名称猜测。

当前透明方案适合角色数量和材质规模，不是通用 Order-Independent Transparency。多层大面积透明仍可能出现排序限制，应通过固定相机和旋转 QA 检查。

## HDR、Bloom 与显示映射

主场景在线性 HDR 中合成。最终 Pass 的概念顺序为：

```text
HDR scene
  -> thresholded bloom
  -> inner outline
  -> exposure (2^EV)
  -> tone mapping
  -> tint
  -> saturation
  -> contrast
  -> swapchain color
```

Bloom 只从超过 Threshold 的能量提取。受光面应明显，但不能依靠全局 EV 把 Face 或 Hair 推成纯白；Neutral Material Check 会降低 Bloom 和轮廓干扰，用于检查原始材质响应。

## 诊断视图

| 视图 | 主要排查内容 |
| --- | --- |
| Beauty | 最终视觉组合 |
| Albedo | 原始底色、纹理和分类错误 |
| World Normal | 法线、蒙皮和内部轮廓输入 |
| Depth | 相机比例、深度连续性和边缘输入 |
| Diffuse Band | Toon Ramp 坐标和分区 |
| Shadow Visibility | PCSS 与 Bias，不含其他阴影染色 |
| Hair KK | HN、发束方向和高光能量 |
| Rim/Specular/Emissive | 独立光照分量 |
| Outline | 外壳与屏幕空间边缘 |
| Shadow Map | 主光深度覆盖 |
| Material ID | Primitive 分类和 Profile |
| Style Mask | 风格化参与区域 |
| Ambient/Direct Diffuse | 环境与主光能量分离 |
| Shadow Tint | AO、Ramp 暗部和染色 |
| Face SDF | SDF 遮罩、方向和连续性 |
| Overlay | 眉毛等透明覆盖层 |
| Bloom | HDR 高亮提取 |

## 固定验收方法

一次角色视觉变更至少检查：

1. 公共资产 Debug Validation 120 帧。
2. `full-body-front` Beauty：构图、转轴、全身材质与地台投影。
3. `face-front` 和 `face-three-quarter`：肤色、Face SDF、眉毛和头发前方高光。
4. 完整 16 秒转台：插值连续、透明排序、阴影和背面细节。
5. Albedo、World Normal、Shadow Visibility、Hair KK、Face SDF、Outline 隔离图。
6. Stylized On/Off 对照，证明效果来自目标分支而不是曝光或资产变化。

自动化可以验证参数、Schema、捕获状态和像素差，但“发丝是否有层次”“脸部阴影是否自然”仍需要人工视觉判断。人工判断也不能替代 Validation 和资源审计。

## 已知限制

- 当前角色光照是风格化直接光加实用型环境采样，不是完整预过滤 PBR IBL。
- Face SDF 依赖资产提供正确纹理方向和头部节点；通用自动推断不可靠。
- 透明层使用排序路径，不是通用 OIT。
- Silhouette 宽度仍受模型单位和导入尺度影响，需要资产 Profile 保持单位一致。
- 私有角色及其派生媒体不能作为公开 CI 或发行资产；公共 Test Model 负责可再现门禁。
