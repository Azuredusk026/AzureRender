# 教学与第一章剧情命令

> 状态为 P3 局部验收。命令、原语音引用、章节门槛和分支在脚本层通过回放。原语音播放和字幕切换在原生 Player 中通过定向回放。

教学组 `tutorial_01` 有 90 条命令。第一章 `main_01` 有 20 条命令，等级门槛为 2。两组共使用 25 种命令、33 段原语音和七种语言的原文。命令按原表行序执行，分支目标通过组内命令标识定位。标题、对白、选项、等待、角色动作参数和原语音时长均从资源表读取。

## 剧情控制

脚本管理文本出现前的延迟、手动与自动继续、选项、脚本手势及等待。教学选项保留本轮查看状态，同一选项可再次查看。原指令的延迟传给画面事件执行端。教程使用的脚本继续事件独立于对白翻页。

`room-unlocks` 版本 1 保存已完成剧情组。写入保留同槽环境购买和其他字段。写入失败保留完成状态以便重试。剧情记录回放使用已完成章节，读取过程保留原进度。

## 回放证据

Lua 脚本使用编译后的源组执行两条完整路线。两条路线各覆盖三个教学功能介绍、重复查看、五次显示选项菜单、脚本继续和第一章 10 段对白。最终选项分别到达源行 80 和 83。两条路线均验证等级 1 时第一章锁定，升到等级 2 后可进入，并核对两条完成记录与既有购买。选项文本、原音频时长和源行身份使用实际内容，呈现回调由脚本夹具提供。

本机 `source-lock/room-opening-chapters.json` 的 SHA-256 为 `b40c0dba2fbd32f3014a2e5763decac3994bb01d5bc73ff18ff895fd911de95e`。两次独立编译的摘要一致。原生回放按房间灯光、窗景、道具、角色动作和剧情特效验收。

## 语音与字幕

本机 `room-opening-voices-verified` 挂载 33 段原始语音，逐段核对 SHA-256。字幕按原 `TalkSpeedData` 读取七种语言的基础字符间隔。日语、简体中文和繁体中文为 0.13 秒，英语、葡萄牙语和俄语为 0.02 秒，韩语为 0.05 秒。快速显示的基础间隔为 0.01 秒。

原生任务 1810 通过房间内按钮分别触发教学源行 30 和第一章源行 96，日志记录两段对应的原 OGG 文件。由简体中文切换到俄语时，语音启动计数保持为 1，语言设置写入测试档。字幕框内有 4,123 个像素改变，进入第一章样本时有 4,286 个像素改变。图片和 `dialogue-replay.json` 位于挂载目录的 `validation/`。任务 1811 使用两个独立 Player，回归了多行笔记、排序、删除、重开和未来版本保护。

## 复验

```powershell
python -B -m tools.npr_room.story_chapters --source $unitySource --inventory $storyInventory --audio-catalog $audioCatalog --audio-probe $nativeAudioReport --output $chapters --lua-output $chapterScript
python -B -m unittest tools.npr_room.test_story_chapters
python -B -m tools.npr_room.story_dialogue_room --room $bodyRoom --output $dialogueRoom --source $unitySource --chapters $chapters
python -B tools/npr_room/test_story_dialogue_player.py --player $player --room $dialogueRoom --resource-root $runtimeResources --chapters $chapters --output $replay
```

Lua 5.4 按顺序加载 `story-progress.lua`、`story-reader.lua`、编译后的剧情程序与 `story-opening-routes.lua`，以同一个程序块执行。
