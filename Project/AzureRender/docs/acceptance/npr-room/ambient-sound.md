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

本机工程位于 `assets_private/npr-room-dev/room-ambient-controls`，画面位于 `reference-preview/ambient-sound.png`。启动时通过 `--save-root` 指定独立本地档。

## 复验

```powershell
python -B -m tools.npr_room.build_ambient_room --room $musicRoom --output $ambientRoom --source $unitySource --audio-catalog $audioCatalog
python -B tools/npr_room/test_ambient_room_player.py --player $player --project $ambientProject --resource-root $resources --output $ambientReplay
python -B tools/npr_room/test_room_startup_player.py --player $player --project $ambientProject --resource-root $resources --output $roomReplay --expect-heroine --expect-music
```
