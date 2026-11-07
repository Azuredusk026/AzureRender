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
python C:/Users/admin/.codex/skills/azurerender-artifact-maintenance/scripts/maintain.py status
```

正常状态直接继续开发。
状态查询读取索引、卷余量及目录根标记。
递归盘点只用于首次建立历史清单或明确的新范围。

每日维护入口为：

```powershell
python C:/Users/admin/.codex/skills/azurerender-artifact-maintenance/scripts/maintain.py daily --apply
```

普通任务完成后保留一天，必要诊断显式选三天或七天。
到期直接回收可重建的程序、中间件及缓存。
每次至多十个受管任务，维护预算一百二十秒。
活动构建、测试及采集期间延后维护。

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

```powershell
python tools/artifact_cleanup.py --finish build/runs/task-directory-name
python tools/artifact_maintenance.py resume --run task-directory-name
```

封存自动保存有上限的精简证据。
继续写入前先重开，活动输入引用在消费者封存后解除。
单任务上限为 2 GiB、10000 个文件，受管池上限为 16 GiB。
生命周期代次用于保护并发重开及恢复。

## 历史缓存

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
