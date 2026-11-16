# 城市窗景与房间集成

> 状态为 P1 验收通过。原家具、角色、动画、城市窗景、计时、原音效和本地存档共用同一独立 Player 工程。

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

## 复验

```powershell
python -B -m tools.npr_room.build_city_window --source $unitySource --catalog $assetCatalog --room $notesRoom --output $cityRoom
python -B tools/npr_room/test_room_startup_player.py --player $player --project $cityRoomProject --resource-root $releaseBuild --output $cityRoute --expect-heroine
python -B tools/npr_room/test_notes_player.py --player $player --project $cityRoomProject --resource-root $releaseBuild --output $citySaveReplay
```
