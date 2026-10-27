# 原生表面转换

作者法线、独立 UV 流及目标烘焙见[原生网格转换](../development/native-surface-uvs.md)。

原生场景和样条共用 `native_surface_materials.py`。
该模块包装显式提供的 Blender 提取器。
提取器负责纹理加载和采样域。
公共适配器负责已核实分支的表面代数。

## 分支与公式

当前适配 Lit 记录 477 的双纹理三通道分支。
Shader 身份、关键词和纹理模式共同限定范围。
其他分支使用上游转换器。
独立 R 遮罩采样域触发明确的待支持诊断。

基础色依次执行乘色、亮度、饱和和覆盖。
基础粗糙度由 NRO 蓝色通道重映射。
模式 1 从基础色 Alpha 读取金属度。
模式 2 使用材质的金属度常量。

各通道权重采用以下公式。
Alpha 来自对应的遮罩颜色。

```text
adjusted = saturate((mask + offset) * (1 + scale) - 0.5 * scale)
weight = adjusted * alpha
```

表面按蓝、绿、红顺序混合。
三个通道分别影响颜色、粗糙度和金属度。
颜色目标为遮罩颜色的绝对线性值。
输出交给公共通道烘焙器及 glTF 管线。

## 法线解码

已核验的 Lit 分支读取 NRO 的红、绿通道。
两通道映射到有符号 XY，绝对值小于 `0.012` 时归零。
Z 为 `sqrt(max(0, 1 - dot(XY, XY)))`，下限为 `1/2048`。
`_NormalScale` 仅缩放 XY，之后进入切线空间转换。

法线强度须为有限非负数，布尔值产生类型错误。
失败发生在创建 Blender 材质之前。
输出记录 `NativeLitPackedNormal/1`。
纹理坐标、烘焙目标及作者切线沿用所属采样域。

阈值、原生输入及实际镜头见[法线验收](../acceptance/nr/lit-normal-decoding.md)。

## 颜色上传

`color_encoding` 显式声明四组表面颜色的输入域。
`gpu-linear` 接收 GPU 线性值，也是适配器默认值。
`authored-srgb` 将 RGB 按 sRGB 传输函数转为线性值。
Alpha 保持线性，输入记录保持原值。

转换范围为 `_BaseColor` 和 `_MaskAlbedoR/G/B`。
自发光及其他颜色保留所属分支的解释。
非有限值、错误长度和编码冲突产生明确错误。
输出记录 `SurfaceColorEncoding: gpu-linear`。

场景和样条生成命令默认读取作者域颜色。
`--surface-color-encoding` 可显式选择上述输入域。
两类入口通过同一表面适配器执行转换。

中央地面的两个来源绘制已核对实际 UV 别名流。
其 `TEXCOORD0/1/4` 共用第二顶点流的首组坐标。
该绑定结论按已核验的绘制范围使用。
详情见[地面运行绑定](../acceptance/nr/floor-runtime-binding.md)。

## 来源与验收

场景块选择见[来源选择说明](../development/native-source-selection.md)。
明确的来源列表在网格与材质装配前执行。

转换输出记录表面契约及采样域状态。
静态场景和样条收据保存适配器摘要。
源材质、上游转换器及纹理仍持有各自来源。
代数正确性和完整画面等价分别验收。

CPU 烘焙覆盖实际源像素及合成参数。
独立参考解释保留的 DXBC 指令。
原生网格 UV、运行时绑定及常量上传另行核验。
结果见[地面材质验收](../acceptance/nr/floor-surface-conversion.md)。

测试入口为 `tools/blender/test_native_surface_materials.py`。
通过 Blender 后台模式执行该脚本。
参数显式提供提取器、材质清单和 Shader 证据收据。
证据中的汇编、参数及类型文件均核对摘要。

`--extractor-src` 指定上游脚本目录。
`--manifest` 指定场景清单，`--output` 指定输出目录。
`--shader-evidence` 指定独立源指令参考收据。
`--adapter` 可选择公共表面或样条入口。
`--color-upload-evidence` 接收实际 GPU 颜色参考。
纯数据验证入口为 `tools/test_native_surface_colors.py`。
