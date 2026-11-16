# 工时统计

> 状态为 P3 局部验收。番茄钟与正计时共用按日工时记录，独立 Player 通过结算与重开。

计时的固定步提供有效工作时间，系统本地日期决定归属。暂停与休息期间的计时步不计入工时。
工时在跨日、累计满 60 秒、计时结束和正常退出时写入。存档槽 `work-calendar` 使用按年和日期排序的版本化记录，保存有效工作的秒数。顺序提交前读取当前档，追加尚未落盘的增量。写入失败时待写增量保留供重试。

房间 Statistics 按钮显示今天与累计工时。显示按原 `StatisticsService` 的日上限和整秒口径计算。
累计工时覆盖已落盘的活动回合，退出重开时沿用已有记录。正计时的最近回合独立恢复。
工时奖励与本地等级见[工时奖励与等级](work-progression.md)。周、月与年图表、手动修订及完成任务日历进入相应的专项验收。

## 回放

可控时钟覆盖跨午夜、暂停、休息、60 秒检查点、写入失败、较新版本和退出恢复。
原计时工作日归属依据 Recovery `TimerCoreService.UpdateCalendarWorkTime`，统计口径依据 `StatisticsService`。

共享队列任务 1683 使用三个独立进程。番茄钟累计 3.3 秒，随后正计时活动回合关闭，重开恢复三秒最近回合。统计槽保留约 6.867 秒，重开过程保持存档原字节。1280×720 画面验证统计与待办同时打开。
同批次还执行 850 帧房间回归及待办音效和重开。

| 证据 | SHA-256 |
| --- | --- |
| 工时回放 | `c37463024e9e275d4ba74ff86f6aca2b3bdfd060d1c2dc00903861900154c42c` |
| 工时与待办画面 | `afab2e53381683e37d84f6fee6243eafb7211687576b3085f71582bdb684e3e0` |
| 启动回归 | `bcafb6c4ff79ab71b47083edebc002c902fe8a5841573e490220bc2264cb6188` |

本机工程位于 `assets_private/npr-room-dev/room-work-calendar`，画面位于 `reference-preview/work-calendar.png`。

## 复验

```powershell
python -B -m tools.npr_room.build_work_calendar --room $todoRoom --output $workCalendarRoom
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_work_calendar_player.py --player $player --project $workCalendarProject --resource-root $resources --output $workReplay
```
