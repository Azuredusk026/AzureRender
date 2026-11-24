# 历史恢复与迁移

## 一次性空间债务

用户原扫描记录任务目录约 949.87 GiB。
主要来源是多份资产、生成缓存及阶段工程的重复展开。
保留标记覆盖约 864.77 GiB，存在长期驻留压力。
历史逻辑大小与实际卷分配分别解释。

日常维护通过十六 GiB 受管池控制新增占用。
普通任务按一天保留期回收，必要诊断选择三天或七天。
历史目录按归档覆盖和实际活动输入分别迁移。
完整扫描结果应缓存，后续按目录身份更新。
每次日常开发的状态查询保持非递归。

## 恢复条件

历史归档为 `build/archives/development-evidence-20261029.tar.zst`。
归档 SHA-256 为：

```text
d14f54f3ede44d6591f43c24743017ad860581f012e2ef80b5a8168d8a1d2e51
```

该归档约 58.58 GiB。
成员索引为大文件，按字段处理并保存摘要结果。
摘要检查点位于 `build/maintenance/20261107-archive-digest-checkpoint.json`。
当前位置、保留用途及来源以项目产物清单核对。

迁移前逐项核对以下输入：

1. 当前项目挂载、冻结参考、正式原始采集和正在使用的输入锁。
2. `docs/development/artifact-manifest.json` 的保留及恢复登记。
3. 原归档、成员索引和候选当前文件的摘要。
4. Git 跟踪文件、嵌套工作树、链接及活动进程。

当前基准的文件地址保持可用。
历史独立副本可采用校验后的冷存储和按需恢复。
保留登记同步记录驻留范围、恢复位置及用途。
NR12 与画质状态继续按照所属验收记录。

每批保存完整路径，最多十个目录。
唯一历史内容先完成增量恢复包及核验。
项目数据与必要恢复包均保存在项目目录。
可重建的缓存及旧构建按已声明用途直接回收。
重复模型以存活原件的完整摘要及累计来源映射作为恢复依据。
普通新任务通过共享资产路径引用所需模型。
迁移须同时检查目标卷预算，实际回收量以源卷查询为准。

## 已知保护范围

`build/q1-ramp-source` 内的三十三个源文件改动及 Git 工作树持续保留。
两个嵌套生成构建已完成归档及回收。
恢复包登记于项目 `recoveryArchives`。

下列删除曾被自动审批审查拒绝，保持原状：

```text
build/runs/20261107-210538-radiance-recomposition-e85c9c/runtime
build/ninja-msvc-debug/*.ilk
build/archives/generated-cache-recovery-20261106.zip
build/archives/generated-cache-recovery-20261106.zip.manifest.json
```

拒绝信息仅为 `blocked by policy`。
对应收据位于 `build/maintenance/`。
任务恢复时沿用该阻断状态。
