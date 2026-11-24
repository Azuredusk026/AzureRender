# 投影头发主纹理层级

版本二投影头发材质可用 `hair.projectedRamp.primaryTextureFootprint` 声明轴向与对角足迹。基色 RGB、参数和双法线在单采样颜色路径消费该选择。字段、输入检查及材质表布局见[投影头发照明](../../runtime/projected-hair-lighting.md#主纹理层级)。

## 实际场景

夹具的 45 份输入中，头发 GLB 新增一项材质画像字段。GLB 二进制载荷与冻结输入逐字节一致。Release 实际 Player 在 NVIDIA 和 Intel 各采三帧，五类参考附件逐字节匹配冻结输入，PNG 逐字节匹配三纹理对照。

| 设备与描述符 | 完整接收超差 | 冻结头发可见量超差 | 冻结头发 RGB 超差 |
| --- | ---: | ---: | ---: |
| NVIDIA 固定 | 237 | 0 | 0 |
| NVIDIA 索引 | 237 | 0 | 0 |
| Intel 固定 | 494 | 111 | 32 |

完整接收范围为 49,384 个位置，冻结头发为 17,017 个位置。可见量和颜色采用逐通道一字节容差。Intel 的 RGB 峰差为 29 字节。NVIDIA 实际索引描述符路径的三张 PNG 和五类附件逐字节匹配固定路径。

## 构建与检查

Release 和 Debug 分别通过 11 项和 10 项 CPU 检查。两种配置在两台设备上各通过 6 项 GPU 检查，同一设备的五类附件及三帧 PNG 逐字节一致。32 份主 UV 颜色程序通过 SPIR-V 校验，固定和索引路径各 16 份。其余 863 份程序与固定对照逐字节一致。两种配置的材质结构大小及既有字段偏移通过编译器核对。

Release 原始四帧在 146.547 秒内完成，四张 PNG 逐字节匹配冻结输入。运行均通过验证层及卸载计数检查。编译诊断、测试夹具检查、调试符号恢复及逐点读回见[结果清单](evidence/2026-11-12/projected-hair-point-mip/results.json)和[验证归档](evidence/2026-11-12/projected-hair-point-mip/validation.zip)。

## 验收状态

整体画质状态为 `failed-user-review`。参考接收、Intel 冻结头发及联合外观继续按原门槛验收。
