# 环境与装饰解锁

> 状态为 P0 静态证据。解锁、购买和窗景运行按 P3 与 P4 验收。

Recovery 的 `UnlockEnvironmentMaster` 与 `UnlockDecorationMaster` 为每项内容定义可用条件。目录保留源条件、参数和移动版试玩标志。Windows 本地使用 `UnlockConditionService` 的规则，剧情条件按已完成的源剧情组 ID 判断，点数条件按购买记录判断。

| 目录 | 项目 | 初始可用 | 剧情解锁 | 点数购买 | 带截止时间 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 环境 | 46 | 12 | 27 | 7 | 2 |
| 装饰 | 99 | 8 | 13 | 78 | 0 |

所有剧情条件均能在源剧情组表中定位。环境截止时间属于 ValentineSweets 与 Brook。购买价格及截止时间保留原值。

两次独立提取得到 SHA-256 `326bb0be2016dbf1a3d8ba7f3e2b81c1de39c2e96a6762ae08da3121dca3afea`。本机报告位于 `assets_private/npr-room-dev/source-lock/unlock-catalog.json`。

## 复验

```powershell
python -B -m tools.npr_room.unlock_catalog --source $unitySource --inventory $storyInventory --output $unlockCatalog
python -B -m unittest tools.npr_room.test_unlock_catalog
```
