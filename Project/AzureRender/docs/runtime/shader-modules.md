# 着色模块与共享类型

> 文档类型：运行说明
> 阶段：R8，原型验收通过
> 范围：可选 Slang 模块原型与共享参数

## 所属边界

着色模块属于 Render Core。
构建适配由宿主机工具执行。
公共请求与结果由 Python 数据类型定义。
Player 消费已编译的 SPIR-V。

`ShaderCompileRequest` 声明源、入口和目标。
请求还包含编译器、依赖根、定义和布局描述。
`ShaderCompileResult` 返回产物路径与来源元数据。
请求版本使用整数 1。

## 构建与来源

原型使用 `AZURE_ENABLE_SHADER_MODULE_PROTOTYPE`。
开启时要求 Slang 2026.8。
目标为 SPIR-V 1.3，浮点模式为 `precise`。
运行已编译内容需要 Vulkan 支持。

编译请求通过 `--request` 传入 JSON 文件。
字段为 `schemaVersion`、`source`、`entry` 和 `target`。
还包括 `profile`、`compiler` 和 `includeRoots`。
其余字段为 `layout`、`defines` 与 `output`。

源、模块和包含文件均从声明的根目录解析。
缺失依赖、依赖环与非法入口返回诊断。
展开上限为 64 层和 1024 个文件。
单文件上限为 2 MiB，编译期限为 60 秒。

来源元数据位于产物的 `.spv.json` 文件。
它记录输入、编译器与适配器哈希。
Windows 来源包含同目录的 Slang 动态库。
缓存命中再次检查反射和实际二进制布局。
候选通过校验后安装到指定产物位置。

产物路径须与全部输入文件分离。
调用方按产物路径串行编译与读取。
元数据安装异常时恢复既有二进制。
断电恢复需重新校验哈希并生成产物。

## 共享参数

类型描述为 `shaders/modules/shared-layout.json`。
生成器输出 C++ 头文件与 Slang 声明。
检查模式验证两个输出与描述一致。
当前描述支持 `float32` 与 `uint32` 标量。

每个标量占 4 字节，类型采用 4 字节对齐。
`BloomParameters` 占 8 字节。
`threshold` 位于偏移 0。
`extractBright` 位于偏移 4。

C++ 构建断言成员偏移、对齐和跨度。
编译适配检查 Slang 反射的类型与偏移。
SPIR-V 检查成员装饰与数组跨度。
GPU 夹具使用相同的共享参数执行实际 Pass。

## 算法与策略

`IBlockSamples` 定义四个 RGB 样本的读取接口。
`DirectSamples` 根据坐标直接读取纹理。
`CachedSamples` 提供已经读取的本地样本块。
`filterBlock` 负责求平均和亮度提取。

入口按构建定义选择策略。
`AZURE_BLOOM_CACHE=0` 选择直接读取。
`AZURE_BLOOM_CACHE=1` 选择本地缓存。
两种策略生成两个独立变体。

参考设计来自 gkNextEngine 的算法接口分解。
本原型使用项目现有的 Bloom 算法。
材质数据采用 RGBA16F 纹理。
策略必须满足相同的 RGB 类型契约。

## 验证与使用范围

GPU 对照包含 GLSL 和两个 Slang 变体。
图像输出按相同设备逐字节比较。
独立参考检查半精度存储的相邻可表示值。
同步验证和关闭后的资源数量分别检查。

夹具包含单像素、奇数边缘和常规尺寸。
亮度提取覆盖关闭、正常阈值与负阈值。
各变体记录逐次 GPU 和 CPU 录制成本。
正式性能预算使用探索与检视工作流验证。

正式渲染与开发重载使用现行 GLSL 程序。
Slang 原型产物位于安装树的 `shaders/modules`。
完整采用范围由[阶段验收](../acceptance/r8/2026-10-07.md)确定。
操作见[着色模块对照教程](../tutorials/shader-modules.md)。
