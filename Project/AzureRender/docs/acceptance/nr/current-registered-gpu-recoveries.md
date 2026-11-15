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

## 构建输入复核与复测

Release 第十三组执行九项 GPU 检查。首次运行的执行结果全部通过，构建来源复核检出共享工作区缺失第三方文件，该次分组等待复测。恢复 34 份已跟踪文件后，按摘要核对 .NET 来源清单的字节，重新核对 Release 的 1,295 个产物和 Debug 的 1,288 个产物。Release 第十三组按原命令复测，九项全部通过。

[复测结果](evidence/2026-11-15/registered-vendor-source-recovery/results.json)和[验证归档](evidence/2026-11-15/registered-vendor-source-recovery/verification.zip)保存两次注册输出、缺失来源记录和产物摘要。

## 生成物池容量与宿主复测

Release 第十四组首次运行记录开发工具宿主通信超时及临时日志句柄占用。生成物池达到上限后，调度器结束该分组。三份到期着色器目录完成逐文件归档，释放 705,311,360 字节配额。中止的着色器热重载夹具逐文件核对并保留。

同一 Release 产物执行原注册命令，五项全部通过。开发工具宿主用时 134.86 秒，着色器热重载宿主用时 60.67 秒，程序化资产宿主用时 86.77 秒。

[复测结果](evidence/2026-11-15/registered-cache-retention-pool-recovery/results.json)和[验证归档](evidence/2026-11-15/registered-cache-retention-pool-recovery/verification.zip)保存首次超时、调度收据、归档摘要及注册复测。

## 生成物池容量与反射运动复测

Release 第三十四组完成十一项注册检查。反射运动用例首次申请 134,217,728 字节时达到生成物池上限，执行在启动渲染前结束。三份到期着色器目录完成逐文件归档，释放 632,109,919 字节配额。同一 Release 产物使用原注册命令复测反射运动，用时 11.98 秒，通过后该组十一项均已通过。

[复测结果](evidence/2026-11-15/registered-reflection-motion-storage-recovery/results.json)和[验证归档](evidence/2026-11-15/registered-reflection-motion-storage-recovery/verification.zip)保存两次注册收据、构建校验、归档摘要及调度结果。

## 探索关与平台关切换复测

Release 第四十二组的关卡切换检查首次达到 600 秒时限。使用同一播放器、相同输入摘要及原注册命令复测，用时 60.73 秒。同一进程完成探索关、平台关切换和平台关通关，收集八枚金币，并通过卸载资源计数检查。

[复测结果](evidence/2026-11-15/registered-game-sequence-recovery/results.json)和[验证归档](evidence/2026-11-15/registered-game-sequence-recovery/verification.zip)保存首次超时、复测收据、输入与运行报告。

## Debug 生成物池与调度恢复

2026-11-16 核对五个到期着色器目录的保留期限、占用状态和归档摘要。归档共收录 1,980 份文件，释放 453,780,625 字节生成物池配额。

主机重启后，上一批次的 SQLite 日志需要回滚。备份资源索引及日志，核实批次进程和子进程均已退出，再由 SQLite 回滚未提交事务。完整性检查通过后，调度器取得执行锁，完成归档并接续验收队列。

[恢复记录](evidence/2026-11-16/registered-debug-pool-recovery/results.json)保存历次调度状态、归档摘要及索引检查结果。

调度器取得资源锁前会核对上一批次的进程状态。已退出批次的索引需要回滚时，调度器持有维护锁，由 SQLite 完成未提交事务的回滚，再核查遗留子进程。回归测试终止持有事务的子进程，核对索引内容和下一批次的锁获取结果。资源调度测试共八项通过。
