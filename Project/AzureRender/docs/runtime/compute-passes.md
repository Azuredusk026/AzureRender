# Compute Pass 运行时说明

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-09-26
> 适用范围：R3 Compute 与材质质量基础接口
> 源码入口：`src/rhi/Rhi.hpp`、`src/rhi/VulkanRhi.cpp`、`shaders/clear.comp`
> 关联测试：`tests/NullRhiTests.cpp`

## 职责与使用场景

命令录制器提供 `dispatch`，Render Core 使用它执行图像清理、材质预计算和后续 GPU 蒙皮等计算 Pass。Vulkan 后端调用 `vkCmdDispatch`，NullRHI 记录工作组尺寸。

## 数据与所有权

计算 Pass 通过 RHI 借用当前命令缓冲区和已绑定的计算管线。资源所有权由 Render Graph 声明，计算着色器不直接管理图像或缓冲区生命周期。

## 生命周期与时序

计算管线在资源加载期间创建，Pass 执行前绑定管线和描述符，再调用 `dispatch`。输入和输出资源的状态由 Render Graph 屏障声明；当前阶段提供基础计算入口和可编译的 `clear.comp` 示例。

## 接口契约

`dispatch(x, y, z)` 要求三个工作组计数均为非零有效 Vulkan 范围。NullRHI 记录 `x`、`y`、`z`，Vulkan 后端原样转发到 `vkCmdDispatch`。

## 线程与同步

命令录制器只能在所属录制线程使用。并行录制和间接计算提交属于 R5；R3 保持单命令缓冲区顺序。

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

## 诊断与排错

NullRHI 中检查工作组尺寸是否符合 Pass 预期。Vulkan 验证层报告资源访问错误时，检查计算 Pass 前后的 Render Graph 状态和屏障。

## 验收与证据

`clear.comp` 在 Debug 构建中成功编译，NullRHI 的 `dispatch(8, 4, 1)` 记录测试通过。完整 IBL、Compute 蒙皮和 Morph 质量验收仍在 R3 后续任务中。

## 参考来源

Vulkan `vkCmdDispatch` 规范和 Vulkan GLSL `local_size` 语义；Piccolo 的渲染资源分层用于接口职责校对。
