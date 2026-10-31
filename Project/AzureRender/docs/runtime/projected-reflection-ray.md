# 投影空间反射射线

`ProjectedReflectionRayGpu` 生成颜色、足迹和可信度。
输入借用当前表面及前帧图像。
投影、深度和历史身份由调用者提供。
计算对象持有每帧槽位的参数及描述符。

## 参数布局

`ProjectedReflectionView` 声明具名的列向量变换。
法线变换及视图方向投影各使用三个向量。
四组位置变换各使用四个向量。
相机位置另占一个向量。

| 字段 | 作用 |
| --- | --- |
| `worldNormalToView` | 世界法线到视图方向 |
| `viewDirectionToClip` | 视图方向到裁剪方向 |
| `clipToView` | 当前裁剪位置到视图位置 |
| `relativeWorldToClip` | 相机相对世界位置到裁剪位置 |
| `clipToWorld` | 当前裁剪位置到世界位置 |
| `previousClipToWorld` | 前帧裁剪位置到世界位置 |
| `position` | 当前相机世界位置 |
| `depthReciprocal` | 设备深度的正向距离解码系数 |

设备深度范围为零至一。
正向距离按 `1 / (A * deviceDepth + B)` 还原。
透视变换及图像上下方向须与输入生产者一致。
主场景装配另行核对像素原点、抖动和历史提交身份。

`projectedReflectionView(current, previous)` 适配引擎相机。
输入复用 `ReflectionTraceView` 的透视投影、位置和正交基。
输入投影使用零近一远的设备深度。
适配结果使用一近零远的反向设备深度。

当前与前帧各自提供实际采样投影及姿态。
前帧为空时，前帧变换采用当前视图。
视场、投影偏移和近远平面按各帧独立计算。
正交投影产生明确的类型诊断。

`convention` 声明视图中的 Y 方向约定。
默认 `MirroredViewY` 表示视图与重投影的 Y 反射约定。
`VulkanUpperLeft` 表示正高度视口及上方 UV 原点。
相机适配函数选择 `VulkanUpperLeft`。

`pack(sourceExtent, traceExtent, settings)` 返回 480 字节。
布局为三十个连续的四分量向量。
源尺寸与射线尺寸可以相同。
半尺寸沿每轴向下取整，单像素轴保留一个像素。

## 设置范围

| 设置 | 默认值 | 合法范围 |
| --- | --- | --- |
| `jitterSeed` | 0 | 0 至 16,777,215 |
| `maxSteps` | 32 | 1 至 4,096 |
| `edgeFade` | 0.1 | 大于零且至多一 |
| `motionBlend` | 0 | 零至一 |
| `maximumFootprintLevel` | 6 | 大于零且至多十六 |
| `motionEncoding` | `FourthRoot` | `FourthRoot` 或 `LinearUv` |

矩阵、位置、深度系数和浮点设置须为有限值。
深度解码在合法表面范围内产生正向距离。
尺寸各限制为一至 8192，帧槽数量为一至八。
参数及资源错误在命令录制前报告。

`edgeFade` 使用命中 UV 到最近屏幕边缘的归一化距离。
边缘系数为 `clamp(distance / edgeFade, 0, 1)`。
该系数参与可信度计算。

采样位移为 `(1 - motionBlend) * motion`。
`motionBlend=1` 使用本帧选样位置。
该设置关闭前帧深度接收检查。
本帧路径的颜色输入借用当前快照。

`motionBlend=0` 完整使用当前减前帧的位移。
有效前帧深度参与世界位置一致性核对。

## 借用资源

源尺寸用于前帧线性 HDR 颜色及运动纹理。
前帧深度、当前深度、遮罩及法线粗糙度使用射线尺寸。
输入视图在录制前进入 `SHADER_READ_ONLY_OPTIMAL`。
采样采用最近点及边缘钳制。

法线的 RG 通道使用 Y 极点八面体编码。
B 通道保存粗糙度。

`FourthRoot` 的运动 RG 以 0.5 为中心。
该编码保存带符号的四次方根位移。
解码位移的幅度为 `abs(2 * encoded - 1)^4`。

`LinearUv` 的运动 RG 保存当前减前帧的 UV 位移。
非有限线性位移按零位移处理。
静态位移的运动衰减为一。

活动瓦片通过 GPU 存储缓冲借用。
高十六位保存横坐标，低十六位保存纵坐标。
每个瓦片覆盖八乘八像素。
调用者保证活动瓦片唯一且位于输出范围内。

单次派发最多包含 65,535 个活动瓦片。
描述符只发布活动前缀所需的字节范围。
缓冲容量和活动数量在录制前核对。
零派发保持图像内容和命令录制状态。

| 输出 | 格式 | 内容 |
| --- | --- | --- |
| `color` | `R11G11B10_FLOAT` | 最大分量压缩颜色 |
| `footprint` | `R16G16_UNORM` | 运动衰减及归一化过滤足迹 |
| `confidence` | `R8_UNORM` | 反射可信度 |

三份输出以 `GENERAL` 布局借用。
可信度的初始内容提供反射资格，派发时先读后写。
输出之间及输入输出之间保持互不别名。
设备存储支持按[Vulkan 格式规则](https://docs.vulkan.org/spec/latest/chapters/features.html)逐格式检查。

## 生命周期与构建

调用者准备输入可见性和输出初始内容。
调用 `record` 后，由后续消费者安排布局及依赖。
同一在途槽位的全部消费者退休后，才能复用该槽位。
销毁计算对象前须等待全部所属提交完成。

同一次提交可使用不同槽位和不同尺寸的资源。
各槽位拥有独立参数缓冲及描述符集合。
源图像、输出图像及瓦片缓冲由调用者持续持有。
主场景图像和历史的拥有者负责其成功提交身份。

CMake 发现 DXC 时提供 `AzureProjectedReflectionShaders`。
该目标生成 `shaders/projected_reflection_trace.comp.spv`。
构造函数接收调用者提供的计算程序字节。
独立编译及校验工具按以下方式执行。

```powershell
python tools/compile_projected_reflection.py `
  --sdk D:/Tools/VulkanSDK/1.4.350.0 `
  --output build/runs/<任务目录>/compiler
```

实现及冻结来源检查见[公共接口验收](../acceptance/nr/projected-ray-contract.md)。
相机、独立镜面及失败控制见[相机适配验收](../acceptance/nr/projected-camera-adapter.md)。
公共快照的编码、预算及同步见[表面准备](projected-reflection-preparation.md)。

场景合成及连续历史按[反射接入计划](../plans/2026-10-28-reflection-filter-integration.md)实施。
