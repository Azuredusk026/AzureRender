# G22 压缩纹理与 mip 交换实施计划

状态：非延期自动门禁通过。

目标：二维 DDS/KTX2 经公共资产操作交换。
容器 mip 进入材料与实际纹理上传路径。
正式范围见[格式扩展计划](2026-10-11-asset-format-expansion.md)。

## 模块与契约

RenderCore 拥有容器解码及编码适配器。
每次调用独立拥有库对象与像素缓冲。
编辑界面使用格式注册描述与公共任务。
独立交换工具验证另一种合法使用方式。

新增 `TextureContainerAdapter.hpp/.cpp`。
`decodeTextureContainer` 返回 `DecodedImage`。
`encodeTextureContainer` 返回有预算的容器字节。
`DecodedImage` 显式携带逐层二维 RGBA 数据。

修改 `ImageCodec` 与 `ImageFormatAdapter`。
注册 DDS/KTX2 的探测、导入与导出设置。
修改 `ImageEncoder` 接入相同事务及损失报告。
尺寸上限为 8192，源及输出上限为 64 MiB。

修改 `GltfLoader`、`MaterialData` 与渲染器。
类型化 mip 契约贯穿独立纹理及 glTF 图像。
颜色与材质因子逐层一致应用。
关闭 mip 时只上传基础层。

DirectXTex 处理 DDS 与 BC 编解码。
KTX-Software 处理 KTX2 与 Basis 转码。
设备能力查询约束最终采样格式。
回退解码总像素预算为 256 MiB。

## 验收重点

二维纹理包含非方形及非二次幂尺寸。
立方体、数组及体纹理分别报告支持范围。
线性数据、sRGB 与法线保留明确语义。
畸形层偏移、截断及巨大尺寸须受控拒绝。

压缩测试固定误差容差并保留透明度检查。
取消覆盖解码、编码及最终发布边界。
重导入保持标识与材料引用。
移动包隐藏来源后在两设备实际播放。

## 执行任务

- [x] 新建 `TextureContainerTests.cpp`。
- [x] 运行失败测试，记录缺失能力。
- [x] 实现二维容器读写与逐层 mip 契约。
- [x] 验证 RGBA、BC、Basis 与损坏输入。
- [x] 接入公共操作、动态设置与材料上传。
- [x] 执行两配置 CPU 及受影响 GPU 回归。
- [x] 验证双设备、重导入及隔离移动包。
- [x] 核验固定依赖来源与组合许可。
- [x] 同步支持矩阵与证据，提交 `feat(g22)`。

测试入口为 `AzureAssets.TextureContainers`。
发布使用现有安装清单与隔离验证工具。
P5 正式性能与长跑集中在 P8。
