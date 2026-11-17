# 房间功能矩阵

> 已核对 Recovery Windows 1.17.3 的源目录。真实入口和完整运行覆盖继续按各项验收。

矩阵按剧情组、窗景、环境、装饰、语言、设置和平台组织。每项使用原版标识，登记入口、前置条件、输入、预期结果、存档影响、源证据及待测范围。目录中的源条目是逐项核验的候选；完整分母由实际入口、授权及运行证据确定。

| 范围 | 源条目 | 口径 |
| --- | ---: | --- |
| 剧情组 | 610 | 81 种有组的剧情类型、2,289 个命令行、90 个组内分支目标 |
| 窗景 | 29 | 场景分支、枚举编号和隔离图像逐一对应 |
| 环境 | 46 | 29 项有窗景，29 项有环境声；同一环境可包含两者 |
| 装饰 | 99 | 8 项初始可用、13 项剧情解锁、78 项点数购买 |
| 语言 | 7 | 游戏语言枚举中的可选语言 |
| 设置 | 27 | 设置存档中的 26 个公开字段及日历每周起始日；复合字段的子选项由对应界面继续核对 |
| 平台 | 8 | 本机 Windows、媒体、Steam 启动及扩展内容、成就、游戏账户、账户同步、Steam 云存档、消息与图片 |

相同命令名称允许出现在多行，剧情矩阵按源行号保留全部命令。第 1902 和 1941 行未归入剧情组，保留在待查列表。环境与窗景按原类型名称连接；烟花按 `EnvironmentTypeExtensions` 连接 `Fireworks_First`。

证据列记录来源和有效范围。开场两组剧情、`pomodoroend_11` 的两条原声分支、城市画面、三个环境声的解锁读档、七种语言和本机窗口回归已有局部 Player 证据。窗景参考图来自 Recovery 隔离采集；正常房间切换、粒子和实际光照按[窗景对照](window-references.md)继续验收。平台记录见[验证条件](platform-scenarios.md)，真实服务使用同构建的授权测试档。

城市环境声的扣点、写入失败和重开见[环境声购买](ambient-access.md#环境声购买)。画面与声音的联动及分别恢复见[城市窗景回放](city-window.md#窗景开关与恢复)。

## 复验

五份源目录和十四个源脚本的摘要写入报告。两次独立运行得到同一 SHA-256 `840b5319070dc2c6180b03aac4adad1f254973c72e8bcd6e5a1e508716d6f2f5`。本机报告位于 `assets_private/npr-room-dev/source-lock/feature-matrix.json`。

```powershell
python -B -m tools.npr_room.feature_matrix --source $unitySource --locks $sourceLock --output $featureMatrix
python -B -m unittest tools.npr_room.test_feature_matrix
```
