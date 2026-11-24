# 缓冲内存类型

`GpuBuffer` 保存请求用途、内存类型索引和属性标志。
真实索引来自 `VmaAllocationInfo::memoryType`。
属性按设备的对应 `VkMemoryType` 读取。
类型索引在当前分配的生命周期内保持有效。

空后端使用 `UINT32_MAX` 表示未知类型。
它保留请求用途，属性标志采用零。
调用方通过完整缓冲描述符交还拥有者。
销毁后，全部描述字段恢复缺省值。

分配器统计累计观测次数及存活分配字节。
存活字节区分设备本地类型与非本地类型。
可映射 GPU 用途另有总量及非本地子集。
GPU 用途包含统一、存储、顶点、索引和间接缓冲。

统计使用实际分配字节，可能大于请求的缓冲长度。
真实后端的两类存活字节合计等于存活缓冲字节。
空后端的未知类型独立于这些真实观测。
统计在分配和销毁边界更新。

宿主的运行报告在 `allocator.bufferMemoryProperties` 输出统计。
类型属性与实际物理驻留、带宽和访问次数分别解释。
统一内存设备也可设置设备本地属性。
性能结论继续采用符合环境条件的实际工作量。

字段语义依据为 [VMA 分配信息](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/struct_vma_allocation_info.html)。
属性语义见 [Vulkan 内存标志](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html)。
