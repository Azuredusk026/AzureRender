# 工时奖励与等级

> 状态为 P3 局部验收。本地普通模式的工时奖励、等级与退出恢复通过独立 Player 回放。

经验和积分按 Recovery `GameParameterMaster` 的参数计算。原参数每分钟提供十点经验与十点积分，单次计算上限为十二小时。经验保留小数，积分截为整数。首级经验至少达到一次升级需要的数值，等级按原表的 37 项阈值和后续默认阈值递进。

本地夹具使用主线第 31 章完成前的状态，经验按原规则累计至 32 级。工时统计界面显示本地等级和累计积分。[环境声面板](ambient-access.md#环境声购买)按已提交的购买记录显示可用余额。主线进度、合作模式和平台成就按对应场景验收。

版本 2 的 `work-calendar` 槽将工时、等级、经验、积分和活动回合秒数一起原子提交。版本 1 的记录在首次提交时保留日工时。重开将已写入的活动回合结算一次，随后清空待结算秒数。

## 回放

共享队列任务 1685 使用四个独立 Player。带有十秒历史工时的测试档在结束番茄钟后升至二级。关闭活动正计时后重开，工时保留，经验结算为约 0.594444。第四个进程打开同一档后，存档字节保持一致。
可控时钟与数据测试覆盖旧档、写入失败重试、正计时恢复、积分计算和 32 级上限。同批次通过 850 帧启动及房间回归。

| 证据 | SHA-256 |
| --- | --- |
| 回放报告 | `442640450ee3c12eaca72c0042d8c891ad4b8eeae6f4c5e38f55e3db608624a4` |
| 房间回归 | `d5dc63ff812adf53a031a210b7496aab60a71931dc671cac6d7a06162aa5f9c0` |
| 工时与等级画面 | `d1854dc020644ea3240fea212765b227b3346dbf9ec76abd85da6abf83b3565b` |

本机工程位于 `assets_private/npr-room-dev/room-work-progression`，画面位于 `reference-preview/work-progression.png`。

## 日历每周起始日

Recovery 的 `AllPlatformSettingData.FirstDayInWeek` 默认采用星期日。`SettingUI` 提供七天选项，`CalenderCoreUI` 按所选星期排列列标题，并按当月一日所在星期填充日期。

房间统计面板以七列显示当月日期。每周起始日写入版本为 1 的 `calendar-first-day` 存档。写入成功后更新列表，写入失败时保留当前排列。重开读取已保存的选项，较新版本的存档保留原字节。工时统计以日期为键。

Lua 测试覆盖七种起始日、闰年二月、跨年月、写入失败及重开。共享队列任务 2097 使用 RTX 4060 上的两个独立 Player，通过原生界面分别选择星期一和星期六，核对两次存档与退出释放结果。诊断关卡通过正式计时脚本和界面运行。完整房间的布局和语言按房间场景继续验收。

本机 `work-calendar-first-day/calendar-first-day-replay.json` 的 SHA-256 为 `63c0685761850ab4d5e5f5e0f1fd8070f938b0c4cd38df3651f87f3e7dc66eab`，同目录保存回放脚本和退出日志。

## 日历月份与日工时

统计面板打开时显示本地当前月份及当天工时。月份按钮按月翻页，在年份边界遵循 Recovery `CalenderCoreUI` 的 2025 年 1 月至 2998 年 12 月范围。月份使用实际天数，七列布局延续已保存的起始日。

点击日期读取同一 `work-calendar` 槽中该日的工时。翻页保留选中日期，选中当天时计时进度随帧更新。浏览和选择使用会话内状态。Lua 回归覆盖跨年翻页、闰月、空白日期、在途工时、起始日变更和月份边界。

任务 2098 通过原生 Player 界面从当天的 18 秒切换到上月末的 61 秒，再返回当天。两次翻页和日期选择均由界面按钮触发。回放后工时存档字节一致，退出时 GPU 缓冲和图像计数归零。本机 `work-calendar-history/calendar-history-native.json` 的 SHA-256 为 `3d2cf588da6d51cc88988c9080fb49db3f5ea5f4905d979fca2d42c41b15fdd2`。

## 复验

```powershell
python -B -m tools.npr_room.build_work_calendar --room $todoRoom --source $unitySource --output $progressionRoom
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_work_calendar_player.py --player $player --project $progressionProject --resource-root $resources --output $replay --expect-progression
```
