# 编辑器采样回归

日期为 2026-11-12。
Debug 和 Release 分别通过 `AzureRender.SamplingHostGpu`。

## 采样切换

两种配置各执行九次采样设置切换，覆盖 0 至 4 的全部合法模式。
每次切换后等待帧提交，核对实际模式和场景尺寸。
超采样的场景宽高分别为输出的两倍，其余模式使用输出尺寸。
越界值 5 返回设置范围错误，文档版本保持一致。

## 编辑器生命周期

超采样模式通过窗口尺寸变更和资源重载检查。
模式 2 随关卡保存，并在文档重载和独立进程重开后保留。
Vulkan 验证错误及卸载后的缓冲和图像分配均为零。

[结果清单](evidence/2026-11-12/editor-sampling-regression/results.json)
与[验证归档](evidence/2026-11-12/editor-sampling-regression/validation.zip)记录首轮结果、两配置复验及构建产物摘要。
