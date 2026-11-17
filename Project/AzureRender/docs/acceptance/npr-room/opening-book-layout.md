# 开场书本布置

> P4 验收范围为书本显隐、剧情临时布置和读档恢复。

原版装饰主表将 `Book_1` 设为书本类默认布置。五套布置共引用 11 个场景对象，沿用原位置和网格。装配按渲染器绑定四份原材质和四张主纹理。

教学源行 5 和第一章源行 92 均在首个动作前选用 `Book_1`。教学及重播使用临时状态，结束后恢复档内布置。首玩当前主线且 `Book_5` 仍锁定时，准备段写入布置。`Book_5` 的原解锁条件为完成 `later_01`。

`room-book-layout` 以版本 1 保存原布置名。写入失败保留档内布置，剧情结束时按该档恢复。教学源行 6 的 `Pc` 属于桌面动画，单列验收。

| 来源 | SHA-256 |
| --- | --- |
| 原房间场景 | `e2dc043e56623a9eff379dac530c2290d4ce4677cf82a9fea539d5aa306053e8` |
| 原装饰主表 | `9d2beabca442be7c73201a08eeaaa547659b3c40e3c7323675d8e7191f869597` |
| 原剧情准备段 | `a7a028293b9262d6e0400e40482a5beae54706e8943b77c31cac4ef7160addb2` |
| 原装饰初始值 | `47b04aefd790526a7a392104272c1877b8cc62b869714b0fd90874f38c164d9b` |
| 原剧情收尾 | `d4e7f90ebff414671269a8933d3b7fcb1b4f38390871bdadbcf8e6e20a50bf2c` |
| 房间装配清单 | `1cee79470e464459d6cfdff6c558272b82de9f5c9c71c3fccab9b4898ae9b62c` |

## 原生回放

任务 1864 从常规启动路线运行三个独立 Player。测试档预置 `Book_5`，用于对照临时布置与存档恢复。教学切换改变书本区域 462 个像素，结束后逐像素恢复。档案字节保持一致。

首次第一章在准备段保存 `Book_1`。收尾画面与新档默认布置逐像素一致。首次教学的 22 段原语音及第一章的 10 段原语音均通过回归。三个进程退出后 GPU 缓冲和图像归零。

画面、运行日志及报告位于本机工程 `room-opening-book-layout/validation/opening-book-layout/`。
报告 SHA-256 为 `7a728fc3ede16beb96746e64858b1d315214f1c92558d7983bfc683a5f788f39`。

## 复验

```powershell
python -B -m tools.npr_room.build_story_book_room --source $unitySource --catalog $assetCatalog --room $cityRoom --output $bookRoom
python -B tools/npr_room/test_story_player.py --normal-room --verify-books --player $player --room $bookRoom --resource-root $runtimeResources --chapters $chapters --inventory $storyInventory --output $bookReplay
```
