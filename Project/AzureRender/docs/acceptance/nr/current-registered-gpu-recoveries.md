# 主线 GPU 定向复验

2026-11-14，Release 的三项注册用例使用原构建和原验收门槛完成复验。完整 GPU 注册回归按配置逐组执行。

| 用例 | 复验检查 | 耗时 |
| --- | --- | ---: |
| `AzureRender.SamplingHostGpu` | 九次采样切换、窗口尺寸变更、重载和存档重开 | 26.23 秒 |
| `AzureEditor.DevtoolsHost` | 探索项目及资产检视项目的预览、刷新、着色器重载和资源卸载 | 161.40 秒 |
| `AzureEngine.ManagedWorkflows.coreclr` | 托管脚本、编辑器预览、打包移动和完整性检查 | 152.87 秒 |

汇总按构建产物、注册清单、测试源码和原始结果的摘要校验每份复验收据。已通过的复验计入通过数，仍待处理的用例保留原状态。NativeAOT 的打包及移动结果随 CoreCLR 一并归档。

[结果清单](evidence/2026-11-14/current-registered-gpu-recoveries/results.json)与[验证归档](evidence/2026-11-14/current-registered-gpu-recoveries/verification.zip)保存首轮结果、复验记录、编译进度及调度检查。

## 标准构建定向复验

2026-11-15 使用同一标准 Release 产物、注册命令与时限复验四项检查。注册清单、测试源码、程序和逐项结果通过摘要核对。

| 用例 | 复验范围 | 耗时 |
| --- | --- | ---: |
| `AzureRender.SamplingHostGpu` | 五种采样模式、窗口尺寸、资源刷新和存档重开 | 11.71 秒 |
| `AzureAssets.ProceduralHost` | 探索及资产检视项目的生成、导入、预览及保存 | 119.31 秒 |
| `AzureEngine.ManagedWorkflows.coreclr` | 托管运行、安装、打包移动及完整性验证 | 240.65 秒 |
| `AzureEngine.LocomotionBlendGpu` | 三路动作混合、相位与角色独立性 | 12.83 秒 |

采样宿主首轮在四样点切换后触发帧等待时限，流程宿主首轮在放置骨骼资源后触发请求时限。使用同一编辑器和 897 份程序隔离应用管线缓存，两个用例分别以空应用缓存在 13.19 秒和 100.09 秒通过。流程宿主的隔离复验包含几何编译器和提案脚本，两个项目各核对 97 个操作。驱动缓存由系统继续管理。

CoreCLR 首轮的安装子进程达到 180 秒时限。复验完成安装、移动后的独立启动以及篡改拒绝。动作混合首轮在生成物预约阶段停止。核对并回收 4,708 份重复生成文件，共 555,649,037 字节，原注册用例获得任务空间并通过。

[结果清单](evidence/2026-11-15/registered-host-and-storage-recoveries/results.json)与[验证归档](evidence/2026-11-15/registered-host-and-storage-recoveries/verification.zip)保存失败与通过收据、应用缓存对照及保留文件的摘要映射。
