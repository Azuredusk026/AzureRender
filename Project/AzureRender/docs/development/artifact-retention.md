# 产物管理

## 执行清理

项目产物与恢复包均存放在项目目录。
本机使用[目录维护技能](../../tools/skills/azurerender-artifact-maintenance/SKILL.md)。
日常会话至多执行一次轻量索引查询。
历史大目录采用缓存清单和每日有界清理。
资产数据库的[源文件快照](asset-source-cache.md)采用有界无损压缩。
暖缓存复用验证结果，单链接旧大缓存随资产刷新迁移。

```powershell
python tools/artifact_maintenance.py status
python tools/artifact_maintenance.py daily --apply
```

正常状态直接继续开发，清理记录写入 `build/maintenance/`。
活动构建、测试或采集期间延后回收。
纯产物维护执行工具检查及源文件、主产物摘要核对。
编译、画面、GPU 测量和长跑按所属开发任务安排。

## 保留与预算

| 内容 | 规则 |
| --- | --- |
| 普通生成任务 | 完成后保留一天 |
| 必要短期诊断 | 显式选择三天或七天 |
| 当前基准与唯一输入 | 登记用途、活动引用或保留标记 |
| 精简证据 | 每任务至多 8 MiB，总池 2 GiB，保留三十天 |
| 显式完整恢复包 | 项目内保存，总池 8 GiB，释放后保留七天 |
| 单任务 | 2 GiB、10000 个文件 |
| 受管任务池 | 累计预留 16 GiB |
| 外部程序日志 | 每份至多 16 MiB |
| Vulkan 管线缓存 | 单文件 128 MiB、总池 256 MiB、最多八项，闲置七天过期 |
| 历史模型副本 | 每天至多 32 GiB、512 个文件，与原件完整摘要一致 |

普通生成任务在保留期后直接回收。
精简证据包含源码、结果、命令、日志摘录和必要截图。
编译程序、中间件及可再生成缓存按生成产物处理。
完整恢复包用于显式指定的必要唯一输入。

正式验收证据和历史原始采集按各自保留登记维护。
正式长期结果进入版本管理及所属证据入口。
临时目录的保留标记说明实际用途及复核条件。
普通任务通过活动引用保持输入可用。

## 空间门禁

```powershell
python tools/artifact_cleanup.py --new-run task-name --planned-bytes 33554432
python tools/artifact_cleanup.py --new-run task-name --planned-bytes 33554432 --input-run existing-run-name
python tools/artifact_cleanup.py --new-run capture-task --planned-bytes 1073741824 --retention-days 3
```

任务名使用小写英文与连字符。
返回路径采用日期、任务名和随机短码。
资产、程序、着色器及项目挂载引用已有路径。
独立构建及整套工程复制需有专项用途。

追加写入前检查任务占用及累计预留：

```powershell
python tools/artifact_budget.py --run build/runs/task-directory-name --planned-bytes 1048576
```

普通任务预留后，卷余量至少为 50 GiB 及卷容量的 10%。
低空间诊断须显式声明，已有与新增合计至多为 64 MiB。
该类诊断预留后至少剩余 5 GiB。
目录链接、联接及超额输出按门禁拒绝。

持续写入的前台程序使用预算监控：

```powershell
python tools/artifact_maintenance.py run --run task-directory-name --timeout 600 --command program.exe arguments
```

程序输出参数须指向该任务目录。
子进程临时文件统一放入该任务的 `tmp/`。
运行器设置 `TEMP`、`TMP`、`TMPDIR` 及 `SQLITE_TMPDIR`。
临时文件计入任务配额，并按任务保留期回收。
独立缓存配置通过工具参数指定到任务目录。
监控每秒核对占用、卷余量及运行时限。
超限只停止本次启动的子进程树，结果保持可检查。
采样监控可能有一个周期的超量，文件系统硬配额独立配置。

## 封存与重建

```powershell
python tools/artifact_cleanup.py --finish build/runs/task-directory-name
python tools/artifact_maintenance.py resume --run task-directory-name
```

封存记录完整文件摘要，并保存有上限的精简证据。
继续写入先执行 `resume`。
活动输入通过 `--input-run` 登记，消费者封存后解除。
生命周期代次保护重开、封存、回收和恢复的并发状态。

普通产物按证据中的源码和命令重建。
必要完整恢复包在创建时显式声明 `--retain-recovery`。
该类任务才提供 `artifact_maintenance.py restore --run`。
恢复中断的目录继续计入预算并受保护。

## 旧任务缓存

历史模型清单在声明范围建立一次：

```powershell
python tools/artifact_asset_cleanup.py inventory --root build/runs/history-task --lock current-input-lock.json --catalog build/maintenance/model-copies.json
```

`asset-copy-cleanup-policy.json` 在每日入口装配该清单。
工具只回收同名同大小且完整摘要与存活原件一致的 `.glb` 副本。
当前输入锁中的全部文件、Git 跟踪文件、嵌套工作树及链接受到保护。
存活原件、输入锁和副本的身份在删除前再次核对。
每批至多十项完整路径，删除前持久化记录并预留日配额。
来源映射保存在 `build/maintenance/`，按回收路径记录原件及摘要。

模型副本单日上限为 32 GiB、512 项，缓存分项上限为 10 GiB。
每日全部维护共用一百二十秒，活动开发进程结束后执行。
剩余配额先按清单预筛，同一文件标识的内容摘要在本次执行内复用。
清单及累计来源映射各至多五万项、32 MiB。
版本管理保护从 Git 索引读取路径，索引链接同时保护其名称和实际目标。
普通任务按共享路径读取模型。

开发构建共享 `build/cache/vulkan/`，按设备、驱动与缓存标识复用管线数据。
写入持有文件锁并采用同目录原子替换，磁盘峰值至多为 384 MiB。
每日维护检查该目录至多六十四个直接条目，每批至多处理十个缓存文件。
缓存清理与写入共用文件锁，Git 跟踪文件及链接保持保护。
维护记录写入 `build/maintenance/pipeline-cache-cleanup-latest.json`。

`artifact_cache_cleanup.py` 使用一次性目录清单及当前输入锁。
它仅识别 `.azure/cache/<资产标识>/<指纹>.azcache`。
缓存须具有有效 `AZCACHE1` 头及相符长度、指纹。
载荷摘要须与仍保留的原始资产完全一致。

当前工程缓存、Git 跟踪文件、嵌套工作树及链接保持保护。
冗余缓存可在保留任务内按上述证明回收。
每批记录完整路径、缓存头、存活源文件及实际卷余量。
缓存由源资产重新生成，整个任务保持原有必要证据。

每天至多处理 10 GiB 缓存，维护预算为一百二十秒。
剩余配额按目录清单预筛，超出本次配额的文件直接跳过载荷读取。
目录清单及不匹配证明复用，完整扫描按新增迁移范围安排。
实际释放量以系统卷查询为准。
模型的逻辑大小和文件标识分别统计。
硬链接共享同一文件，容量说明见 [Microsoft 文件链接文档](https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions)。
每日自动任务的状态见[调度收据](artifact-automation.json)。

## 历史空间债务

历史任务与新受管池分别登记。
当前基准、正式原始采集及未提交源码保持各自地址和所有权。
历史重复资产按当前输入、完整恢复覆盖及用途分别处理。
自动审查拒绝的目标保留原状及阻断记录。

历史专项删除每批最多十个目录。
`artifact_cleanup.py --run-list` 接收已核验路径清单。
`--legacy-list` 用于已审阅的独立生成目录。
原始正式总归档在历史重复副本处理期间继续保留。

空间来源见[恢复核验](../acceptance/nr/resume-storage-performance.md)。
三个归档任务释放约 2.32 GiB，见[维护记录](../acceptance/nr/archived-p0-maintenance.md)。
历史工作树的两个生成构建释放约 6.89 GiB，源文件完整保留，
见[构建维护](../acceptance/nr/legacy-worktree-build-maintenance.md)。
十月记录见[清理清单](cleanup-2026-10-14.json)
及[后续记录](cleanup-2026-10-26.json)。
