# 房间环境声

> 状态为 P3 局部验收。内置循环环境声、混音和本地设置通过独立 Windows Player 回放。

环境声目录从 Recovery `AmbientMaster` 和 `AmbientSoundType` 提取 30 个源身份，保留原始编号、GUID 和音频容器。面板有 28 种循环环境声。两条烟花单次音频保留在来源清单，烟花事件与窗景联动按 P4 验收。

每种环境声使用独立音源和静音状态。初始音量为零，首次开启使用 50%。调音量和静音保留活动音源及播放位置。原声容器与音量设置在相同房间持续叠加，重开恢复已开启的环境声及各自增益。

## 回放

共享队列任务 1713 使用两个独立 Player。第一个进程开启 Light Rain，将音量从 50% 调至 45%，静音后恢复，再开启 City。第二个进程恢复两个环境音源，单独静音 Light Rain，City 保持在 50%。每个进程启动两个环境音源和一个曲库音源，按原音频 GUID 检查，共用同一独立档。

任务 1714 完成 850 帧启动、计时、角色与曲库回归。空白档的环境声音量为零，两次进房共播放两次 Aqua Room，计时提示音五次。

| 证据 | SHA-256 |
| --- | --- |
| 来源报告 | `4b7c6265a914c464bea0ab56cff62eb1d70013a215f0ee4b814b9a5ee40ba15e` |
| 独立 Player 回放 | `9469453ed47b783efdbf0e72993f24fa2b6b9c3ce331d2940ded60974a99192e` |
| 房间回归 | `c18432c8ab697ff7107e51cbecdd6d38477e1e5c4db45064e31cbedb61e36968` |
| 环境声画面 | `451f9691845040e7a64b1328abf47f17535c034ef8a08e38662f355bde86ade3` |

本机试听工程位于 `assets_private/npr-room-dev/room-ambient-controls`，画面位于 `reference-preview/ambient-sound.png`。启动时通过 `--save-root` 指定独立本地档。整合房间的可用范围见[环境声解锁](ambient-access.md)。

## 复验

```powershell
python -B -m tools.npr_room.build_ambient_room --room $musicRoom --output $ambientRoom --source $unitySource --audio-catalog $audioCatalog
python -B tools/npr_room/test_ambient_room_player.py --player $player --project $ambientProject --resource-root $resources --output $ambientReplay
python -B tools/npr_room/test_room_startup_player.py --player $player --project $ambientProject --resource-root $resources --output $roomReplay --expect-heroine --expect-music
```

## 原创环境声恢复

环境声在首帧按存档逐条尝试播放。
失败音源保留偏好和重试提示，其他音源继续运行。
玩家恢复资源后，可调节音量或重新启用以重试。
调节活动音源的音量保持播放位置。

队列 2367 使用两段原创测试音和生产脚本。
第一段音频截断时，第二段正常播放。
恢复第一段后，音量从 45% 调至 50%并成功播放。
较新容器被拒后保留原字节，恢复兼容容器可重试。
第二个 Player 恢复两条音源，单独静音第一条。

两次退出通过 Vulkan 校验，活动图像和缓冲归零。
[结构化结果](evidence/2026-11-21/ambient-recovery/verification.json)记录程序与输入身份。
[失败画面](evidence/2026-11-21/ambient-recovery/unavailable.png)及[恢复画面](evidence/2026-11-21/ambient-recovery/recovered.png)登记界面反馈。
物理设备断开及听感留待现场复核。

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
