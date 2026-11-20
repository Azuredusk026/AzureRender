# 房间计时回放

> 状态为 P1 局部验收。启动、计时、椅子片段和原音效通过独立 Player 回放。

## 应用入口

`Boot` 脚本通过生产关卡请求进入房间。
当前房间夹具装配原桌椅和椅子的开窗片段。
Lua 脚本通过项目动作管理番茄钟。
界面显示当前阶段和剩余时间。

默认值采用 Recovery 的 25 分钟工作与 5 分钟休息。
默认工作轮数为二。
开始、暂停和结束音效依据 `TimerSeMaster` 选择。
开始与结束音量为 0.5，暂停音量为 1。

| 输入 | 当前动作 |
| --- | --- |
| T | 开始、暂停或恢复 |
| K | 跳过当前阶段 |
| R | 结束当前计时 |
| W / X | 增加或减少工作分钟数 |
| B / N | 增加或减少休息分钟数 |
| C / V | 增加或减少工作轮数 |

设置操作在空闲状态生效。工作和休息分钟数范围为 1 至 999，轮数范围为 1 至 99。

## 回放结果

自动输入按房间就绪条件等待。
随后按键经项目动作进入脚本和音频服务。
Release Player 处理 11 个动作，播放五次原音效。
椅子动画运行 346 帧，界面绘制 1,038 次。
运行 360 帧，脚本与表现诊断为空。
退出后缓冲与图像计数均为零。

项目移入私有目录后再次通过 Player 项目校验。
运行记录和产物摘要见[回放清单](room-probe.json)。

## 偏好存档

独立 Player 进程经实际键盘路由设置工作 26 分钟、休息 6 分钟、三轮工作并开始计时。
此次运行消费 13 个输入事件，播放六次原音效。
第二个 Player 从同一目录读取并校验三项设置，以空闲状态进入房间。
活动回合随进程结束，两个进程退出后均释放缓冲和图像。
存档单测覆盖项目隔离、并发提交、损坏回退和较新版本保护。

本机夹具位于 `assets_private/npr-room-dev/room-saved-probe`。
其中 `validation` 保存验收用 Release Player 和回放记录。
该 Player 使用工作树的存档源码与本机 Release 构建库链接。
整套工作树构建和项目发布按对应验收项执行。

### 偏好写入与模式恢复

房间在保存成功后应用新的计时偏好和模式。写入被拒绝时保留当前值，计时器可继续开始和结束。

共享队列任务 2108 运行三个独立 Player，覆盖两份较新格式的存档保护、空白存档写入三项计时偏好及模式、重开后恢复正计时。受保护的文件字节保持一致，三次运行退出后缓冲与图像计数均为零。

本机 `timer-preferences-validation/timer-preferences-replay.json` 的 SHA-256 为 `39b753505833edca9023de012dfbf23fe33ae6e15716823a9b4fc43691f9ff09`。同目录保存回放动作和执行记录。

### 计时存档内容版本

Lua 在写入计时偏好、模式和检查点前读取当前内容版本。
较新内容保留原始字节。检查点的待写入记录沿用重试流程。
项目存档服务校验外层文件版本。

队列 2305 复现三种较新内容版本被覆盖。
队列 2307 验证原字节保护、兼容格式重试和重开。
两秒暂停进度在重开后记入最近回合。
四个进程退出后的活动图像和缓冲均为零。

报告位于 `20261120-005557-original-timer-schema-guard-3f38c4/verified/verification.json`。
报告 SHA-256 为 `62acf262e504000bd69801af404e5e1452d6e64fc5679f7c3afdb2f64bd63bf4`。

### 计时音效资源

计时音效装配在已有房间候选上加入开始 B、开始 C、结束 B 和结束 C。四个槽保留原 `AudioClip` GUID 与文件 ID。原版开始 A、结束 A 和暂停沿用固定槽名。房间装配核验覆盖全部七个计时音效及三个系统音效。

任务 2110 通过按键依次播放四个新增槽。Player 成功解码四个原 OGG 文件，记录四次播放和九个动作，退出后缓冲与图像计数均为零。该回放直接触发资源槽。

本机 `timer-sound-variant-validation/sound-cue-mounts.json` 与 `timer-sound-variant-validation/timer-sound-playback.json` 的 SHA-256 分别为 `17cae30dee2f3f24d9cf3b1ce59f70da168150cb473fcc21a060d8101b249978` 与 `fad337fc608306dda6eefec81a7c293ddce1cb6804d19a2c3b5ced037f04793c`。

```powershell
python -B -m tools.npr_room.build_timer_sound_room --room $roomDirectory --source $unitySource --inventory $referenceInventory --output $roomWithTimerSounds
python -B -m tools.npr_room.sound_cue_inventory --source $unitySource --inventory $referenceInventory --room $roomWithTimerSounds --audio-repairs $audioRepairReport --output $soundCueReport --timer-variants
```

### 计时音效偏好

番茄钟与正计时各自保存开始和结束的 A、B、C 音效、百分比音量和静音。选择音效或提交音量时试听。字段可通过回车或失去焦点提交。选项写入成功后生效，较新格式及写入失败时保留当前选项。

任务 2112 覆盖两种模式独立保存、切换标签页时提交音量、静音及重开。任务 2114 从完整房间打开七种语言的音效面板，在 1280×720 下逐一记录画面，并通过面板及计时按钮播放原音效。各进程均正常释放缓冲与图像。主音量及原版滑杆交互按设置专项复验。

本机 `timer-sound-settings-validation/timer-sound-settings-replay.json` 与 `timer-sound-settings-validation/full-room-timer-settings.json` 的 SHA-256 分别为 `9e00f59e10007817f6f56e44ccb0b1d73c85a7c1ec39efb0eacac71fd169eff0` 与 `71c413cf31d5505f1aaf7ddb7869a0137b2daf3937d52ef97ea809b485974378`。

```powershell
python -B -m tools.npr_room.build_timer_sound_room --room $roomDirectory --source $unitySource --inventory $referenceInventory --story-inventory $storyInventory --output $timerSettingsRoom
```

## 托管存档

通过固定版本的 CoreCLR 9.0.22，两个独立 Release Player 各运行 24 帧。
托管脚本读取计数并将递增值分别写回存档和场景节点。
两个运行报告记录的节点横坐标分别为 1 和 2，最终存档值为 2。
两个进程均无脚本错误，退出后缓冲与图像计数均为零。
本机 `assets_private/npr-room-dev/managed-save-probe` 保存托管项目、运行时来源校验、样例源码和运行记录。

## 计时与角色动作

集成工程在原房间和主相机下装配七个角色部件。
夹具将报告书写设为工作动作，计时开始时同步切换七个部件，计时结束时回到待机。
两段动作使用原 Avatar 的逐帧数据。 [人体动画采样](character-assets.md#人体动画采样)

共享队列任务 1646 回放 850 帧及 23 个输入事件，完成取消、进入、计时、结束及重进。
七个角色部件在每个有效回放帧的片段索引和播放时间一致，依次经过待机、报告书写和待机。
运行触发五次原音效，脚本与表现诊断为空，退出后缓冲和图像计数均为零。
本机工程位于 `assets_private/npr-room-dev/room-heroine`，移动后通过项目校验。
回放报告摘要为 `68d175e382d0dd2f7bed08f593c0782205f16f6c05e139cb2a7389cb90f8215a`。
原版动作选择、语音时序、道具和窗景联动按相应验收路径实现。

## 复验命令

在 AzureRender 根目录执行以下命令。
原资产和输出项目均使用本机受管路径。

```powershell
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/build_room_probe.py --source $unitySource --furniture $importedFurnitureProject --audio-playback $audioPlayback --output $roomProbe
python -B tools/npr_room/test_room_probe_player.py --player $player --project $roomProject --output $replayOutput
python -B tools/npr_room/test_room_save_player.py --player $player --project $roomProject --output $saveReplayOutput --resource-root $releaseBuild
python -B tools/npr_room/build_room_probe.py --source $unitySource --furniture $importedFurnitureProject --audio-playback $audioPlayback --room-geometry $importedRoomProject --heroine-motion $heroineMotionProject --startup-flow --output $heroineRoom
python -B tools/npr_room/test_room_startup_player.py --player $player --project $heroineRoomProject --output $heroineReplay --resource-root $releaseBuild --expect-heroine
```

`$importedFurnitureProject` 指向保存后的 Windows 家具项目目录。
真实 Player 回放通过共享 GPU 队列运行。
夹具的 Player 使用 Release 构建与 vcpkg 的 DLL 搜索路径。
交互启动通过 `--save-root` 选择 D 盘独立存档目录。

## 提示音失败隔离

计时状态与提示音播放分别处理结果。
提示音不可用时，计时继续处理暂停、继续和结算。
提示音文件恢复后，后续操作重新尝试播放。

队列 2369 使用生产计时脚本和截断的原创测试音。
暂停时保存两秒检查点，恢复声音后结算四秒。
第二个 Player 读回相同记录。
两个进程无脚本错误，通过 Vulkan 校验及退出释放。
[提示音失败证据](evidence/2026-11-21/timer-cue-failure/verification.json)记录计时与播放结果。

## 音效试听失败恢复

停止和暂停操作保持未加载音源的无声状态。
加载诊断继续保留，显式播放承担资源重试。
设置试听失败时显示错误，已保存的选项保持有效。
资源恢复后，可点击同一选项重新试听。

队列 2370 的公共测试复现未加载音源停止时抛出异常。
队列 2371 构建通过，两个公共呈现测试通过。
队列 2372 以两个原生 Player 核对设置面板和重开。
损坏音频期间选项 B 成功写档，恢复后同选项播放一次。
两个进程无脚本错误，通过 Vulkan 校验和退出释放。
[公共传输结果](evidence/2026-11-21/timer-preview-recovery/verification.json)与[试听结果](evidence/2026-11-21/timer-preview-recovery/player-verification.json)保留来源。
