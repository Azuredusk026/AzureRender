# 证据盘点与恢复

本页用于核对证据用途、归档与恢复。
命令在 `Project/AzureRender` 目录执行。
保留决定见[证据清单](evidence-retention.json)。

## 引用与体积

```powershell
python tools/audit_repository.py --source ../.. --inventory --output build/maintenance/evidence-inventory.json
```

盘点扩展原有卫生报告，保持原字段兼容。
统计包含逻辑大小、独立文件数据量及引用来源。
硬链接身份通过 `os.stat` 核对。
独立数据量与实际分配空间分别计量。

引用扫描覆盖嵌套说明、脚本、导航与 JSON 清单。
单个文本输入上限为八 MiB。
动态拼接路径由操作者按用途核对。
报告只列出统计与引用，处理决定进入证据清单。

## 归档与校验

以下命令展示参数形式。
将任务路径与输入文件替换为审阅后的实际清单。

```powershell
python tools/evidence_archive.py pack --source build/runs/closed-task --output build/archives/closed-task.zip --input src/example.cpp
python tools/evidence_archive.py verify --archive build/archives/closed-task.zip
python tools/evidence_archive.py restore --archive build/archives/closed-task.zip --output build/runs/recovered-task
```

归档保存逐文件 SHA-256、原始字节与来源提交。
`--input` 显式包含所需源码、配置或其他文件。
对应已跟踪输入的工作树补丁进入归档。
未跟踪输入按实际字节保存。

恢复目标必须是新目录。
输入内容位于 `inputs/`，原任务内容位于 `payload/`。
归档包含独立的 `manifest.json`。
校验覆盖成员集合、字节长度与每项哈希。

归档与恢复的解压总量上限为四 GiB。
启动前核对原文件、压缩包与恢复目录的峰值空间。
目录联接、路径穿越和符号链接按错误处理。
被修改或损坏的归档需要保留诊断并重新生成。

## 源目录处理

`pack --remove-source` 在校验后处理原任务目录。
源必须为 `build/runs/` 的直接子目录。
目录需有 `.finished` 且满足保留清单的处理决定。
工具保护 `.keep`、Git 跟踪文件与活动写入。

P3 的原始字节快照按其 manifest 保留。
P5 的压缩诊断与失败证据按历史审计用途保留。
本轮已归档诊断的恢复命令位于证据清单。
