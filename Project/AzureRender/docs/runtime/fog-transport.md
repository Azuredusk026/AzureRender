# 雾效传输与体积数据

本页定义公共雾效数值和预积分载荷。
类型位于 `FogTransport.hpp` 和 `FogVolumeData.hpp`。
当前范围为数据、CPU 评价及 GPU 采样。
视图投影与场景合成按[视图雾效计划](../plans/2026-10-28-view-fog-composition.md)实施。

## 传输语义

`FogTransport` 持有线性散射 RGB 和逐通道透射率。
散射非负，透射率位于零至一。
默认值具有零散射和单位透射率。
全部输入及结果要求有限。

`composeFogTransport(front, back)` 按相机距离合成。
前段透射率同时衰减后段颜色与透射率。
分段积分和连续积分采用相同的数值语义。
非法值及结果溢出产生异常。

```text
T = front.T * back.T
S = front.S + front.T * back.S
color = linearColor * T + scatteringWeight * S
```

`applyFogTransport` 的散射权重位于零至一。
透明表面采用权重一。
加色发光采用权重零，保留光学衰减。
颜色使用线性 HDR 数值。

## 高度雾

`ExponentialHeightFog` 声明密度、衰减和参考高度。
散射颜色采用线性 RGB。
密度和高度衰减均非负。
线段位置与参考高度采用同一世界单位。

```text
density(y) = density * exp(-heightFalloff * (y - referenceHeight))
opticalDepth = integral(density(y), segment)
T = exp(-opticalDepth)
S = scatteringColor * (1 - T)
```

水平线段和零长度线段具有明确结果。
积分在对数域计算，支持极端高度范围。
零密度保持恒等传输。
改变线段方向保持相同的光学厚度。

## 体积载荷

`FogVolumeData` 持有线性 RGBA16F 数据。
RGB 表示预积分散射颜色，A 表示标量透射率。
数据按 X、Y、Z 顺序排列，X 连续存储。
该格式独立于来源游戏和场景身份。

| 内容 | 规则 |
| --- | --- |
| 文件扩展名 | `.azfog` |
| 标识及版本 | 八字节 ASCII `AZFOG001` |
| 尺寸 | 三个小端无符号三十二位整数 |
| 像素 | 小端 RGBA16F，保留原始半精度位 |
| 单轴尺寸 | 一至二百五十六 |
| 载荷预算 | 至多 64 MiB |
| 文件长度 | 二十字节头部加精确载荷长度 |

读取拒绝错误版本、非法尺寸和不完整载荷。
多余字节、非有限数值及越界透射率同样拒绝。
`sample` 使用归一化坐标和三线性筛选。
寻址采用边缘限制，纹素中心遵循半纹素规则。

`encode`、`decode`、`save` 和 `load` 提供完整往返。
载荷进入 GPU 前调用完整校验。
局部采样校验所访问的纹素及坐标。
视图投影与载荷的空间解释由消费契约声明。

实际来源及检查见[数据验收](../acceptance/nr/fog-transport-data.md)。

## GPU 资源

`FogVolumeGpu` 持有一个不可复制的三维图像。
图像采用线性 RGBA16F、边缘限制及线性筛选。
上传保留完整半精度位，临时缓冲在上传后释放。
格式能力和载荷校验发生在分配之前。

构造与析构复用生产 RHI 的三维资源接口。
后端生命周期覆盖资源对象。
调用方在销毁前结束已提交的资源使用。
`writeTexture` 向调用方指定的描述符位置写入资源。

`fog_transport.glsl` 提供对应的线性合成函数。
原版半精度过滤结果作为真实载荷的对照值。
CPU 全精度插值与硬件格式精度分别记录。
独立测试着色器由测试目标构建。

资源、真实采样及释放检查见[GPU 验收](../acceptance/nr/fog-volume-gpu.md)。
