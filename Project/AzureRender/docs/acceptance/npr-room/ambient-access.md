# 环境声解锁

> 状态为 P3 局部验收。Windows 环境声按剧情和购买记录验证可用范围。

28 种循环环境声使用[原版环境解锁表](unlock-catalog.md)中的条件。锁定项显示条件并屏蔽音量操作。已过截止时间且未解锁的项从面板隐藏。已有的音量记录保留在本地档，播放时按当前解锁状态核对。

`room-unlocks` 槽保存版本、已完成剧情组和已购买环境的原名。回放通过隔离档提供条件。剧情完成、扣点购买和首次解锁通知按各自功能验收。

## 回放

共享队列任务 1724 使用两个独立 Player。档内有小雨和城市声的音量记录。无解锁记录时，只有初始可用的大雨能播放。提供 `main_19` 的完成记录和 City 的购买记录后，重开恢复三个环境声。单独静音小雨时保留其 45% 音量。

可控日期验证 Brook 在截止时刻仍可显示锁定项，截止后隐藏，已取得的剧情解锁继续有效。任务 1725 通过 850 帧启动、提示音、曲库和角色回归。

| 证据 | SHA-256 |
| --- | --- |
| 接入报告 | `9f7c03b2b3241194823e015737996fa07ce87c7ba160f3382cfe068fcf17913d` |
| 独立 Player 回放 | `a129736ad0dd812a113b6c9914b41251a20759b41f8bf5d3441d5fb447636c87` |
| 房间回归 | `49cdb7c03b6552c8e40c2f9ce5595ebdb55ffbe49e8f69b88930527eb154c88b` |
| 锁定面板 | `04cafbe2a628d14d9ff2351a9f17ab9570fc41efcc9a8ee1447ee375eeddf7f3` |

本机工程位于 `assets_private/npr-room-dev/room-unlocks`。

## 复验

```powershell
python -B -m tools.npr_room.build_unlock_room --room $choiceRoom --output $unlockRoom --source $unitySource --unlock-catalog $unlockCatalog
python -B tools/npr_room/test_unlock_room_player.py --player $player --project $unlockProject --resource-root $resources --output $unlockReplay
```
