# GPU 注册回归的产物预约与管线缓存

2026-11-14，主线 Release 的八项采集在启动前触发卷余量门禁。释放卷空间后，七项按原命令复验，均在启动前触发 16 GiB 受管池门禁。材质显示的首轮记录同时包含两项门禁。门禁拒绝时未启动 Player。

## 产物预约验证

按已封存的实际写入量为十个采集脚本核定启动预约。投影相机预约 128 MiB，其余九项各预约 64 MiB。每项预约均覆盖已记录写入峰值的两倍。单任务 2 GiB 配额、受管池 16 GiB 上限与卷余量规则保持有效。

隔离候选沿用 Release 和 Debug 的注册参数、构建产物、时限和图像断言。每种配置均通过八项原受阻用例及九项相邻用例。材质显示每种配置完成 53 次采集及 16 组比较。34 项结果均有独立受管任务及封存收据，写入量低于各自预约。

[双配置清单](evidence/2026-11-14/registered-gpu-infrastructure/capture-reservations-release-debug.json)与[采集归档](evidence/2026-11-14/registered-gpu-infrastructure/capture-reservations-release-debug.zip)保存注册命令、摘要、时限和实测写入。主线采集脚本使用上述预约。正式回归使用原注册命令、主线构建产物和独立收据。

主线以原 CTest 命令运行九项采集，Release 通过九项，Debug 通过七项。Debug 材质辐射和材质显示到达 600 秒进程时限。两组对照均使用同一 Debug Player 和同一验证层。材质辐射的 34 个共同完成子场景命令与帧摘要一致，管线准备中位数分别为 3.42 秒和 13.75 秒。NPR 对照场景的 75 次管线请求在两轮均命中驱动缓存。材质显示完成的 36 帧与通过轮逐像素一致。

固定驱动缓存及场景输入后，交替使用预热和空的验证缓存各重放两次。预热组的管线准备为 12.19 和 12.77 秒，空缓存组为 11.95 和 12.38 秒。四次驱动缓存摘要及输出帧摘要一致。该对照未复现材质辐射两轮间的管线耗时差。

[主线结果清单](evidence/2026-11-14/registered-gpu-infrastructure/registered-main-capture-completed.json)和[主线复验归档](evidence/2026-11-14/registered-gpu-infrastructure/registered-main-capture-completed.zip)保存两种配置的 CTest 结果、材质显示逐帧核对及缓存对照。

Debug 两项材质用例以同一主线产物和原 CTest 命令复验。材质辐射耗时 382.50 秒，完成 36 个场景和 10 组比较。材质显示耗时 564.94 秒，完成 53 个场景和 16 组比较。两项均低于 600 秒门槛，验证层通过，182 帧 PNG 与对应通过轮逐字节一致。通过轮和超时轮均纳入回归记录。

[材质复验清单](evidence/2026-11-15/registered-gpu-infrastructure/registered-material-focused-retests.json)和[材质复验归档](evidence/2026-11-15/registered-gpu-infrastructure/registered-material-focused-retests.zip)保存原注册收据、整个帧集的摘要及两项复验的 CTest 结果。

[材质辐射对照清单](evidence/2026-11-14/registered-gpu-infrastructure/material-radiance-same-input-timeout.json)和[采集归档](evidence/2026-11-14/registered-gpu-infrastructure/material-radiance-same-input-timeout.zip)保存命令、帧摘要与管线日志。

## 雾效管线缓存

`AzureEngine.ViewFogGpu` 在同一 Release 产物上两次达到 600 秒时限。第二次完成全部 25 个索引描述符组合及 20 个固定描述符组合。两次采集中可对照的 43 张图像摘要一致。

两条路径读取相同的管线缓存代次。第二次复验读取约 127.76 MiB 缓存。索引路径命中 2,009 次，创建管线累计约 8.11 秒。固定路径命中 1,084 次，共请求 1,552 次，创建管线累计约 407.62 秒；18 个已完成组合触发单文件保存上限。按实际描述符和蒙皮模式区分缓存代次的候选继续隔离验证。

## Debug 编辑器超时

原宿主的帧计数、引擎状态及着色器状态同批返回。源码修复后生成的 897 份候选程序均与基准逐字节一致，随后场景重新创建完成描述符阶段，等待着色器恢复的请求达到时限。候选编辑器在创建字符预览时也完成描述符阶段，随后达到操作时限。另一轮候选编辑器在首个着色器状态请求处超时。

隔离目录验证通过 897 项字节检查，总耗时为 90.18 秒。该轮采用隔离运行器，正式目录包装的 90 秒进程限制及两个编辑器工作流继续核验。
[超时归档](evidence/2026-11-14/registered-gpu-infrastructure/debug-host-timeout-diagnostics.zip)记录主机观测、原始日志和独立对照。

## 管线调用与着色器读取

雾效缓存分流候选在 Release 耗时 276.28 秒，通过 50 项检查，并与 43 张可对照图像的摘要一致。Debug 在 600 秒内完成 38 项；固定描述符已完成的 13 次缓存保存全部成功，1,012 次请求命中 1,010 次。

Debug 编辑器采得 97 次完整图形管线调用，全部命中驱动缓存。调用累计耗时 23.37 秒，驱动反馈累计 1.24 秒；编辑器仍触发操作时限。Vulkan 验证层在管线创建时按特化值处理并验证 SPIR-V，相关实现见 [Khronos 1.4.350 源码](https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/v1.4.350/layers/core_checks/cc_spirv.cpp#L1803)。

Debug 着色器读取对照覆盖 897 个文件，共 274,002,740 字节。迭代器读取耗时 32.00 秒，分块读取耗时 1.12 秒，逐文件内容一致。目录编译按 90 秒进程限制及原有字节断言核验。

[计时清单](evidence/2026-11-14/registered-gpu-infrastructure/debug-pipeline-wall-time.json)和[诊断归档](evidence/2026-11-14/registered-gpu-infrastructure/debug-pipeline-wall-time.zip)保存两种配置的雾效结果、调用跟踪及读取对照。

[结果清单](evidence/2026-11-14/registered-gpu-infrastructure/results.json)及[验证归档](evidence/2026-11-14/registered-gpu-infrastructure/release-baseline-recovery.zip)保存启动预算、实际采集、缓存日志和原始复验收据。
