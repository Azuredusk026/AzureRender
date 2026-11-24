# 城市窗景与房间集成

> P1 原资产集成通过。P4 城市窗景开关与恢复通过本机 Player 回放。

## 资产与合成

城市窗景对应 `WindowViewType.City` 编号 20。原版场景的渲染器 8872 绑定 `m_buil`，沿原六级变换链放置。底色和发光贴图采用原 PNG 字节，写入同一 GLB 的独立贴图通道。GLB 记录原发光强度，材质槽继承网格目录的编号。

稳态窗口使用原玻璃底色透明度和过渡阈值。该配置保留两片玻璃与过渡网格的资产和位置，并关闭静态回退绘制。玻璃反射、过渡图案、原城市材质的边缘光及室内光照进入 P5。

导入链使用原坐标单位和 Z 轴反射。家具导入、坏引用拒绝、保存重开与标识稳定见[家具验收](furniture.md)。原人体骨架和片段使用 Unity Avatar 的 60 Hz 采样，经同一 GLB 播放入口加载。 [人体动画采样](character-assets.md#人体动画采样)

## 独立 Player

| 任务 | 结果 |
| --- | --- |
| 1664 原主相机成对渲染 | 两次运行各输出两张 1280×720 图。城市显示时，画面左侧窗区有 31,939 个不同像素。两次均无脚本及表现错误，退出后缓冲和图像归零 |
| 1665 启动与重进 | 850 帧、23 次输入、原音效五次。七个角色部件同步进入报告书写并回到待机 |
| 1666 保存与重开 | 两个独立进程保存和读取多页中文笔记，验证排序、删除与版本拒绝后的重试 |

成对渲染采集固定主相机和时步，用于确认城市在窗区参与合成。P5 使用原版配准画面验收色彩和光照。
本机工程位于 `assets_private/npr-room-dev/room-city`，移动后通过项目校验。画面位于 `reference-preview/city.png`。

| 证据 | SHA-256 |
| --- | --- |
| 城市 GLB | `52e1a0b9d13ce7761099ea5df5083d0554c1b71e25294d7f95f188196a8b69c5` |
| 成对渲染报告 | `8a984c944b69cd0d0273bb42de7e0ce50fe8832ed2e298ab1a3614e7fcae0a1a` |
| 城市画面 | `1db3d5089194b899a7ac69df19b79b91dfccf7cd048f01db3a3a5edaacb5f34c` |
| 启动回放报告 | `107110be3242241691b57c538ceede95ffafd8d6b1fd480741295d0b7c1d8bbb` |

## 窗景开关与恢复

城市是原 `WindowViewType` 编号 20，房间初始关闭城市分支。完成购买后显示城市并播放对应环境声。窗景按钮单独控制画面，主按钮同时控制画面和声音。主按钮在任一部分启用时关闭两者，两者均关闭时同时启用。

`room-windows` 按窗景原名保存显式开关。尚无显式设置时，本机城市购买记录启用初始画面。写入失败时保留当前窗景。打开新进程时从同一档恢复所选状态。

任务 1862 运行三个独立 Player。回放验证锁定、购买、单独关闭窗景、关闭后重开、同时切换及启用后重开。左窗对照区在三组开关配对中各有 25,487 个像素变化。相同开关状态的左窗像素逐一相同，桌面 7,000 个对照像素保持不变。

三次进程均播放源城市环境声，并正常退出释放 GPU 资源。此回放验证城市窗景的房间联动，完整色彩与光照按 P5 验收。报告位于本机 `room-opening-city-switch-verified/validation/city-switch/`。

任务 1863 通过购买取消、余额不足、档案写入失败和重开回归。回放报告与环境声购买记录的摘要一致。

| 证据 | SHA-256 |
| --- | --- |
| 装配清单 | `85ae1404c7241aaaac1a729c1c90345845325519eacce7c78d6cbe8088384bf1` |
| 窗景回放 | `02e306b5794bb83f37c5144555949a7be38d06ebe6bbebba0e7b897f20249ff1` |

## 复验

```powershell
python -B -m tools.npr_room.build_city_window --source $unitySource --catalog $assetCatalog --room $notesRoom --output $cityRoom
python -B tools/npr_room/test_room_startup_player.py --player $player --project $cityRoomProject --resource-root $releaseBuild --output $cityRoute --expect-heroine
python -B tools/npr_room/test_notes_player.py --player $player --project $cityRoomProject --resource-root $releaseBuild --output $citySaveReplay
python -B -m tools.npr_room.build_purchase_room --room $openingRoom --output $citySwitchRoom --source $unitySource --unlock-catalog $unlockCatalog --window-scenes $windowScenes --city-receipt $cityImportReceipt
python -B tools/npr_room/test_city_switch_player.py --player $player --project $citySwitchProject --resource-root $resources --output $citySwitchReplay
```
