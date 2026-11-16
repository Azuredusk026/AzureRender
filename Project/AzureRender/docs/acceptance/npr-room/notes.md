# 房间笔记

> 状态为 P3 局部验收。独立 Player 通过笔记编辑、重排、删除、写入失败重试及存档重开。

房间界面的 Notes 按钮打开笔记。每页有独立的标题、正文和稳定标识。初始页标题为 New Page。可以新建、前后切换、移动及删除笔记，至少保留一页。
保存按钮即时写入。切换页面、添加页面、移动页面或关闭面板时，先保存当前输入。写入失败时保留输入并显示状态。
行为依据 Recovery 的 `NoteService`、`NoteList` 和 `NoteUIContentsView`。

笔记的版本、下一页标识和有序页面共同写入项目存档槽 `notes`。排序、删除和编辑使用同一原子提交流程。
损坏的笔记内容回退到初始页。较新版本的文件在写入失败后保留原字节。
字体来自原版简体中文 IBM Plex Sans SC，并随本机工程加载。

## Player 回放

共享队列任务 1659 使用两个独立的 Release Player。第一轮以 RmlUi 输入标题 `项目记录` 和带换行、尖括号的正文，通过存档及 1280×720 画面确认输入。随后添加第二页、把顺序调整为 2、1，再删除第一页。
第二轮打开同一存档，编辑保留页，验证较新版本文件拒绝写入，恢复可写版本后重试。尝试删除最后一页时存档保留该页。两轮退出均释放全部缓冲与图像。

| 证据 | SHA-256 |
| --- | --- |
| Release Player | `21476ab1cbb2a10dc5c0f44278151990a18caa5ccb47c0a58fa033295a21da69` |
| 回放报告 | `9983f3d735b24e976dbc908ddd822c092ca33fe3135d14a43efb320516e4f3ee` |
| 中文笔记画面 | `95f50a02931f5f245cf4a3c0bfef194cce5c9488c04f432c6d66b772a8582153` |
| 房间回归报告 | `eaead870613d06d185fb4033d048730c5407644950b7d266e6c6cac6d0d5b617` |

本机工程位于 `assets_private/npr-room-dev/room-notes`，画面位于 `reference-preview/notes.png`。
工程移动后通过项目校验。共享队列任务 1660 使用同一候选通过 850 帧启动、取消及重进回放，处理 23 次输入与五次音效，七个角色部件完成写作与待机切换。
原版的面板布局、拖动交互、文本组合输入及三档界面仍在专项验收范围内。

## 复验

```powershell
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_notes_player.py --player $player --project $roomProject --resource-root $releaseBuild --output $notesReplay
```
