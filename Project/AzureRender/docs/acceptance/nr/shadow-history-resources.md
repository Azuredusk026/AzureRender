# 阴影历史资源层

日期：2026-11-09。
状态：资源契约通过，运行时接入待完成。

## 容量与所有权

`ShadowHistoryCache` 持有独立的工作流和帧槽缓冲。
每像素八字节，整个资源池至多 128 MiB。
两个帧槽和两种工作流支持原生与冻结图像尺寸。
该组合的 4K 输入由容量检查拒绝。

清空票据具有所属对象、帧槽和递增代次。
重复、过期、异属及越界票据均被拒绝。
调用方负责帧槽完成和销毁前的 GPU 完成。
渲染器负责视图资格、失效规则和历史选择。

## 检查范围

Debug 与 Release 均通过 CPU 契约检查。
两配置分别在 NVIDIA 与 Intel 上检查 GPU 行为。
每项检查四次尾部清空和 49152 条首写者记录。
两种工作流、两个帧槽和重叠三角形均参与核对。

各项验证层错误及释放后资源计数为零。
原有 675 个着色器的散列保持受检值。
新增三个着色器服务于资源清空与所属检查。
本单元原生图像及整帧性能进程均为零。

## 验收边界

资源层通过只代表机制具备后续接入条件。
当前渲染路径尚未使用本历史资源。
实际命中比例、失效范围及整帧净收益继续验收。
整体状态仍为 `failed-user-review`。

同步范围参考 [Khronos 官方示例](https://github.khronos.org/Vulkan-Site/guide/latest/synchronization_examples.html)。
后续按[实施计划](../../plans/2026-11-09-temporal-shadow-history.md)推进。
[结果清单](evidence/2026-11-09/shadow-history-resources/results.json)
与[精简证据](evidence/2026-11-09/shadow-history-resources/validation.zip)保存执行记录。
