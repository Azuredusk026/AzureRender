# 房间笔记

> 状态为 P3 局部验收。独立 Player 通过笔记编辑、重排、删除、写入失败重试及存档重开。

房间界面的 Notes 按钮打开笔记。每页有独立的标题、正文和稳定标识。初始页标题为 New Page。可以新建、前后切换、移动及删除笔记，至少保留一页。
保存按钮即时写入。切换页面、添加页面、移动页面或关闭面板时，先保存当前输入。写入失败时保留输入并显示状态。
Player 正常关闭时，开放的笔记面板提交当前页面。
提交沿用存档版本校验及原子写入。
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

## 便笺退出恢复

原创便笺夹具以固定界面输入两行正文，再触发窗口关闭。
第二个 Player 打开同一档，显示正文并追加文字。
测试随后写入较新存档，正常关闭时核对原字节。
Lua 回归覆盖写入失败时保留已提交页面及输入。

队列 2287 复现关闭后缺少存档。
队列 2288 验证保存、重开、画面变化及原字节保护。
两次退出后活动图像与缓冲均为零。
Vulkan 验证层已加载，两轮均无同步错误。

| 证据 | SHA-256 |
| --- | --- |
| 受检 Release Player | `a3089e8b690a7ebf67fae2eaa64485bb6b2421db4b4f8ad1b6c34346c1020c19` |
| 回放报告 | `577c75f01c493de4365880d4e694d02e80dd8c317f7a5af496acf2070471b7b9` |
| 重开画面 | `984341adba05efc5d5ed2c60eaa54dc26edd2a6eff048bae177e46ce07886c79` |

报告位于 `20261119-235001-original-notes-exit-e13d1b/verified/verification.json`。
其中记录原创夹具、实际着色器及三张画面的摘要。
将 `$resources` 指向受管目录内的着色器资源根。

```powershell
python -B tools/npr_room/test_notes_exit_player.py --player $player --resource-root $resources --output $notesReplay
```

## 运行中内容版本保护

每次文档提交前读取当前内容版本。
较新内容保留原字节，兼容后可沿原面板保存输入。
保护覆盖启动时和运行中安装的较新文档。

队列 2377 使用笔记与待办的四个原生 Player。
内容版本 2 和 3 均得到保护。
同进程恢复后保存草稿，重开后可继续操作。
四次退出通过 Vulkan 校验，活动图像和缓冲归零。
[工具版本报告](evidence/2026-11-21/room-tool-payload/verification.json)记录输入、版本和结果。
