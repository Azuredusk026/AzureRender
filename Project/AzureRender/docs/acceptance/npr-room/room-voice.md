# 角色空闲语音

> 状态为 P3 局部验收。空闲单句回应的原声音频、简体中文字幕和结束时序通过独立 Windows Player 回放。

原版 `ReactionTalkSelector` 使用 23 个普通空闲回应。每条回应以源剧情组和 `LocalizationMaster` 身份定位语音，在来源报告登记语言文本、原音频 GUID、采样长度和表情参数。编排工具可选七种源语言，房间验收使用简体中文。

计时处于空闲时可从房间的对话按钮触发回应。选择池每轮使用各回应一次，换轮时避开上一条。播放期间的重复输入保持当前回应。字幕与原语音同时开始，播完后按原参数停留一秒。

角色点选、工作与休息阶段的时段回应、嘴形与表情同步按各自功能验收。

## 回放

共享队列任务 1716 使用两个不同的原版语音，确认播放期间重复点击只触发一次，字幕收起后可触发下一条。Player 共启动一个背景音乐音源及两个语音音源。脚本时钟用例遍历全部 23 条回应，并检查停留时间和换轮顺序。

任务 1717 完成 850 帧整合回归，两次进房正常启动音乐，原版提示音播放五次，角色动作序列为 `[0, 1, 0]`。

| 证据 | SHA-256 |
| --- | --- |
| 语音来源报告 | `ae7bffd6da3af1e81226a7e1dc01d666c91f44a439d438fbc9ef21cb718a5e70` |
| 独立 Player 回放 | `e4ef743e3c48e44952c3aba2f9b2b870ed4daee51e7a26ca089c4ec752392551` |
| 房间回归 | `b22836c06241b69c0954995714437d6e4c22a1bb7cadd1a4a5860c349f5ee5e1` |
| 字幕画面 | `86d3da52c539d2bae6a79a8dd3bc48de6dc01801aa0f4f8e803d2b6ab75c6a84` |

本机工程位于 `assets_private/npr-room-dev/room-voice`，画面位于 `reference-preview/room-voice.png`。

## 复验

```powershell
python -B -m tools.npr_room.build_voice_room --room $ambientRoom --output $voiceRoom --source $unitySource --inventory $storyInventory --audio-catalog $audioCatalog --audio-probe $nativeAudioReport --language ZhHans
python -B tools/npr_room/test_room_voice_player.py --player $player --project $voiceProject --resource-root $resources --output $voiceReplay
python -B tools/npr_room/test_room_startup_player.py --player $player --project $voiceProject --resource-root $resources --output $roomReplay --expect-heroine --expect-music
```
