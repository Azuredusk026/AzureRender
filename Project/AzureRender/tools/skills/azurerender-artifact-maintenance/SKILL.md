---
name: azurerender-artifact-maintenance
description: 维护本机 AzureRender 开发产物，按天清理可重建内容、控制累计占用、处理冗余缓存与任务收尾。用于目录清理、空间不足及开发产物管理；日常只查轻量索引。
---

# AzureRender 目录维护

项目为 `D:/Projects/FYP/Project/AzureRender`。
先核对当前仓库及 `AGENT.md`。
项目产物、证据及必要恢复包均存放在项目目录。
本机技能调用版本管理中的项目工具。

## 日常入口

会话开始至多执行一次轻量查询：

```powershell
python D:/Projects/FYP/Project/AzureRender/tools/artifact_maintenance.py status
```

正常状态直接继续开发。
状态查询读取索引、卷余量及目录根标记。
递归盘点只用于首次建立历史清单或明确的新范围。

每日维护入口为：

```powershell
python D:/Projects/FYP/Project/AzureRender/tools/artifact_maintenance.py daily --apply
```

普通任务完成后保留一天，必要诊断显式选三天或七天。
到期直接回收可重建的程序、中间件及缓存。
每日至多十批，每批至多十项。
所有批次、模型及缓存回收共用一百二十秒预算。
批次之间核对活动进程，发现开发活动时延后剩余回收。
阻断、时限和批次上限分别记录于收据。
活动构建、测试及采集期间延后维护。

项目技能发现入口为仓库的 `.agents/skills/`。
该目录与完整维护规则均保存在 D 卷。
本机按每日十点的既有日程执行，异常时通知。

开发管线缓存共享 `build/cache/vulkan/`。
程序名称及内容决定缓存代次，全部代次共享同一预算。
总池为 256 MiB、单文件至多 128 MiB、最多八项，闲置七天过期。
每日入口核对至多六十四个直接条目，并与运行时写入共用文件锁。

精简证据每任务至多 8 MiB，总池 2 GiB，保留三十天。
证据保存源码、结果、命令、日志摘录和必要截图。
正式长期结果按项目验收入口及版本管理保留。
只有明确的必要唯一输入才声明完整恢复包。
此类恢复包在项目内保存，总池 8 GiB，释放后保留七天。

## 创建与收尾

以下命令在引擎根目录执行：

```powershell
python tools/artifact_cleanup.py --new-run task-name --planned-bytes 33554432
python tools/artifact_cleanup.py --new-run task-name --planned-bytes 33554432 --input-run existing-run-name
python tools/artifact_cleanup.py --new-run capture-task --planned-bytes 1073741824 --retention-days 3
```

资产、受检程序、着色器及项目挂载沿用既有路径。
任务目录保存新增结果和源码差异。
主 Debug、Release 构建与当前安装目录持续复用。
独立工程副本需有具体专项用途。

持续写入的前台程序采用预算监控：

```powershell
python tools/artifact_maintenance.py run --run task-directory-name --timeout 600 --command program.exe arguments
```

调用方将输出参数指向任务目录。
监控每秒核对任务、卷余量及运行时限。
它提供采样保护，文件系统硬配额独立配置。
普通写入边界通过 `artifact_budget.py` 追加预留。
子进程的临时目录统一为任务目录下的 `tmp/`。
`TEMP`、`TMP`、`TMPDIR` 及 `SQLITE_TMPDIR` 均采用该位置。
运行器同时设置 .NET 主目录、解包目录及 NuGet 缓存。
这些专用目录位于任务的 `tmp/` 内。
临时文件计入任务配额，并按任务保留期回收。
使用独立缓存配置的工具须将其缓存参数指向任务目录。

Python 构建入口复用 `artifact_maintenance.managed_environment`。
调用时传入任务内的 `tmp/`，将结果作为子进程环境。
原型 Debug 可执行目标采用 `/INCREMENTAL:NO`。
主构建配置按项目调试契约持续复用。

```powershell
python tools/artifact_cleanup.py --finish build/runs/task-directory-name
python tools/artifact_maintenance.py resume --run task-directory-name
```

封存自动保存有上限的精简证据。
继续写入前先重开，活动输入引用在消费者封存后解除。
单任务上限为 2 GiB、10000 个文件，受管池上限为 16 GiB。
生命周期代次用于保护并发重开及恢复。

## 历史缓存

每日入口读取 `build/maintenance/asset-copy-cleanup-policy.json`。
模型清单在声明的历史任务范围建立一次，日常复用清单。
与当前存活原件完整摘要一致的 `.glb` 副本按天回收。
当前输入锁中的全部文件、Git 跟踪文件及嵌套工作树受到保护。
源文件、输入锁及副本的身份在删除前再次核对。
逐文件来源保存在项目内的累计映射和批次记录。
同一文件标识的摘要在本次执行内复用，实际释放量按卷余量核对。

模型副本每天至多 32 GiB、512 个文件。
原缓存分项每天至多 10 GiB，全部每日维护共用一百二十秒。
达到配额后继续下一天，活动开发进程结束后执行维护。
声明新历史范围时使用：

```powershell
python tools/artifact_asset_cleanup.py inventory --root build/runs/history-task --lock current-input-lock.json --catalog build/maintenance/model-copies.json
```

每日入口读取 `build/maintenance/cache-cleanup-policy.json`。
按缓存清单与当前输入锁核对冗余 `AZCACHE1` 载荷。
只有与仍保留原始资产完全一致的副本才回收。
当前工程缓存、Git 文件、嵌套工作树及链接保持保护。

每天最多处理 10 GiB，复用目录清单及已知不匹配证明。
记录缓存头、原路径、存活源文件、摘要及实际释放量。
当前基准与正式原始采集按保留登记维护。
历史其他大目录处理时读取[历史目录](references/legacy-recovery.md)。

## 权限与验证

自动维护依据当前会话及持续有效的用户授权。
授权范围内的例行清理按核验结果执行。
自动审批审查拒绝的目标保持原状，保存阻断原因。
日常只汇报异常及必要的实际回收摘要。

纯产物维护运行工具检查及主产物摘要核对。
编译、画面、GPU 和长跑按对应开发任务门槛安排。
普通生成产物按保留的源码与命令重建。
显式完整恢复包才使用 `restore --run`。
