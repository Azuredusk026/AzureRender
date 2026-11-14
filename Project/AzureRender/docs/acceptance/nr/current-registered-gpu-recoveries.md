# 主线 GPU 定向复验

2026-11-14，Release 的三项注册用例使用原构建和原验收门槛完成复验。完整 GPU 注册回归按配置逐组执行。

| 用例 | 复验检查 | 耗时 |
| --- | --- | ---: |
| `AzureRender.SamplingHostGpu` | 九次采样切换、窗口尺寸变更、重载和存档重开 | 26.23 秒 |
| `AzureEditor.DevtoolsHost` | 探索项目及资产检视项目的预览、刷新、着色器重载和资源卸载 | 161.40 秒 |
| `AzureEngine.ManagedWorkflows.coreclr` | 托管脚本、编辑器预览、打包移动和完整性检查 | 152.87 秒 |

汇总按构建产物、注册清单、测试源码和原始结果的摘要校验每份复验收据。已通过的复验计入通过数，仍待处理的用例保留原状态。NativeAOT 的打包及移动结果随 CoreCLR 一并归档。

[结果清单](evidence/2026-11-14/current-registered-gpu-recoveries/results.json)与[验证归档](evidence/2026-11-14/current-registered-gpu-recoveries/verification.zip)保存首轮结果、复验记录、编译进度及调度检查。
