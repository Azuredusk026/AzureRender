# 房间待办

> 状态为 P3 局部验收。多列表、任务编辑、排序、完成、恢复及独立存档重开通过 Player 回放。

房间上的 Tasks 打开待办面板。列表和任务各有稳定标识，可以跨列表切换，编辑标题、任务内容与到期日。工作中的任务保留手动顺序，完成的任务按完成时间从新到旧排列。删除当前列表时回到第一张列表，始终保留至少一张列表。

完成时间使用 Windows 本地时间。任务恢复为待处理时清空完成时间。顺序、文本、日期和完成状态写入 `todo` 槽。日期输入按公历校验，写入失败时保留修改供重试。

Windows 行为依据 Recovery 的 `FacilityTodo`、`TodoListData` 和 `TodoData`。新增、完成、恢复与删除使用原版系统音效。完成提示音的 PCM 保持原样，WAV 容器使用[音频验收](audio-catalog.md)记录的尺寸字段修复。
原版任务列表布局、拖动、完成日期编辑和日历视图按对应的界面及统计验收项实现。

## 独立 Player

共享队列任务 1681 以两个独立 Player 回放。第一轮通过实际表单输入中文，建立两个列表和三个任务，调整顺序并将完成时间写入测试档。第二轮从同一档恢复，重开完成任务，删除任务和列表并验证最后一张列表保留。两轮分别触发五次和三次原音效，退出后缓冲与图像归零。

同一批次通过 850 帧启动、取消、重进及七部件同步动作回归。任务 1678 使用 CoreCLR 9.0.22 验证同一宿主的表单读写、本地时间和原子存档，并回放房间与中文笔记。
本机工程位于 `assets_private/npr-room-dev/room-todo`，移动后通过项目校验。画面位于 `reference-preview/todo.png`。

| 证据 | SHA-256 |
| --- | --- |
| Release Player | `d3b15bf11842b006437df3e11c31f5dfb0e7c76ad5023f30f13a4453bfa61c0f` |
| 待办回放 | `846b2c4c02719cbf060d8350634c39af1072f3c2723db1f2a599aa11ca6b3d86` |
| 启动回归 | `6003c50f976f6790b511ecf03798d29b4ab4723b1f4fb3ecd3573ef06a846ca2` |
| CoreCLR 验证 | `ca1a17f902416a5e45ab0c1c8e8efc217bd3b854a1d651718b50797cbe007860` |

## 复验

```powershell
python -B -m tools.npr_room.build_todo_room --room $splashRoom --source $unitySource --audio-playback $audioPlayback --output $todoRoom
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_todo_player.py --player $player --project $todoRoomProject --resource-root $resources --output $todoReplay
```

## 完成提示音失败隔离

完成和恢复待办各自提交存档并刷新界面。
可选提示音使用公共可恢复播放入口。
提示音损坏时，完成时间和进度仍保持一致。
修复音频后，后续完成操作可继续播放。

队列 2376 使用原创任务和测试音验证两个 Player。
损坏音频期间可完成和恢复同一任务。
声音修复后可再次完成，重开可恢复为未完成。
两个进程无脚本错误，通过 Vulkan 校验及退出释放。
[待办声音隔离结果](evidence/2026-11-21/todo-cue-failure/verification.json)记录输入和状态。
