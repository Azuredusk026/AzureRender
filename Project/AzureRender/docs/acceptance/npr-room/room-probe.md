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
