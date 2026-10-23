# 原生帧还原计划与会话交接

日期：2026-10-22。
执行状态：已按用户指令恢复自主实施。
C16-R3 的双配置构建、CPU 及 GPU 回归通过。
恢复入口及剩余检查见[片元与合成核对](../acceptance/nr/character-composite-diagnosis.md)。
当前执行逐技术点还原及真实模型验证。
范围见[角色技术清单](2026-10-22-character-technique-closure.md)。
本轮接入审计见[架构与能力审计](2026-10-22-render-capability-integration-audit.md)。
新增任务为 NR0-A1、NR6-C15 至 C18、NR8-V2。
首批角色候选已进入主项目与可玩入口。
范围见[主场景接入](../acceptance/nr/main-character-integration.md)。
公共材质已接入稀疏参数覆盖及共享编辑操作。
八次 GPU 夹具与真实角色对照见[覆盖验收](../acceptance/nr/material-parameter-overrides.md)。
C15 完整画像、UUID 依赖及编辑事务接入通过。
证据见[完整文档验收](../acceptance/nr/complete-material-document.md)。

当前执行 C16 的输入边界及 C17 的多层高光。
虚拟主灯与独立条纹已接入公共材质契约。
三个真实正主叶及五份关闭对照见[分层验证](../acceptance/nr/projected-hair-positive-layers.md)。
C11-R7 接入逐材质光色，范围见[皮肤验证](../acceptance/nr/npr-material-lighting.md)。
独立角色可见量继续按 C16-R4 核对。
C11-R8 的材质边缘响应及正交回退见[实际验证](../acceptance/nr/material-rim-and-view-tint.md)。
身体输入、局部漫反射及独立镜面继续按 C16-R1 和 C9 核对。
C11-R9 的有符号主光色带通过双配置验证。
身体拓扑、真实片元及剩余阴影见[色带验收](../acceptance/nr/signed-main-ramp.md)。
C16-R4 继续核对屏幕遮罩的独立角色、场景可见量。
材质来源选择与常量接收遮罩通过双配置验证。
原生对象阴影图、采样及剩余生产门禁见[来源验收](../acceptance/nr/character-visibility-inputs.md)。
实例布局已统一，范围见[实例契约验证](../acceptance/nr/instance-gpu-contract.md)。
对象记录使用独立版本、所有权与容量检查。
完整几何与平方根 Alpha 已接入公共材质特征。
原生运算及双配置范围见[投影覆盖验证](../acceptance/nr/material-shadow-coverage.md)。
注册组件、公共投影及文档链路通过双配置检查。
范围与宿主边界见[配置验证](../acceptance/nr/object-shadow-configuration.md)。
独立深度绑定及共享采样器通过双配置检查。
实际画面与范围见[共享采样验证](../acceptance/nr/shared-depth-sampling.md)。
对象图集、在途记录及分布过滤已接入。
原生深度对照与完整外观边界见[GPU 验证](../acceptance/nr/object-shadow-gpu.md)。
腿部仍有七字节误差，继续分离可见量与局部灯贡献。
接收位置与表面法线已接入公共组件。
对象隔离测得腿部为 `5/255`，与原生一致。
范围见[接收坐标验证](../acceptance/nr/object-shadow-receiver.md)。
继续核对切线基、法线采样和剩余皮肤光照。
随后推进完整头发环带、服饰和距离描边。
发冠强边缘光偏差已定位到缺失的外扩深度壳。
事件 440 的覆盖影响 C17，按 C18 公共描边契约接入。
普通头发世界顶点一致，深度采样及实际覆盖分别验收。
共享 XY 投影核及原生顶点见[描边来源验证](../acceptance/nr/outline-depth-source.md)。
可选顶点流、顶点采样及深度壳已进入生产候选。
半球与八面体方向共用独立记录和当前姿态。
真实发冠的八个冻结控制点误差为零至一字节。
原生整区和动态晋升仍需验证。

机制与待项见[接入验证](../acceptance/nr/outline-material-integration.md)。
具体公共契约及绑定任务见[描边接入计划](2026-10-23-outline-projection-integration.md)。
眼睛与瞳孔按 C9-E1 核对原生视差、光照和遮挡。
公共 NPR 适配及捕获姿态文档见[眼睛验证](../acceptance/nr/eye-npr-inputs.md)。
五点误差至多一字节，完整内缩区域最大十二字节。
继续核对层级边界、眼白附加绘制及动态视线。
C11-R10 的逐材质局部灯下限已通过双配置验证。
范围见[响应验证](../acceptance/nr/material-local-wrap.md)。
实际腿部颜色为 `[82,53,45]`，原生为 `[85,59,52]`。

C11 已接入显式照明组合，继续闭合 HDR 与调色链。
范围见[照明验收](../acceptance/nr/npr-illumination.md)。
执行后顶点与实际调色表见[采集证据](../acceptance/nr/captured-vertex-and-grading.md)。
切线手性、色带方向及诊断快照见[坐标验证](../acceptance/nr/projected-hair-coordinates.md)。
九槽采样、容器颜色及原生 mip 见[输入验证](../acceptance/nr/typed-image-inputs.md)。
实际过滤、层级偏移与编辑消费见[采样验证](../acceptance/nr/sampler-filter-and-bias.md)。
共享颜色核及头发的输入职责见[照明核对](../acceptance/nr/captured-hair-illumination.md)。
全局环境权重及主灯条纹见[运行验证](../acceptance/nr/ambient-weight-and-main-line.md)。
逐材质头发光色及双叶完整照明见[头发验证](../acceptance/nr/projected-hair-lighting.md)。
局部灯原始输入、颜色模式及顺序见[灯记录](../acceptance/nr/captured-character-local-lights.md)。
局部颜色响应见[实际验证](../acceptance/nr/character-local-light-response.md)。
投影方向、真实点光及主场景影响见[聚簇验证](../acceptance/nr/clustered-light-projection.md)。
线形发射、方向积分及实际灯二见[线形验证](../acceptance/nr/character-line-light.md)。
有序混合、实际权重及最终像素见[颜色混合](../acceptance/nr/character-color-blend.md)。
屏幕边缘及镜面消费者见[运行验证](../acceptance/nr/character-screen-light-modes.md)。
完整九灯候选的图像误差仍待修复，按 C16-R1 继续定位。
捕获子像素偏移已接入公共相机版本二。
多片元输入与双配置检查见[投影验证](../acceptance/nr/camera-projection-offset.md)。

环形多层高光与距离描边保持完整原生验收待项。
完整抗锯齿按[NR7-AA 任务](2026-10-22-spatial-antialiasing.md)推进。
运动与 HDR 历史的公共接入见[时序验证](../acceptance/nr/temporal-reconstruction-integration.md)。
当前继续执行材质细节、连续运动及正式画质门禁。
公共四倍尺寸与覆盖核对见[时序质量](../acceptance/nr/temporal-coverage-quality.md)。
距离描边采样及线性深度历史见[覆盖契约](../acceptance/nr/outline-temporal-footprint.md)。
先保存主入口走样红测及官方时序通道输入。
材质阶段同步核验发丝、KK、Ramp、服饰与描边走样。
视图历史、运动场及静态与动态联合画质保持待验。
最终目标为完成 NR0 至 NR12 的全部适用门禁。

## 目标与阅读顺序

目标是近乎原生地复刻白黄色帝江号中庭。
范围覆盖后方室内、中央物流、光照与反射。
莱万汀使用中性表情和原生编队待机 01。
可玩交付支持 WASD、碰撞及原生运动动画混合。

先读根目录 `AGENT.md` 和本页。
阶段与依赖见[可玩还原计划](2026-10-17-native-playable-restoration.md)。
原始门禁见[渲染规格](2026-10-17-native-render-restoration.md)。
当前任务见[角色与物流计划](2026-10-22-character-logistics-restoration.md)。

仓库为 `D:/Assigment/2609/FYP`。
引擎目录为 `Project/AzureRender`，分支为 `engine-dev`。
NR0 在阶段表中登记为 `Active`。
NR1 至 NR12 均为 `Pending`，完整门禁保持待验。

游戏目录保持只读，原始资产留在本机产物目录。
公共引擎按类型、版本和显式内容参数接入能力。
角色名、关卡名和源对象身份属于离线转换及项目数据。
公共能力同时验证合成夹具与实际内容。

## 原生帧证据

九帧均通过官方 RenderDoc 1.46 加载和通道查询。
实际图形接口均为 D3D11。
文件共 `5521101932` 字节，哈希重新核对通过。
加载成功与专项绑定解析分别验收。

捕获产物目录记为 `CAPTURE`：

```text
D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261022-201845-endfield-capture-954719
```

文件名前缀为 `Endfield-games-child_frame`。
九帧清单及收据见[原生帧验证](../acceptance/nr/native-game-captures.md)。
工具复用与官方桥接见[抓帧接手说明](2026-10-22-endfield-capture-handoff.md)。
目录的 `.keep` 标记用于保留原始证据。

| 锚帧 | 用途 | 已验证产物 |
| --- | --- | --- |
| 3534 | 中庭正面、地面反射与中央物流 | 实际颜色图、资源清单、通道查询 |
| 10401 | 莱万汀全身、脸、头发、皮肤和服饰 | 实际颜色图、资源清单、通道查询 |
| 4697、5565 | 中庭侧面、顶部及其他物流绘制 | 加载、缩略图、通道查询 |
| 6834 | 莱万汀编队预览对照 | 加载、缩略图、通道查询 |
| 8402、8877 | 洁尔佩塔与大潘，验证材质通用性 | 加载、缩略图、通道查询 |
| 2399、1058 | 中庭带界面及标题界面 | 加载、缩略图、通道查询 |

3534 与 10401 的实际重放图均为 2560×1440。
两帧已完成目视核对。
中庭 Present 为 12945，角色 Present 为 3460。
各自最终颜色均为本帧的 `ResourceId::406`。

帧内资源号必须与完整帧哈希共同使用。
同名 `.png` 是缩略图，`*-replayed.png` 是实际输出。
Present 输出槽查询有失败记录，显式资源导出成功。
通道分组及静态程序号须再对应到实际绘制。

中庭和角色预览各自保存照明常量。
侧面与仰视帧用于空间交叉检查。
物流时间分析需要同机位的多个时刻。
九帧的画质配置和相机参数仍需补齐。

## 实施顺序

以下为恢复执行后的优先任务。
依赖以具体证据及公共契约为准。
正式阶段完成顺序继续由阶段表控制。
每项交付保留来源、错误、输出和适用范围。

先按 NR0-A1 固定有效主入口和内容指纹。
脸部、服饰镜面和头发颜色候选按 NR8-V2 晋升。
新增纹理及参数先满足 C15、C16 的契约条件。
C17、C18 分别承接完整头发高光与距离描边。

| 顺序 | 代号 | 任务与输入 | 完成条件 |
| --- | --- | --- | --- |
| 1 | NR0-F2 | 为 3534、10401 建立实际事件和资源索引 | 帧哈希、事件、阶段、绑定槽及导出摘要闭合 |
| 2 | NR6-C14 | 导出角色逐材质真实绑定与全部相关通道 | 脸、发、眼、皮肤、服饰、发影及描边各有证据 |
| 3 | NR6-C12-R1、C11 | 核对脸部附加帧与完整照明，处理凹凸暗影 | 同源输入的数值与鼻下、脸颊近景共同通过 |
| 4 | NR6-C13、C9 | 接入服饰镜面、二维高光及其余材质缺项 | 源通道保留，实际绘制与公共机制回归通过 |
| 5 | NR1、NR2、NR3 | 核对后方几何、采样流、材质及灯组 | 可见绘制与导入实例对应，投影和遮挡闭合 |
| 6 | NR4-G6 | 恢复运行 GI、默认环境及角色环境参数 | 原生常量、三层过渡和移动采样共同验证 |
| 7 | NR5-R7 | 核对原生反射路径、地面 UV 与镜面响应 | 探针、动态反射、粗糙度和地面近景闭合 |
| 8 | NR7-L4、L5 | 核对物流实际集合、软交界及时间参数 | 实例、纹理层、深度、运动方向及周期闭合 |
| 9 | NR7、NR0、NR7-AA0 至 AA6 | 核对曝光、调色、雾、辉光与完整抗锯齿 | 源输入、中间目标、运动稳定性及主场景细节通过联合对照 |
| 10 | NR8 至 NR12 | 联合画面、原生动作、WASD 及发布验收 | 全部适用门禁、目录移动和动态实录通过 |

每个问题先定位源绘制及输入，再定义最小公共契约。
缺项测试应证明实际缺陷，之后实施并做受影响回归。
真实角色近景和完整中庭共同验证内容消费。
每个可独立验收任务按 `feat(nrN): 中文摘要` 提交。

### 事件与绑定清单

清单格式为 `NativeCaptureBindingManifest/1`。
每条记录保存帧哈希、API、实际事件和着色阶段。
同时保存程序二进制、反射布局及原始资源摘要。
两份完整事件索引已生成，并通过严格校验。
范围见[绑定索引](../acceptance/nr/native-binding-index.md)。

纹理记录格式、尺寸、层级、切片和线性色彩解释。
采样器记录过滤、寻址及实际槽位。
缓冲同时保存原始字节、偏移及类型解释。
网格记录顶点流、索引、实例和世界变换。

深度、模板、剔除、混合与渲染目标共同记录。
压缩纹理保留原始载荷，并单独导出查看图。
shader 保存原二进制、反汇编及可用的反射信息。
静态源程序与帧内程序按摘要和指令对应。

### 脸部与完整角色照明

优先从 10401 定位脸部实际绘制及前置通道。
导出 SDF、遮罩、RGBA 色带、影色表及采样器。
核对角色全局常量和完整漫反射照明量。
镜面、环境、主光、局部灯和自发光分别核对。

静态 face117 的径向环境法线混合使用遮罩 G。
其法线与 SDF 蓝通道的局部灯法线各有消费者。
动画缓冲首三行的帧身份须用实际矩阵证明。
头骨、角色根及渲染器矩阵须在同帧对照。

当前候选采用宿主显式头节点作为径向原点。
其原生等价性尚未确认。
颜色选择的水平相机方向也须对应原生基底。
鼻下和脸颊暗影按直接光、GI、AO、镜面分项归因。

SDF 绑定和开关像素变化已有证据。
角色整体仍存在可见照明与材质差距。
最终检查包含头部转动、近景与完整场景受光。
源帧可能仍有艺术化遮罩，各区域以真实输出为准。

### 场景照明与反射

在 3534 定位地面、后方墙体、玻璃和顶部绘制。
将各绘制与现有原生实例及材质来源对应。
地面 `_MaskUVSet=1` 的实际采样流须优先导出。
源 Mesh 的 UV1 为空，额外流及生成坐标仍待核验。

导出活动灯数据、作用体积、阴影及聚光形状资源。
核对 GI 纹理、采样器、三层常量和默认球谐系数。
重点字段为 `_IVDefaultSHAr/g/b`。
角色环境全局量与中庭环境分别对应实际绘制。

追踪反射相关生产通道、消费通道和历史输入。
按实际纹理绑定区分探针、屏幕反射及其他来源。
粗糙层级、DFG、强度及多重散射一起核对。
画面中可见反射只证明现象，算法身份由管线证明。

### 物流与动态效果

3534、4697、5565 用于定位物流模型和不同角度。
每个实例记录变换、绘制、材质和透明规则。
同时核对导轨、条纹、圆柱、发光层和环带。
源十发射器候选有 91 个槽位，实际激活仍待对应。

纹理滚动、软交界及表现时间分别导出输入。
同机位时序用于数量、方向、周期和出生消失核验。
帧号差及文件时间不能独立证明物体运动周期。
物流动态还需检查阴影、反射及暂停恢复。

## 已提交的局部实现

以下提交可按记录范围复用。
公共机制检查与原生外观验收分别记录。
完整 NR 门禁保持待验。

| 提交 | 范围 | 剩余边界 |
| --- | --- | --- |
| `8425d78` | 抓帧工具和历史失败接手入口 | 成功原生帧见本页与抓帧验证 |
| `0a267df` | 身体皮肤水平环境法线 | 脸部径向帧及完整环境照明 |
| `b60e63e` | 主光漫反射能量遮挡 | 原生完整照明及真实外观 |
| `faf415f` | 角色影色组合及逐灯接收范围 | 原生运行全局量及未消费槽位 |
| `fbe8797` | 三层原生方向场离线组合 | 运行三层采样、默认 SH 和活动体积 |
| `506f75b` | 反射射线计算及动态遮挡回归 | 正式性能和原生反射路径等价 |

六面探针、DFG、动态 SSR 和粗糙过滤已有分项证据。
角色、物流和完整中庭已有联合候选与视频。
AA 已覆盖主辅视口和 Player 的既定检查。
原生走跑混合已有局部输入回放与生命周期证据。

这些结果按源码、配置及内容摘要复用。
新 shader、材质或采样域会触发对应回归。
真实源帧用于核对当前候选及剩余宿主假设。
完整性能、视觉及发布仍按原计划验收。

## 工作区与产物

主实施目录记为 `RUN`：

```text
build/runs/20261022-051129-character-logistics-restoration-0f2e71
```

角色审计目录为 `RUN/scene-audit/character-binding-audit`。
本轮文档核对目录为：

```text
build/runs/20261022-210028-capture-plan-handoff-2e194d
```

该目录保存接手前的 `workspace-before.patch`。
`workspace-before.json` 保存分支、HEAD 和修改摘要。
`wip-source/` 保存未提交源码副本。
新会话先看实际 `git status`，保留全部现有改动。

### 静态调查入口

下表路径相对于角色审计目录。
静态算法、配置来源与帧内运行值分别使用。
新原生帧用于核对待定基底、激活和实际输入。

| 文件或目录 | 主要内容 |
| --- | --- |
| `native-diffuse-normal-consumers.json` | 脸、皮肤的环境法线及各消费分支 |
| `reserved-skin-frame-producer.json` | 额外三行矩阵的分配及来源追踪 |
| `character-volume-profile-trace.json` | 中庭角色环境配置和覆盖项 |
| `volume-stream-contract.json` | 环境配置参数的读取与流契约 |
| `cloth-texture-trace/` | 参数图通道、高光色带及 C13 设计 |
| `face-frame-radial-review.json` | 径向候选静态审查与适用边界 |

场景调查位于 `RUN/scene-audit/advanced-lighting/`。
`gi-activation-audit/` 保存三层组合和默认 SH 调查。
`ssr-uv-binding/` 保存地面 UV、实例及附加流待项。
源 face117 程序位于下列文件：

```text
build/runs/20261021-172156-native-render-evidence-623c52/native-face/actor-face-vulkan117.hlsl
```

### 脸部径向法线候选

公共机制已提交为 `2677dc0`。
当前证据见[脸部法线验收](../acceptance/nr/face-radial-indirect.md)。
模式名为 `face-frame-radial-horizontal`。
它使用面部类别、版本二 SDF 和显式头节点位置。
SDF 关闭时，环境采样回退表面法线。

| 内容 | 现有结果 |
| --- | --- |
| Debug GPU | `face-radial-indirect-debug-v3/summary.json`，36 次通过 |
| Release GPU | `face-radial-indirect-release/summary.json`，36 次通过 |
| 数值范围 | 最大线性误差 `0.0029339036`，预算 `0.008` |
| 校验层 | Debug 开启，Release 关闭 |
| 数学与加载 | 163 数学探针及所属加载检查已有结果 |
| 实际角色 | 四次近景及中庭捕获通过运行与释放检查 |
| 原生帧身份 | 单姿态位置及轴映射已核对；跨姿态继续验收 |

最终 GPU 批次已退出，退出码为零。
恢复时优先读取现有摘要及失败记录。
测试输入调整用于增强独立控制的可辨识度。
最终数值预算与控制差值阈值保持原值。

已提交范围包含角色着色、加载器及相关测试。
运行契约记录当前径向模式及原生待项。
`face-radial-frozen-source/` 的测试脚本属于较早输入。
提交前须冻结最终脚本和收据，并核对源码摘要。

实际候选为：

```text
RUN/joint-candidate/content-character/hero-npr-face-radial.glb
RUN/scene-audit/character-binding-audit/face-radial-candidate.json
RUN/scene-audit/character-binding-audit/capture_face_radial.py
```

### 服饰镜面候选

`character_surfaces.py` 支持显式选择服饰镜面输入。
函数 `orm_specular_image` 输出 `(P.B,255-P.A,P.R,P.G)`。
私有红绿与三张真实参数图的通道保持已有验证。
公共加载、着色与五份实际服饰已接入。
结果见[服饰镜面验收](../acceptance/nr/cloth-specular-input.md)。

原生参数图 G 为介电镜面系数。
五份服饰的二维高光色带已提取，实际绑定仍待完成。
`specularInput` 的字段与打包遵循 C13 契约。
其原生公式和源 UV 应与新帧的实际绘制再次对应。

证据及草案位于角色审计目录的 `cloth-texture-trace/`。
眼睛公共径向能力可用，真实 GLB 的 `eyeSurface` 仍缺。
额发遮挡、眉毛遮罩、预深度及模板继续逐项核验。
头发、皮肤与丝袜的分支分别保存参数来源。

## 验收与运行边界

恢复执行先核对文件和现存进程。
共享 GPU 的游戏、官方重放和引擎验证串行安排。
已有工具及 Python 环境按路径复用。
大型原始资产保持本机存储，仓库保存工具与摘要。

当前终端的 `python` 命令可能不在 PATH。
本轮文档检查使用以下解释器：

```text
C:/Users/23587/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe
```

画面目标沿用有效区域 SSIM 至少 0.95。
关键区域亮度中位误差目标至多 10%。
算法分项、GPU、资源释放及第二用途按所属契约验收。
相机、噪声和有效遮罩须在调参前固定。

100 米深度门禁保留原失败记录。
双设备、正式性能和历史 P8 待项保留原条件。
NR12 保留 1080p、60 帧目标与持续移动检查。
脸部、发丝、衣料和动态效果另做局部人工核验。

最终交付包含后方区域闭合及完整原生物流。
WASD、原生动画混合、输入失焦及碰撞共同回归。
编辑保存、重开、发布及目录移动须通过。
视频覆盖角色近景、场景环视及带光照移动。

## 新会话启动提示

将以下文字发给新会话即可接续：

> 继续 AzureRender 的终末地原生还原，恢复自动实施。
> 先读根 AGENT.md 与本交接文档，核对实际 Git 状态。
> 已有九份 D3D11 原生帧，官方 1.46 加载与通道查询通过。
> 3534 和 10401 已导出并目视核对 2560×1440 实际图像。
> 复用工具，先导出双锚帧的真实事件、shader、纹理与常量。
> 优先核对脸部附加帧及鼻下、脸颊暗影，再补完整角色材质。
> 随后闭合场景 GI、地面反射、后方资源及中央物流。
> 核对已提交分项、当前工作区与失败证据。
> 按本页任务顺序实施，最终完成全部 NR0 至 NR12 门禁。
