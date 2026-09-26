# RHI 同步契约

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-09-26
> 适用范围：R1 Windows Vulkan 渲染核心
> 源码入口：`src/rhi/Rhi.hpp`、`src/rhi/VulkanRhi.cpp`、`src/rhi/NullRhi.cpp`
> 关联测试：`tests/NullRhiTests.cpp`、`tests/NullRhiPassTests.cpp`

## 职责与使用场景

RHI 同步契约把图像和缓冲区的布局、访问类型与流水线阶段交给命令录制器。渲染 pass 通过 `ICommandRecorder` 描述依赖，Vulkan 后端将其转换为 `vkCmdPipelineBarrier`，NullRHI 记录调用顺序、阶段、访问掩码与子资源范围用于无 GPU 验证。

## 数据与所有权

`ImageBarrierDesc` 和 `BufferBarrierDesc` 只借用调用方提供的 Vulkan 句柄，不负责创建或销毁资源。图像屏障包含子资源范围；缓冲区屏障包含字节偏移和大小。资源所有权仍由分配器和 RHI 生命周期管理。

## 生命周期与时序

资源首次使用前提交布局转换，写入完成后提交到读取阶段。屏障在同一命令缓冲区中按录制顺序生效。暂停、交换链重建和帧结束由平台帧流程负责，窗口尺寸为零时等待事件，尺寸有效时立即重建；等待期间收到关闭请求则退出。重建先等待设备空闲并检查返回值。

## 接口契约

`imageBarrier()` 支持显式 `srcStageMask`、`dstStageMask`、`srcAccessMask`、`dstAccessMask`、`aspectMask`、`baseMipLevel` 和 `mipLevels`。两侧阶段均为零时保留按布局推导的兼容路径；显式指定阶段时，访问掩码按调用方提供的值提交。`mipLevels` 至少为 1。仅指定一侧阶段、空子资源范围、无阶段的显式访问掩码会抛出 `std::invalid_argument`，两个后端使用相同检查。

`bufferBarrier()` 使用显式阶段和访问掩码；阶段为零时分别回退到 `TOP_OF_PIPE` 与 `BOTTOM_OF_PIPE`。`size` 可使用 `VK_WHOLE_SIZE`。队列族在当前单图形队列契约中固定为 `VK_QUEUE_FAMILY_IGNORED`。

## 线程与同步

命令录制器只在所属渲染线程使用。描述对象在调用期间保持有效，录制完成后可立即释放。跨队列所有权转移暂不开放，后续扩展必须补充队列族字段和验收。

## 序列化与兼容

屏障描述不进入场景或资产序列化格式。新增字段提供零值默认值，旧调用保持布局推导行为。任何需要持久化的资源状态由渲染图资源声明负责。

## 平台行为

本阶段验证 Windows Vulkan。Android 平台保持 Deferred，不配置工具链、不执行构建，也不建立移动端同步预算。同步接口保持平台无关，具体后端在用户重新安排 Android 阶段后接入。

## 使用示例

```cpp
recorder.bufferBarrier({
    uploadBuffer, 0, VK_WHOLE_SIZE,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT});
```

## 诊断与排错

若验证层报告访问冲突，先核对写入方的 `srcStageMask/srcAccessMask`、读取方的 `dstStageMask/dstAccessMask` 及图像布局。若 NullRHI 测试缺少屏障调用，检查 pass 是否通过 `ICommandRecorder` 录制。Vulkan 后端问题使用验证层日志和最小 pass 重现。

## 验收与证据

Debug 与 Release 的 NullRHI、NullRHIPass、SceneGraph 测试必须通过。阶段证据记录在 `docs/acceptance/r1/2026-09-26.md`。GPU 视觉回归沿用 R0 基线，R1 不改变着色输出。

## 参考来源

Vulkan 规范中的 [`vkCmdPipelineBarrier`](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap7.html#vkCmdPipelineBarrier)、`VkImageMemoryBarrier` 与 `VkBufferMemoryBarrier` 定义阶段、访问和布局依赖。本文按 Vulkan 接口定义描述屏障，未移植第三方引擎代码。
