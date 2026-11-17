# 房间语言设置

> 状态为 P3 局部验收。计时器文字、空闲字幕和工后对话支持运行中切换七种语言。

语言标识与 Recovery Windows 的 `GameLanguageType` 一致。实际房间提供日语、英语、简体中文、繁体中文、葡萄牙语、韩语和俄语。候选工程以简体中文启动，读取独立档中的已保存语言。

13 个计时及语言设置文本、23 段空闲语音的字幕和 20 组工后对话均取原文本表。工后对话含 80 句带语音的台词及 60 个选项。切换时重新显示当前字幕或选项，并保持同一音源的播放位置。房间装配简体中文、日本、繁体中文和韩文字体。原韩文字体覆盖本范围所需的 277 个韩文音节。

[显示设置语言](platform-scenarios.md#显示设置语言)登记图形设置和 Windows 菜单使用的九条原文本、七种菜单标题和俄语面板实机画面。

界面其他文字、完整窗口样式以及操作系统默认语言选择纳入完整 P3 验收。

## 本地存档

`room-language` 保存语言标识及版本。保存成功后应用新语言，重开读取同一标识。Windows 存档按规范化后的本机路径创建临时文件，支持深目录中的读取、原子替换和失败回收。项目目录身份与槽位格式按[项目存档](../../runtime/script-services.md#项目存档)管理。

## 回放

任务 1750 以两个独立 Player 核对七次语言切换，记录标识顺序为 1、4、5、6、3、2、7。播放中切换保持同一段原语音。重开恢复俄语，从工后对话 `pomodoroend_11` 的选项存档续接并播放第二项的原回复。韩语和俄语画面保留实机截图。

任务 1748 通过 Windows 深路径和原有存档回归。任务 1751 完成 850 帧房间启动、取消加载和重入，核对 23 次输入、七次音源启动和七个角色部件同步。任务 1753 通过中文笔记、顺序调整、重开及拒绝较新存档版本。任务 1754 通过待办重开、列表操作和完成音效回归。

| 证据 | SHA-256 |
| --- | --- |
| 语言、文本及字体清单 | `fc8ab04b7ede1c6bb62285d042aabbad93b0c4369d40606d5f793717e961672d` |
| 实机语言回放 | `a00cca56c6d0be69e8e26d6bfbe068f3da93c8e4d441bcb95e52490d1b28b6e9` |
| 韩语界面画面 | `68ed9169ac886124f19894d7246a4af4a2d7c1ff6a4e179df658ea5bd70e0dea` |
| 俄语对话画面 | `b00a7a74a7c117e0e1518493e84fe93bfe7c91e0cf5ef8e20f15bf94c740a6aa` |
| 房间启动回归 | `aa83fc58f48d9157409a6f521a73c99d36671afe35096d38c5243f96f084832c` |
| 笔记回归 | `9983f3d735b24e976dbc908ddd822c092ca33fe3135d14a43efb320516e4f3ee` |
| 待办回归 | `298269f51418117c8da1fb88d99713319dca98fd5c296b4cc5b9bf83753700bb` |

本机工程位于 `assets_private/npr-room-dev/room-localized-fonts`，Player、构建记录和回放位于其 `validation` 目录。

## 复验

```powershell
python -B -m tools.npr_room.room_language --room $room --source $unitySource --inventory $storyInventory --images $imageCatalog --output $localizedRoom
python -B tools/npr_room/test_room_language_player.py --player $player --project $project --resource-root $resources --output $replay
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaLibrary
ctest --test-dir $buildDir -R '^AzureEngine\.ProjectSaveStore$' --output-on-failure
```
