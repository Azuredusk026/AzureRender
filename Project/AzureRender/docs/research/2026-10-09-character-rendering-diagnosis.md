# 终末地角色渲染根由报告

> 类型：根由调查与修复记录
> 状态：核心修复已验证，交付验收中
> 日期：2026-10-09

## 根由与修复机制

| 问题 | 已核实根由 | 修复机制 |
| --- | --- | --- |
| 额发白块 | HairShadow 需要调制场景颜色。白色回退纹理被直接显示。 | 版本化场景调色叠层与线性深度权重。 |
| 眼部亮边 | EyeShadow 母材质输出黑色。资产回退值为白色。 | 导入有效基色与透明度。 |
| 眉毛遮挡 | 位移单位和区域解释有误。透明度缺少深度渐隐。 | 米制参数、独立眉毛岛与深度预通道。 |
| 织物材质响应 | B 为 AO，A 为光泽。粗糙度读取了 B。 | 按母材质函数转换，保存独立通道。 |
| 发光响应 | 光泽 A 被复用为发光遮罩。 | 发光读取独立的 `_E` 贴图。 |
| 阴影闪烁 | 先插值深度再比较，偏置受贴图法线扰动。 | 最近邻搜索、比较后过滤与接收面深度修正。 |
| 阴影级联 | 投影未采用真实视场角和纹素对齐。 | 相机投影覆盖、世界尺度半影与边界混合。 |
| 高对比度 | 风格阴影与 AO 同时压暗直接和间接光。 | 分层光照贡献与受控补光。 |
| 头发体积 | 视角 AO、高频描边及行号误作方向偏移。 | 独立遮罩、体积法线与高光参数。 |
| 严重锯齿 | 单样本输出，角色贴图只有一级。 | 分类纹理层级、边缘过滤及四子像素解析。 |

材质类别 3 为头发，类别 4 为织物。
混合金属区域按贴图保留自身响应。
自定义法线的反向统计仅用于定位可疑面片。

## 核实证据

逐材质消融分别关闭 Brow、HairShadow 和 EyeShadow。
关闭 HairShadow 后，额发白块检测降至 18 像素。
修复两个阴影叠层后，同一检测为 0 像素。
检测门槛为 16 像素，保存在 GPU 回归工具中。

测试固定 1280×720、正面近景及额发区域。
AO 独立变化测试已复现粗糙度错误。
线性基色及金属度系数测试使用公共资产。
静止帧、运动序列与同步验证分别保存证据。

证据目录为 `captures/character/repair-20261009`。
原始探针目录为 `captures/character/diagnosis-20261009`。
这些产物位于本机忽略目录。
执行状态见[修复计划](../plans/2026-10-09-character-rendering-repair.md)。

## 参考边界

HairShadow 读取不透明场景颜色与线性深度快照。
颜色转换为 HSV 后，应用实例缩放与 RGB 调色。
快照与透明叠层拥有独立资源和读取依赖。
游戏参考图与本地镜头、光照存在差异。

## 本地实现与联网参考

本地研究源为 `D:\Epic\UE Project\ZMDRender`。
读取了 Brow、Hair、Face、Cloth 等实际材质图。
资料版本由文件 SHA256 固定，见证据清单。
该工程是复刻参考，不能等同于游戏官方算法。

| 来源 | 研究用途与限制 |
| --- | --- |
| [官方角色页](https://endfield.gryphline.com/en-us/operator)与[官方莱万汀角色图](https://web-static.hg-cdn.com/endfield/official-v4/_next/static/media/laevatain.edd103d4.png) | 外观与发色参考。角色宣传图不定义实时着色算法。 |
| [Microsoft 级联阴影](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps) | 深度覆盖、级联混合与投影稳定。 |
| [NVIDIA PCSS 资料](https://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf) | 遮挡搜索、半影估计与比较过滤参考。已取得文件。 |
| [Filament 材质文档](https://google.github.io/filament/main/filament.html) | 金属度、粗糙度、线性色彩与高光抗锯齿。 |
| [Epic 抗锯齿文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/anti-aliasing-and-upscaling-in-unreal-engine) | 空间与时间抗锯齿的作用范围。 |
| [Unity 卡通描边文档](https://docs.unity3d.com/Packages/com.unity.toonshader@0.11/manual/Outline.html) | 描边宽度、颜色和法线控制。 |
| [佩丽卡独立复刻](https://github.com/Morgana-lgtm/Arknights-Endfield-Inspired-Perlica-Character-Shader) | 已读取头发、光照、描边及刘海阴影源码。通道语义可交叉核对。 |
| [终末地 MME 复刻](https://github.com/chris0214/Arknights-Endfield-MME-Shader) | 已读取说明文件。声明原创代码为 MIT，贴图许可独立。 |

联网访问日期为 2026-10-09。
佩丽卡参考取自 `main`，文件哈希已保存。
其许可证尚未确认，候选实现采用自行编写的算法。
民间复刻只用于交叉研究，外观目标以用户参考为准。

## 接收面与抗锯齿验收

斜面上的过滤点具有不同的接收深度。
过滤器按屏幕导数计算接收面斜率。
每个纹素在自身位置执行遮挡比较。
梯度在级联分支选择前计算。

GPU 夹具覆盖深度阶跃与未遮挡的斜面。
接触阴影核按完整五乘五纹素足迹采样。
标量深度比较不能通过斜面夹具。
按接收面深度比较可保持完整可见度。
主着色器与夹具使用同一比较函数。

实时边缘过滤与超采样均采用空间采样。
超采样先完成四个子像素的着色。
解析在线性显示颜色空间进行。
质量档、窗口缩放和保存重开通过生产操作。

## 适配边界

头发曲线采用源工程第六行的线性曲线。
源曲线、生成器与输出纹理均记录哈希。
行号、方向偏移和相机偏移拥有独立字段。

头发 `_P.R` 接入源图的 `Font_Mask` 反相节点。
其完整函数输入映射仍需进一步核实。
当前头发响应包含受控的视觉适配参数。
游戏参考用于外观校准，官方算法尚无完整数据。
设备验收覆盖本机 RTX 4060 Laptop。
