# 房间曲库

> 状态为 P3 局部验收。原版内置曲库、播放控制和本地设置通过独立 Windows Player 回放。

曲库按 Recovery `MusicMaster` 的顺序登记 43 首曲目，保留标题、署名、分类、UUID、原音频 GUID 和采样长度。普通房间状态可播放 42 首。手动解锁曲目 Without You 保留来源和解锁标志。

音乐面板可翻页选曲、暂停和继续、前进或回退、单曲循环、随机和静音。回退在当前曲目播放满五秒时从该曲开头开始。播放到末尾后按模式续播。音乐音量初始为 50%，停播时保留同一进程内的采样位置。存档保存启动播放、音量、静音、随机及单曲循环标志。

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

## 原创曲库设置恢复

队列 2352 使用生产曲库脚本和两段原创测试音。
三个独立 Player 验证音量、静音和循环模式重开。
循环启用时，曲末重新播放同曲。
循环关闭后，曲末切换到下一首。

存档版本为 1，第六字段记录单曲循环。
五字段档案使用默认关闭状态。
非法循环字段使用默认值，其他合法偏好仍可读取。
较新容器写入被拒后保留原字节，并显示保存失败提示。
恢复兼容容器后，可通过循环按钮重新保存。

三个进程均加载 Vulkan 校验层，退出分配计数归零。
检查覆盖当前默认音频设备，听感由现场复核。
物理无设备及资源不可用恢复各自登记验收。
[结构化结果](evidence/2026-11-21/music-preferences/verification.json)记录程序、着色器和输入摘要。
[循环恢复画面](evidence/2026-11-21/music-preferences/repeat-restored.png)与[保存失败画面](evidence/2026-11-21/music-preferences/write-failed.png)保留可见状态。

```powershell
python -B tools/npr_room/test_music_preferences_runtime.py --lua $luaDll
python -B tools/npr_room/test_original_music_player.py --player $player --resources $resources --source examples/npr-room/assets/music.lua --output $replay
```

## 无设备音频契约

队列 2353 增量构建本工作树的公共呈现测试。
测试显式使用 `AudioRuntime(false)` 和离线呈现。
离线混音、暂停位置、音量、继续和循环均通过。
单次音频在最后采样帧停止。

项目呈现同时覆盖混音能量和静音调节。
实体销毁与关卡替换释放所属音源。
[测试证据](evidence/2026-11-21/offline-audio/verification.json)记录程序、源码及原创夹具身份。
此证据适用于显式无设备模式。
物理无设备的初始化回退和听感留待现场复核。

## 公共音频资源重试

`PresentationRuntime::play` 显式重试当前资源。
被动帧同步保留失败缓存，诊断记录供调用方查询。
资源恢复后，显式播放可加载并产生有效混音。

队列 2355 的测试复现恢复后仍无法播放。
队列 2356 构建 Player 与两种公共呈现测试。
两种测试通过音频播放、暂停、循环及释放检查。
队列 2357 通过三个 Player 的曲库设置回归。

[资源重试证据](evidence/2026-11-21/audio-resource-retry/retry-green.json)记录受检程序。
[原生回归](evidence/2026-11-21/audio-resource-retry/player-regression.json)记录输入、着色器与退出结果。
脚本初始化期间的失败反馈与项目重试另列单元。

## 曲库播放失败恢复

曲库首次播放在运行回调中尝试。
解码失败后，面板显示播放重试提示。
玩家可调整音量，恢复音频文件后再按播放。
成功播放后清除提示，保留音量和循环偏好。

队列 2364 使用截断的原创测试音启动。
界面保持可用，播放次数为零且脚本错误为空。
恢复完整测试音后，播放按钮成功启动同曲。
第二个 Player 读回 45% 音量并正常暂停。
两次退出均释放图像和缓冲，通过 Vulkan 校验。

`try_audio_play` 为 Lua 与托管生成绑定提供共同契约。
它在运行回调中返回播放结果，初始化事务拒绝该调用。
后端契约与本机互操作测试均通过。
托管构建当前为关闭配置，生成一致性通过三项检查。
[失败恢复报告](evidence/2026-11-21/music-recovery/recovery.json)记录测试音及程序身份。
[失败画面](evidence/2026-11-21/music-recovery/unavailable.png)与[恢复画面](evidence/2026-11-21/music-recovery/recovered.png)保留可见反馈。

## 偏好内容版本保护

偏好容器和偏好内容各自持有版本。
保存前重新读取内容版本，较新版本保持原字节。
恢复兼容内容后，可在同一进程沿当前控件重试。
控件显示的临时设置遵守当前会话状态。

队列 2368 验证曲库与环境声的四个原生进程。
启动版本 2 和运行中版本 3 均保持原字节。
恢复兼容内容后可保存，并在重开后继续操作。
四次退出均通过 Vulkan 校验，图像和缓冲归零。
[版本保护报告](evidence/2026-11-21/audio-payload-version/verification.json)记录来源与结果。

## 剧情音轨失败恢复

剧情音轨失败后，曲库恢复进入剧情前的播放状态。
失败会话结束保持曲库状态，修复音轨后可重新播放。
正常剧情播放期间，曲库保持暂停位置。

队列 2385 验证原创双音源的失败、恢复和淡出。
失败期间曲库继续播放，修复后剧情音轨成功接管。
曲库暂停时采样位置保持一致，淡出结束后恢复。
音源启动共两次，Vulkan 校验和退出释放通过。
[原生结果](evidence/2026-11-24/story-music-failure/verification.json)与[本机回归](evidence/2026-11-24/story-music-failure/cpu-regression.json)记录来源和命令。
