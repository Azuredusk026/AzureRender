# 计时动作与语音提示点

> 状态为 P3 与 P4 局部验收。16 种计时动作和 113 段原语音进入房间工程。

Unity 2022.3.62f2c1 使用原 Avatar 采样 371 个变换。三个源片段共采样 686 个时刻。七个角色部件的 GLB 共对账 196,196 个局部变换。最大旋转误差为 0.0000057°，最大位移误差为 0.000000104 米，满足 0.1° 和 0.0005 米门槛。

动作状态采用 Unity 导入的片段长度。三个片段的导入长度均比序列化停止时刻多一个原采样间隔。语音使用片段内原 `PlayPomodoroVoice` 提示点。计时界面切换语言时按当前语言显示字幕，并保留播放位置。

## 原生回放

任务 1768 从房间的正常计时入口依次触发开始、进入休息和结束。固定抽样值选择三类原语音的首条台词。1,450 帧内，七个角色部件在每帧的片段与局部时钟一致。原音乐、三次计时提示音和三段语音共七次音源启动，退出时图像和缓冲区归零。

| 计时阶段 | 源片段 | 原提示点（秒） | Player 起声（秒） | 偏差（毫秒） |
| --- | --- | ---: | ---: | ---: |
| 开始 | `anim_wild_banzai` | 1.0888253 | 1.1000000 | 11.175 |
| 休息 | `anim_wild_wet` | 1.2000000 | 1.2000000 | 0.000 |
| 结束 | `anim_wild_stretch_2` | 4.1755114 | 4.1833333 | 7.822 |

无进度音源按原 30 秒等待上限结束当前字幕，Lua 回放覆盖该路径。真实音频设备中断按计时与角色矩阵验收。

任务 1769 在同一套角色资产上完成 850 帧普通房间启动、取消加载和重入。23 次输入、七次音源启动和原两段角色动作的同步顺序均符合预期。

| 证据 | SHA-256 |
| --- | --- |
| 原 Avatar 采样 | `3e4032671cb461eb4d29bbd8f152f59ba318ebb6b8c478b29130dafc239a6411` |
| 变换对账 | `569fcea1da4a7e231bcc0e40ca76f29e6d6798fe60c66d378a842420571b4c6d` |
| 动作装配清单 | `6443f955df17e113a4d9d2e34e293a2bc89fc70c433362d4ac3ded28fc408c53` |
| 原生提示点回放 | `925e885a6d729d00c5ebcba4f6bb7919c562cb4b88de238a089962150266c208` |
| 普通房间回归 | `fff9a6312ca723d64d2b24ca8a31764cb56819491c779cb01c558b9afa7bb931` |

本机回放工程位于 `assets_private/npr-room-dev/room-timer-voice-replay`。采样和变换对账位于 `timer-motion-reference`。动作来源见[计时语音来源与选择](timer-voice-sources.md)。

## 完整动作库

任务 1770 至 1772 按各片段导入的 30 或 60 Hz 采样。动作 67 使用原 Avatar 和三个原片段，在 Unity 动画状态机按两个序列化转场求值。任务 1775 在 60 Hz 采集该动作的 607 个时刻，语音提示 1 与 2 分别位于 5.883333 和 4.283333 秒。

任务 1776 至 1778 装配 16 种计时动作，覆盖 18 个原片段和 3,565 个采样时刻。对账覆盖七个部件的 1,019,590 个局部变换。最大旋转误差为 0.00000576°，最大位移误差为 0.000000104 米。原资产与原始采样以摘要关联，片段时刻误差小于 0.000001 秒。

正式房间工程位于 `assets_private/npr-room-dev/room-full-timer-voice`。装配清单 `timer-motion-receipt.json` 的 SHA-256 为 `e3255859c24c9dd62e843eba84f8c3b7c02991c9223c4c802d1b540ef108d6cc`。Unity 采样工具位于 `tools/npr_room/unity`，独立采样的编辑器入口为 `TimerHumanoidSampler.Run`，三片段转场入口为 `TimerVoiceSequenceSampler.Run`。

正式随机选择使用全部 113 段原语音。任务 1780 在 2,190 帧内回放番茄钟的开始、休息与提前结束。任务 1781 在 2,910 帧内回放正计时的开始、暂停、继续与结束，抽中五分钟内恢复工作的原连续语音。两个进程对照实际打开的原音频摘要、提示点、七部件时钟和退出资源计数。

| 原生回放 | 音源启动次数 | 回放摘要 SHA-256 |
| --- | ---: | --- |
| 番茄钟 | 7 | `09c237bea4495b8402c8f227ee3a97431ebd253fa5fc8a0f78342ea3a458d8fa` |
| 正计时 | 10 | `4ad75441e3c770a8ccc85a886707b2ac2e3363b3d2299cacf40cf9871ce1d330` |

罕见分支继续按原时长与抽样概率验证，每个动作的载入和时序记录使用上面的采样与变换清单。

任务 1783 验证完整动作与语音工程从启动到房间的取消加载和重入。850 帧内两次加载均播放原音乐，七个角色部件同步，退出后缓冲区和图像计数为零。任务 1784 在原计时交互夹具执行同一启动流程，23 次输入、七次音源启动和原角色片段顺序均通过。两次运行分别使用独立存档根。

## 复验

```powershell
python -B -m tools.npr_room.timer_voice_motion --room $room --output $motionRoom --catalog $catalog --samples $samples --rig $rig --motion 55 --motion 54 --motion 69
python -B -m tools.npr_room.timer_voice_player --room $motionRoom --output $cueRoom --catalog $catalog --source $unitySource --cue-replay
python -B tools/npr_room/test_timer_voice_player.py --player $player --project $project --resource-root $resources --output $replay
```

完整动作库使用本机 `timer-motion-reference` 目录的五批采样。以下命令在 AzureRender 根目录运行，输出写入新目录。

```powershell
$sampleArguments = foreach ($batch in 'A','B','C','D') { '--additional-samples'; "$sampleRoot/timer-voice-batch$batch.json.gz" }
$motionArguments = foreach ($motion in 12,50,51,52,53,54,55,56,59,60,61,64,65,67,69,71) { '--motion'; $motion }
python -B -m tools.npr_room.timer_voice_motion --room $room --output $motionRoom --catalog $catalog --samples "$sampleRoot/timer-voice-initial.json.gz" --rig $rig @sampleArguments @motionArguments
python -B -m tools.npr_room.timer_voice_player --room $motionRoom --output $fullRoom --catalog $catalog --source $unitySource
python -B -m tools.npr_room.verify_timer_motion --room $fullRoom --sample-root $sampleRoot --rig $rig --output $audit
python -B tools/npr_room/test_timer_full_player.py --player $player --project "$fullRoom/project.azureproject" --resource-root $resources --catalog $catalog --mode pomodoro --output $pomodoroReplay
python -B tools/npr_room/test_timer_full_player.py --player $player --project "$fullRoom/project.azureproject" --resource-root $resources --catalog $catalog --mode countup --output $countupReplay
```
