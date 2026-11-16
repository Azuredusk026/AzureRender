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

## 回放结果

自动输入按房间就绪条件等待。
随后按键经项目动作进入脚本和音频服务。
Release Player 处理 11 个动作，播放五次原音效。
椅子动画运行 346 帧，界面绘制 1,038 次。
运行 360 帧，脚本与表现诊断为空。
退出后缓冲与图像计数均为零。

项目移入私有目录后再次通过 Player 项目校验。
运行记录和产物摘要见[回放清单](room-probe.json)。
计时状态由当前运行持有。
存档持久化和正常交互流程按完整移植计划验收。

## 复验命令

在 AzureRender 根目录执行以下命令。
原资产和输出项目均使用本机受管路径。

```powershell
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/build_room_probe.py --source $unitySource --furniture $importedFurnitureProject --output $roomProbe
python -B tools/npr_room/test_room_probe_player.py --player $player --project $roomProject --output $replayOutput
```

`$importedFurnitureProject` 指向保存后的 Windows 家具项目目录。
真实 Player 回放通过共享 GPU 队列运行。
