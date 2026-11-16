# 教学与第一章剧情命令

> 状态为 P3 局部验收。命令、原语音引用、章节门槛和分支在脚本层通过回放。房间交互和视听效果使用原生 Player 验收。

教学组 `tutorial_01` 有 90 条命令。第一章 `main_01` 有 20 条命令，等级门槛为 2。两组共使用 25 种命令、33 段原语音和七种语言的原文。命令按原表行序执行，分支目标通过组内命令标识定位。标题、对白、选项、等待、角色动作参数和原语音时长均从资源表读取。

## 剧情控制

脚本管理文本出现前的延迟、手动与自动继续、选项、脚本手势及等待。教学选项保留本轮查看状态，同一选项可再次查看。原指令的延迟传给画面事件执行端。教程使用的脚本继续事件独立于对白翻页。

`room-unlocks` 版本 1 保存已完成剧情组。写入保留同槽环境购买和其他字段。写入失败保留完成状态以便重试。剧情记录回放使用已完成章节，读取过程保留原进度。

## 回放证据

Lua 脚本使用编译后的源组执行两条完整路线。两条路线各覆盖三个教学功能介绍、重复查看、五次显示选项菜单、脚本继续和第一章 10 段对白。最终选项分别到达源行 80 和 83。两条路线均验证等级 1 时第一章锁定，升到等级 2 后可进入，并核对两条完成记录与既有购买。选项文本、原音频时长和源行身份使用实际内容，呈现回调由脚本夹具提供。

本机 `source-lock/room-opening-chapters.json` 的 SHA-256 为 `b40c0dba2fbd32f3014a2e5763decac3994bb01d5bc73ff18ff895fd911de95e`。两次独立编译的摘要一致。原生回放按房间灯光、窗景、道具、角色动作和剧情特效验收。

## 复验

```powershell
python -B -m tools.npr_room.story_chapters --source $unitySource --inventory $storyInventory --audio-catalog $audioCatalog --audio-probe $nativeAudioReport --output $chapters --lua-output $chapterScript
python -B -m unittest tools.npr_room.test_story_chapters
```

Lua 5.4 按顺序加载 `story-progress.lua`、`story-reader.lua`、编译后的剧情程序与 `story-opening-routes.lua`，以同一个程序块执行。
