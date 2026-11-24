# 参考头发近邻层级

版本二头发材质可用 `referenceNormal` 的版本一编码 `hair-point-mip`。参考法线读取 `projectedRamp.snapshotNormal` 声明的 `material-normal-rg` 纹理。受检材质使用线性纹素过滤、最近层级、各向异性一及 mip 链。

参考片元根据轴向与对角梯度计算基础层级。实际采样器偏移用于判断层级是否位于半层边界的 `1/256` 内。边界内采用隐式样本，其他位置采用显式足迹样本。两次采样在片元选择前完成，固定纹理和索引纹理程序具有相同的选择顺序。显式采样消费基础层级，采样器应用自身偏移。[Vulkan 层级计算](https://docs.vulkan.org/spec/latest/chapters/textures.html#textures-level-of-detail-operation)

## 实际场景

| 设备 | 完整接收超差 | 冻结头发法线超差 | 冻结头发可见量超差 | 冻结头发 RGB 超差 |
| --- | ---: | ---: | ---: | ---: |
| NVIDIA | 237 | 0 | 0 | 0 |
| Intel | 494 | 25 | 111 | 568 |

完整接收为 49,384 个点，冻结头发为 17,017 个点。可见量和 RGB 使用逐通道一字节门槛，法线来源使用 10 位编码的一单位判据。NVIDIA 的完整五类附件与三帧 PNG 逐字节匹配已验收基线。Intel 的深度与对象标识逐字节匹配基线，完整接收有 115 处进入门槛，新增超差 1 处。冻结头发 RGB 有 11 处进入门槛，新增超差 4 处。

## 双配置回归

Release 通过八项 CPU 检查，Debug 通过七项 CPU 检查。两种配置在两块设备上各通过五项 GPU 检查。同一设备的两种配置在五类附件及三帧 PNG 上逐字节一致，验证层检查通过，卸载后分配器计数归零。

两种配置各有 895 份着色程序。参考可见量的固定和索引纹理程序通过 SPIR-V 校验，其余 893 份程序与冻结基线逐字节一致。材质结构大小和既有字段偏移通过两配置核对。

Release 原始四帧在 264.843 秒内完成，四张 PNG 逐字节匹配冻结指纹。默认场景的捕获清单符合原始输出契约。输入指纹、执行收据、逐点结果和着色程序见[结果清单](evidence/2026-11-12/reference-hair-mip-boundary/results.json)及[验证归档](evidence/2026-11-12/reference-hair-mip-boundary/validation.zip)。

## 验收状态

整体画质状态为 `failed-user-review`。参考接收、Intel 冻结头发和联合外观继续使用原门槛。
