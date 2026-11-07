# 构建与临时产物管理

本页供开发者和 AI 执行日常清理。
清理入口为 `tools/artifact_cleanup.py`。
命令均在引擎根目录执行。

## 目录与保留规则

| 用途 | 目录 | 保留规则 |
| --- | --- | --- |
| 主开发构建 | `build/ninja-msvc-debug` | 持续复用 |
| 主发布构建 | `build/ninja-msvc-release` | 持续复用 |
| 当前交付 | `build/ux-2026-10-14/Engine` | 保留可用安装 |
| 重大交付留档 | `build/p8-final-2026-10-14` | 人工确认后才删除 |
| 新任务临时产物 | `build/runs/` | 结束且停止写入 7 天后清理 |
| 后续重大版本 | `build/archives/<版本号>-<提交短码>/` | 明确里程碑后留档 |
| 清理结果 | `build/maintenance/cleanup-latest.json` | 覆盖保存最近一次结果 |
| 正式证据 | `docs/acceptance/` | 由 Git 管理 |

主构建依赖缓存与 `build/qa-python` 持续保留。
依赖索引恢复前，应保留已有可用依赖。
源码、用户项目和正式证据分别维护。
安装副本应复用当前交付目录。

## 创建与结束任务

创建临时输出目录：

```powershell
python tools/artifact_cleanup.py --new-run ui-layout
```

目录名为 `YYYYMMDD-HHMMSS-任务名-随机短码`。
任务名使用小写英文与连字符。
日志、截图、临时脚本和测试包放入该目录。
测试工具的输出参数也应指向该目录。

任务结束后，先归档必要摘要和失败证据。
随后标记目录可以进入保留期：

```powershell
python tools/artifact_cleanup.py --finish "build/runs/<目录名>"
```

需要继续使用时，先删除该目录的 `.finished`。
需要长期保留时，在任务根目录创建 `.keep`。
重大版本记录提交、构建配置、指纹和验证摘要。
同一里程碑保存一份交付包。

重大版本由正式交付计划明确指定。
日常阶段产物按普通任务规则管理。

保留标记位于 `build/runs/<任务目录>/.keep`。
清理工具按任务根目录读取该标记。
目录中的项目、视频及证据共同保留。
长期保留任务登记于保留清单的 `preservedRuns`。

角色参考、完整环境及画质诊断分别登记用途和入口。
输入锁清单记录实际程序、着色器及资产摘要。
当前可复现基准按清单路径与保留标记共同维护。

原生中庭的可玩工程及来源按 NR12 归档周期保留。
清单登记后廊四向画面、原速视频及抗锯齿对照。
来源快照、几何误差与失败门禁收据共同保存。
当前入口见[中庭完整性验收](../acceptance/nr/atrium-completeness.md)。

## 执行清理

查看候选：

```powershell
python tools/artifact_cleanup.py
```

执行删除：

```powershell
python tools/artifact_cleanup.py --apply
```

每周日北京时间 10:00 执行默认候选扫描及分批清理。
任务标识为 `azurerender`，工作目录为 `D:/Projects/FYP/Project/AzureRender`。
每批最多十个目录，采用七天保留期及当前有效清理授权。
清单、实际删除结果与空间复核写入 `build/maintenance/`。
自动任务依赖 Codex 的本机调度与执行环境。
构建或测试运行期间，自动任务延后清理。
离线期间可用上述命令手动补做。
配置核对见[自动任务收据](artifact-automation.json)。
本机调度要求见[官方说明](https://learn.chatgpt.com/docs/automations?surface=app)。

实际删除前保存该批次的全部绝对路径。
校验恢复归档及成员清单的 SHA-256。
候选的全部当前文件须匹配已验证归档成员。
硬链接条目沿 `link` 解析到实际内容摘要。
归档以外的内容先完成增量归档及恢复核验。
执行依据为当前会话的有效授权。
用户授予自动清理权限后，核验通过的批次直接执行。
每批至多删除十个目录。
每批复核卷余量、主构建完整性及 Git 修改。
`--run-list` 接收已审阅任务绝对路径的 JSON 数组。
`--apply` 在候选超过十项时拒绝整批执行。
指定任务仍须满足期限、标记、跟踪及路径规则。

```powershell
python tools/artifact_cleanup.py --run-list build/maintenance/batch-01.json
python tools/artifact_cleanup.py --run-list build/maintenance/batch-01.json --apply
```

默认范围仅含符合保留期的任务目录。
目录内任何新写入都会延后清理时间。
工具保护 Git 跟踪文件，并拒绝链接和目录联接。
失败记录写入清理结果，命令返回非零状态。

`--legacy-list` 用于一次性历史目录清理。
清单内容必须经过逐项审阅。
定时任务只使用默认清理范围。
历史报告中的构建路径是当时的证据来源。

查看当前结果应使用已归档的正式证据。

## 本机清理记录

2026 年 10 月 14 日完成历史产物清理。
清理文件逻辑大小约为 91.3 GiB。
清理后 D 盘可用空间约为 133 GB。
详细目录与字节数见 [清理清单](cleanup-2026-10-14.json)。

保留清单见 [交付与构建登记](artifact-manifest.json)。
2026 年 10 月 26 日的任务、缓存与文档处理见[清理记录](cleanup-2026-10-26.json)。
截图保留视频、命名图片和每组首、中、末帧。
当前引擎入口指纹与清理前一致。

主构建目录及其依赖已核对存在。

清理工具的 6 项测试全部通过。
检查覆盖预览、期限、保留标记和删除范围。
同时覆盖 Git 文件、目录联接及只读文件保护。
执行命令为 `python tools/test_artifact_cleanup.py`。

## 本机空间管理

本机阶段产物、候选项目和资产缓存保存在 D 盘。
运行前核对可用空间，验证结束后清理可重新生成的缓存。
原始截帧、固定输入、验收截图和收据按所属保留规则保存。
清理前核对绝对路径、资源用途及活动进程，并保存清理记录。

## 空间门禁

`tools/artifact_budget.py` 提供创建前与写入前检查。
`--new-run` 在创建目录前执行同一门禁。
单任务逻辑占用与预计新增量合计至多为 2 GiB。
预计新增量预留后，卷余量须至少为 50 GiB。
同时须至少保留卷总容量的 10%。
任务目录保存创建时的 `budget.json`。

```powershell
python tools/artifact_budget.py --planned-bytes 2147483648
python tools/artifact_budget.py --run "build/runs/<任务目录>" --planned-bytes 1048576
```

低空间诊断须显式声明 `--diagnostic`。
目录现有占用与预计新增合计至多为 64 MiB。
预留后须剩余 5 GiB。
诊断复用现有输入，输出以日志及结构化摘要为主。

```powershell
python tools/artifact_cleanup.py --new-run performance-check --diagnostic --planned-bytes 67108864
```

门禁返回非零状态时，调用方停止对应写入。
每次采集、安装及复制边界重新检查任务实际占用。
本工具提供调用方门禁，文件系统配额需独立配置。
外部程序的持续写入由其调用方预算管理。
当前超额目录保留文件，并进入人工审阅清单。

空间恢复所需的增量备份写入 `build/archives/`。
备份按文件预留原始大小及额外 64 MiB。
整个过程保留至少 5 GiB 卷余量。
每份备份先核对内容摘要，再释放对应展开副本。
重复内容可引用已验证归档成员。
恢复清单保存每份原路径、摘要和归档成员位置。
当前项目的生成缓存继续保留，便于复测。

任务通过共享资产路径引用已有内容。
项目挂载和资源根负责定位，输入锁记录资产摘要。
截图按每组首、中、末帧及必要失败帧保留。
`.keep` 与 `preservedRuns` 应一一对应。
每项保留记录包含用途、基准入口及复核条件。

自动任务先保存预览，再按完整路径清单分批应用。
每批至多十个目录，授权范围沿用当前有效授权。
状态清单与任务收据共用 `DefaultPreviewThenReviewedRunListsApply`。
累计诊断预算的检查见[预算计划](../plans/2026-11-07-cumulative-diagnostic-budget.md)。

归档释放前，核对包及清单的 SHA-256。
每份待释放文件逐字节摘要匹配归档成员。
活动项目挂载及当前冻结输入所在任务持续保留。
已验证展开副本可按当前授权释放并登记归档入口。
重大里程碑的正式完成仍按 NR12 所属条件验收。
归档收据的历史 `verified` 状态供复核使用。
历史根目录需映射到本次确认的项目根目录。
归档以外的文件先完成增量归档及恢复检查。
正式证据与 Git 仓库由版本管理规则保护。

本次分级策略与空间事实见[恢复核验](../acceptance/nr/resume-storage-performance.md)。
