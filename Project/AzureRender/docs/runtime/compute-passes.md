# Compute Pass 运行时说明

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-09-26
> 适用范围：R2 Compute 与材质质量基础接口
> 源码入口：`src/render/ComputePass.hpp`、`src/rhi/Rhi.hpp`、`src/rhi/VulkanRhi.cpp`、`shaders/clear.comp`
> 关联测试：`tests/ComputePassTests.cpp`、`tests/NullRhiTests.cpp`

## 职责与使用场景

命令录制器提供 `dispatch`，Render Core 使用它执行图像清理、材质预计算和后续 GPU 蒙皮等计算 Pass。Vulkan 后端调用 `vkCmdDispatch`，NullRHI 记录工作组尺寸。

## 数据与所有权

计算 Pass 通过 RHI 借用当前命令缓冲区和已绑定的计算管线。资源所有权由 Render Graph 声明，计算着色器不直接管理图像或缓冲区生命周期。`ComputePass` 根据输出尺寸和本地工作组尺寸计算向上取整的工作组数量，尺寸为空或 Pass 被禁用时保持空录制。

## 生命周期与时序

计算管线在资源加载期间创建，Pass 执行前绑定管线和描述符，再调用 `dispatch`。输入和输出资源的状态由 Render Graph 屏障声明；Vulkan 后端执行真实 `vkCmdDispatch`，NullRHI 记录同样的绑定顺序和工作组尺寸。

## 接口契约

`dispatch(x, y, z)` 要求三个工作组计数均为非零有效 Vulkan 范围。`ComputePass` 使用 `(extent + localSize - 1) / localSize` 计算数量；NullRHI 记录 `x`、`y`、`z`，Vulkan 后端原样转发到 `vkCmdDispatch`。

## 线程与同步

命令录制器只能在所属录制线程使用。并行录制和间接计算提交属于后续阶段；R2 保持单命令缓冲区顺序。

## 序列化与兼容

计算管线和着色器不进入场景序列化。着色器由 CMake 的 Vulkan `glslc` 规则编译为安装树中的 SPIR-V。

## 平台行为

本阶段验证 Windows Vulkan。Android 适配保持 Deferred。

## 使用示例

```cpp
commands->bindPipeline(computePipeline);
commands->bindDescriptorSet(layout, descriptorSet);
commands->dispatch((width + 7) / 8, (height + 7) / 8, 1);
```

## 已接入的 GPU Pass

Blackhole 使用四级亮部金字塔。第一级从 Trace 图提取超过阈值的 HDR 亮部，后续级逐级降采样，合成 Pass 把各级结果写入半分辨率图像，再由 TAA/历史累积 Pass 采样。Render Graph 按级声明 Compute 采样与存储写入，并在 TAA 采样前转换状态。

Character 环境图的粗糙度 Mip 由 `ibl_prefilter.comp` 生成。Compute 对方向采样环境并按粗糙度扩散，顶层用于漫反射环境照明，其余层用于镜面反射。每次创建环境资源时执行初始化计算。

Character 的 `skin.comp` 对每个顶点混合 Morph 目标、读取关节矩阵并写出逐帧顶点缓冲。阴影、轮廓和主材质 Pass 共用该输出。设备能力或运行参数禁用 Compute 时，顶点着色器执行对应蒙皮与 Morph 运算。公共验证网格带有一个线性关节动画和一个位置 Morph 目标；`AzureRender.CharacterMorphComputeGpu` 比较动画末帧的 Compute 与顶点回退捕获。

## 诊断与排错

NullRHI 中检查工作组尺寸是否符合 Pass 预期。Vulkan 验证层报告资源访问错误时，检查计算 Pass 前后的 Render Graph 状态和屏障。

## 验收与证据

`ComputePass`、NullRHI 和 Vulkan Compute Pipeline 契约在 Debug 构建中通过。`clear.comp` 作为首个可编译的 Compute 着色器入口，后续材质、Bloom、蒙皮和 Morph Pass 复用同一契约。

## 参考来源

Vulkan `vkCmdDispatch` 规范和 Vulkan GLSL `local_size` 语义；Piccolo 的渲染资源分层用于接口职责校对。
