# 环境声解锁

> 状态为 P3 局部验收。Windows 环境声按剧情和购买记录验证可用范围。

28 种循环环境声使用[原版环境解锁表](unlock-catalog.md)中的条件。锁定项显示条件并屏蔽音量操作。已过截止时间且未解锁的项从面板隐藏。已有的音量记录保留在本地档，播放时按当前解锁状态核对。

`room-unlocks` 槽保存版本、已完成剧情组及已有授权的源环境名。环境声购买写入 `room-purchases`。剧情完成、窗景联动和首次解锁通知按各自功能验收。

## 环境声购买

环境声面板显示可用积分，点击可购买的锁定项目打开确认区。城市和水母定价 14,000 点，地平线定价 6,000 点。余额不足时隐藏确认按钮。取消后保留当前余额和音量。

购买记录保存环境原名和原价。余额为工时累计积分减去已提交的购买金额。一次写入同时记录权益和扣点；提交成功后播放原 `BuyItem` 音效并启用对应环境声。购买记录重开后继续有效。同步启用窗景、购买专属剧情及其他装饰按对应场景验收。

任务 1859 从常规启动路线运行三个独立 Player。13,999 点的独立档无法确认城市购买。15,000 点的档覆盖取消与较新版本文件的写入拒绝。恢复可写档后成功购买城市，余额为 1,000 点。重开时恢复城市声音，原购买按钮保持不可用，地平线仍受余额限制。三次退出后 GPU 资源计数均为零。

本机报告位于 `room-opening-purchases/validation/purchases/`。来源清单 SHA-256 为 `bac89e19193c97d8e40463e555f596fcb917becde62ed73a9c0ec0948d0daf72`。回放报告 SHA-256 为 `89ccbb15c0446d605383f9750bf8db6fcc53388d409048c8893145a05fbba7f9`。

任务 1860 在同一工程通过中文笔记、排序、删除、版本拒绝和重开回放。

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
python -B -m tools.npr_room.build_purchase_room --room $openingRoom --output $purchaseRoom --source $unitySource --unlock-catalog $unlockCatalog
python -B tools/npr_room/test_purchase_room_player.py --player $player --project $purchaseProject --resource-root $resources --output $purchaseReplay
```
