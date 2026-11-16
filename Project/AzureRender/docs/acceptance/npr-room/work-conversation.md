# 工后对话与选项

> 状态为 P3 局部验收。工后短对话的触发、原声音频、组内选项与读档续接通过独立 Windows Player 回放。

20 段 `ShortConversation` 含 80 条带原语音的文本、60 个选项及 60 条结束命令。每个选项按 `ScenarioReader` 的组内查找规则定位回应及下一条结束命令。来源报告登记源表行、七种语言文本、原音频身份和时序。

结算实际工作时长后，对话可用二十分钟。触发概率按工作时长线性增加，五分钟达到 25%。成功触发后使用该次机会。停留时间取自源文本命令。

本地档以剧情组、当前阶段和选项编号记录进度。进程重开可继续选择，或从当前语音开头继续。完成原分支的结束命令后清除活动记录。

## 回放

共享队列任务 1719 使用固定随机种子的本机回放工程。第一个 Player 完成真实计时输入，触发 `pomodoroend_11`，在三个选项出现时退出。两个独立 Player 从同一进度的隔离副本分别选择第 2 和第 3 项，按来源身份播放各自的语音，抵达原表第 1480 和 1482 行后清除进度。

可控播放时钟覆盖全部 60 个分支、触发概率和结束写入。任务 1721 使用常规随机配置的项目完成 850 帧房间回归，包含两次音乐启动、五次计时提示音及七个角色部件的同步动作。

| 证据 | SHA-256 |
| --- | --- |
| 常规配置来源报告 | `7f5b443ec1db620d1a7689afe793988f08e488ef738f97779c535019f45c2d2d` |
| 独立 Player 回放 | `cc0cdce456075be533414cfc4923cdc7dce351c16ac32250250b1a0a013c6eb6` |
| 房间回归 | `463b07a5875e6d58d58ca0043489a0d92a2126e151636fffc664454420bae55c` |
| 对话选项画面 | `9cc83e83b47e679f4ce10f8bf182c3849b756f8b70b00f7b1df8cc979b1e4ed3` |

本机工程位于 `assets_private/npr-room-dev/room-choice`。回放工程为同级的 `room-choice-replay`，画面位于 `reference-preview/work-conversation.png`。

## 复验

```powershell
python -B -m tools.npr_room.build_choice_room --room $voiceRoom --output $choiceRoom --source $unitySource --inventory $storyInventory --audio-catalog $audioCatalog --audio-probe $nativeAudioReport
python -B -m tools.npr_room.build_choice_room --room $voiceRoom --output $choiceFixture --source $unitySource --inventory $storyInventory --audio-catalog $audioCatalog --audio-probe $nativeAudioReport --seed 5144
python -B tools/npr_room/test_choice_room_player.py --player $player --project $fixtureProject --resource-root $resources --output $choiceReplay
python -B tools/npr_room/test_room_startup_player.py --player $player --project $choiceProject --resource-root $resources --output $roomReplay --expect-heroine --expect-music
```
