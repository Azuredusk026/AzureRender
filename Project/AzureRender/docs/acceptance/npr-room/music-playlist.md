# 房间曲库

> 状态为 P3 局部验收。原版内置曲库、播放控制和本地设置通过独立 Windows Player 回放。

曲库按 Recovery `MusicMaster` 的顺序登记 43 首曲目，保留标题、署名、分类、UUID、原音频 GUID 和采样长度。普通房间状态可播放 42 首。手动解锁曲目 Without You 保留来源和解锁标志。

音乐面板可翻页选曲、暂停和继续、前进或回退、单曲循环、随机和静音。回退在当前曲目播放满五秒时从该曲开头开始。播放到末尾后按模式续播。音乐音量初始为 50%，停播时保留同一进程内的采样位置。存档保存启动播放、音量、静音和随机标志。

曲目通过[实时源文件](../../development/asset-source-cache.md#实时源文件)登记并流式读取，音频容器保持原样。首次教学的音乐时序、曲目解锁、收藏、外部文件、环境声与语音按对应功能验收。

## 回放

共享队列任务 1708 使用两个独立 Player。第一个进程暂停时调低音量，继续后切曲，翻页选择第三首，写入静音和停播设置。第二个进程恢复 40% 音量及静音设置，开启播放后回退至曲库末尾。两次进程分别启动四次和两次音源。

使用可控播放时钟的脚本用例覆盖第一帧等待、曲末续播、单曲循环、随机遍历 42 首、五秒回退和存档失败重试。
任务 1709 完成 850 帧房间回归，两次进房各播放一次 Aqua Room，原版计时提示音播放五次，七个角色部件的动作序列为 `[0, 1, 0]`。曲库的资产快照文件为零。

| 证据 | SHA-256 |
| --- | --- |
| 曲库来源报告 | `197c74a64cff10ba1a098375778b510733dcc5718b4aa89d3d6ba5f6b75ad620` |
| 独立 Player 回放 | `95bc14a341294a1629c148306d2d9facbdf199cf9711ebb362cbec0e9db8fa83` |
| 房间回归 | `a58e16e3610aa6adb5eea878a42248194b7758ec30e21e78c4510930fc711519` |
| 曲库画面 | `b24cb00b0fd7893555700acbaf4f746352c90bfa9511437413e9de96cf40acc7` |

本机工程位于 `assets_private/npr-room-dev/room-music-player`，画面位于 `reference-preview/music-playlist.png`。

## 复验

```powershell
python -B -m tools.npr_room.build_music_room --room $progressionRoom --output $musicRoom --source $unitySource --audio-catalog $audioCatalog --audio-probe $nativeAudioReport
python -B tools/npr_room/test_music_room_player.py --player $player --project $musicProject --resource-root $resources --output $musicReplay
python -B tools/npr_room/test_room_startup_player.py --player $player --project $musicProject --resource-root $resources --output $roomReplay --expect-heroine --expect-music
```
