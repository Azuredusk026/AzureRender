# 计时动作与语音提示点

> 状态为 P3 与 P4 局部验收。番茄钟开始、进入休息和结束的三个原动作通过原生房间回放。

Unity 2022.3.62f2c1 使用原 Avatar 采样 371 个变换。三个源片段共采样 686 个时刻。七个角色部件的 GLB 共对账 196,196 个局部变换。最大旋转误差为 0.0000057°，最大位移误差为 0.000000104 米，满足 0.1° 和 0.0005 米门槛。

动作状态采用 Unity 导入的片段长度。三个片段的导入长度均比序列化停止时刻多一个原采样间隔。语音使用片段内原 `PlayPomodoroVoice` 提示点。计时界面切换语言时按当前语言显示字幕，并保留播放位置。

## 原生回放

任务 1768 从房间的正常计时入口依次触发开始、进入休息和结束。固定抽样值选择三类原语音的首条台词。1,450 帧内，七个角色部件在每帧的片段与局部时钟一致。原音乐、三次计时提示音和三段语音共七次音源启动，退出时图像和缓冲区归零。

| 计时阶段 | 源片段 | 原提示点（秒） | Player 起声（秒） | 偏差（毫秒） |
| --- | --- | ---: | ---: | ---: |
| 开始 | `anim_wild_banzai` | 1.0888253 | 1.1000000 | 11.175 |
| 休息 | `anim_wild_wet` | 1.2000000 | 1.2000000 | 0.000 |
| 结束 | `anim_wild_stretch_2` | 4.1755114 | 4.1833333 | 7.822 |

无进度音源按原 30 秒等待上限结束当前字幕，Lua 回放覆盖该路径。其余原动作、正计时路线和真实音频设备中断继续按计时与角色矩阵验收。

任务 1769 在同一套角色资产上完成 850 帧普通房间启动、取消加载和重入。23 次输入、七次音源启动和原两段角色动作的同步顺序均符合预期。

| 证据 | SHA-256 |
| --- | --- |
| 原 Avatar 采样 | `3e4032671cb461eb4d29bbd8f152f59ba318ebb6b8c478b29130dafc239a6411` |
| 变换对账 | `569fcea1da4a7e231bcc0e40ca76f29e6d6798fe60c66d378a842420571b4c6d` |
| 动作装配清单 | `6443f955df17e113a4d9d2e34e293a2bc89fc70c433362d4ac3ded28fc408c53` |
| 原生提示点回放 | `925e885a6d729d00c5ebcba4f6bb7919c562cb4b88de238a089962150266c208` |
| 普通房间回归 | `fff9a6312ca723d64d2b24ca8a31764cb56819491c779cb01c558b9afa7bb931` |

本机回放工程位于 `assets_private/npr-room-dev/room-timer-voice-replay`。采样和变换对账位于 `timer-motion-reference`。动作来源见[计时语音来源与选择](timer-voice-sources.md)。

## 复验

```powershell
python -B -m tools.npr_room.timer_voice_motion --room $room --output $motionRoom --catalog $catalog --samples $samples --rig $rig --motion 55 --motion 54 --motion 69
python -B -m tools.npr_room.timer_voice_player --room $motionRoom --output $cueRoom --catalog $catalog --source $unitySource --cue-replay
python -B tools/npr_room/test_timer_voice_player.py --player $player --project $project --resource-root $resources --output $replay
```
