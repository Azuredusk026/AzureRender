# 环境资源运行时说明

环境资源通过 `loadEnvironmentImage` 读取为 RGBA16F。Radiance HDR 保持浮点动态范围；PNG、JPG 等 LDR 输入先转换到线性空间；OpenEXR 的 R、G、B 通道按线性浮点读取，Alpha 固定为 1。

OpenEXR 支持任意数据窗口原点，输出像素按数据窗口从左到右、从上到下排列。缺少通道、空数据窗口、损坏文件或无法打开的文件都会抛出包含文件路径的错误。

发布包包含 OpenEXR、Imath、libdeflate 和 OpenJPH 的运行库及许可证文本。Android 环境资源路径仍由平台适配阶段处理。

关联实现：`src/render/EnvironmentAsset.cpp`、`src/render/EnvironmentAsset.hpp`。关联测试：`tests/EnvironmentAssetTests.cpp`。
