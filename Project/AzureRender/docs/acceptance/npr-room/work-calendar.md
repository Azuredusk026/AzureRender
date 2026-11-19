# 工时统计

> 状态为 P3 局部验收。番茄钟与正计时共用按日工时记录，独立 Player 通过结算与重开。

计时的固定步提供有效工作时间，系统本地日期决定归属。暂停与休息期间的计时步不计入工时。
工时在跨日、累计满 60 秒、计时结束和正常退出时写入。存档槽 `work-calendar` 使用按年和日期排序的版本化记录，保存有效工作的秒数。顺序提交前读取当前档，追加尚未落盘的增量。写入失败时待写增量保留供重试。

房间 Statistics 按钮显示今天与累计工时。显示按原 `StatisticsService` 的日上限和整秒口径计算。
累计工时覆盖已落盘的活动回合，退出重开时沿用已有记录。正计时的最近回合独立恢复。
工时奖励与本地等级见[工时奖励与等级](work-progression.md)。周、月与年图表进入相应的专项验收。

## 单日工时修订

在月历选中日期后，可按时、分、秒编辑工时。回车或离开输入框提交。空字段沿用当前值，时数限制为 0 至 24，分和秒限制为 0 至 59。24 小时时分和秒归零。修订写入同一工时档，并保留其他日期、计时待写增量和奖励进度。

编辑行为依据 Recovery 的 `CalendarUI`。当日和累计值使用 `StatisticsService` 的 24 小时日上限和整秒显示。格式保护或写入失败时保留当前工时。

共享队列任务 2116 以三个独立 Player 验证字段输入、关闭日历时提交、24 小时上限、重开和清零。测试保留另一日期的 20 秒和已有等级、经验及点数，较新格式的文件保持原字节。回放报告位于本机 `calendar-work-time-validation/manual-calendar-work-time.json`，SHA-256 为 `211ed9a427b7fd33e746c04285dc96807957a73652ce063474e57e792a068ec1`。

### 工时编辑与计时交错

编辑器以打开或最近一次提交时显示的时、分、秒识别字段变化。字段数值未改动时，失焦会刷新显示，工时沿用计时器的累计精度。

队列 2326 在原创工时面板启动计时，通过鼠标切换输入焦点。失焦时原档字节保持一致，计时暂停并退出后保留 12.967 秒。第二个进程通过输入将工时设为 32 秒，第三个进程读回同一记录。三个进程正常释放图像和缓冲。

报告位于 `20261120-012807-original-work-time-edit-84febd/verified/verification.json`，SHA-256 为 `421dd5f624a9af2e69fc44f338a6a35b36c3aa6fdeec225cd5b660f2a3f57c95`。

## 单日完成任务

月历按选中日期列出已完成待办，显示完成时间和标题。待办完成或恢复后，日历根据当前存档更新。较长标题在列表内换行。日历条目的独立编辑、删除及日期移动按各自入口验收。

任务 2118 在 1280×720 的房间同时打开月历和待办，实测完成、恢复、跨日期切换和重开。回放记录了交给界面的文字和两张画面。两次 Player 正常释放图像和缓冲。本机 `calendar-todo-validation/calendar-completed-todo.json` 的 SHA-256 为 `f86cdcb356409710ac7d6a87a5f73dd37c83aa989808091ba6bd440bf077f449`。

## 单日日记

在选中日期打开日记，回车换行。焦点移出编辑框、切换日期、关闭月历或正常退出时保存文本。各日期使用单独的版本化存档槽。写入失败时保留编辑中的草稿，可以重试或取消。

多行输入和 5,000 个 UTF-16 代码单元的上限来自 Recovery `RoomScene` 的输入框 24713。日期归属及编辑提交依据 `CalendarUI` 与 `FacilityCalendar`。

任务 2120 用三个 Player 进程复验多行输入、按日期保存、关闭进程后恢复及较新格式保护。可控测试覆盖超出长度上限、写入失败、选择日期及取消。报告位于本机 `calendar-diary-validation/calendar-diary-replay.json`，SHA-256 为 `77f71b3f70ac3196d940839a87ec9799e5f19b951fd36f316d866afde1f2f9b8`。

## 习惯与日历记录

月历的习惯面板支持新增、命名、排序、每周启用日、当日暂停或恢复，以及选中日期的完成与撤销。日历保留已删除习惯的完成历史。删除经过面板内确认。

日期启用条件采用 Recovery 的创建日期、星期位标记和暂停区间。已完成日期可撤销，未启用的日期保持未完成。历史记录依据 `HabitDataService.GetCompletedHabitsForCalendar` 与 `CalendarUI` 核对。日期隐藏、周视图和拖动排序分别按原入口继续验收。

任务 2122 使用两个 Player 复验星期规则、暂停恢复、完成撤销、重排、确认删除与重开。图像和缓冲在退出后归零。可控测试覆盖日期边界、暂停区间、写入失败和较新格式保护。本机 `calendar-habit-validation/calendar-habit-replay.json` 的 SHA-256 为 `6c9c139c6083f6c2bcb4b96fafd4fc0747158b558dbe9900467e9992a10be669`。

## 完整房间整合

候选 `room-tools-integrated` 集成工时修订、日记、习惯及计时音效偏好。装配从冻结房间保留角色、道具、剧情和音频。原素材通过共享文件引用，装配记录保存输入与输出摘要。

任务 2125 在 1280×720 核对七语界面和共同存档。任务 2126、2127 将相同候选移至私有目录，以 1920×1080 和 1280×800 再验。六个 Player 进程均正常退出，图像与缓冲计数归零。每种尺寸记录七张语言画面和一张共同编辑画面。

| 尺寸 | 回放报告 SHA-256 |
| --- | --- |
| 1280×720 | `0dd8aebf1cbeed7f684d5ff96e97b2e61e0390330f7883456c268e24b1747af0` |
| 1920×1080 | `327f1129b605e2790d5c650915967957eed02efd79451508293ac887488c0199` |
| 1280×800 | `4537be93efd6247b055ae59b2d055e3aa2310bf557fb32596f6141e2ea273924` |

本机 `room-tools-integration-validation` 保存回放报告、画面与装配收据。以下命令生成新的整合候选。

```powershell
python -B -m tools.npr_room.build_work_calendar --room $frozenRoom --output $calendarRoom --refresh-integrated $storyInventory
python -B -m tools.npr_room.build_timer_sound_room --room $calendarRoom --output $integratedRoom --source $unitySource --inventory $referenceInventory --story-inventory $storyInventory
```

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
