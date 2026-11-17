# 开场桌面姿态

> P4 验收范围为教学准备段的 `Pc` 姿态和课本静置蒙皮。

教学源行 6 立即将桌面切到 `Pc`。原控制器在第 0 层直接读取 `FromBookToPc` 的末帧。装配保留键盘、报告本和课本的源局部位置与旋转。

`WorkspaceLayout` 提供 `Pc`、`Book` 和 `Report` 三套末帧。课本使用原网格、11 根骨骼、原材质及主纹理。静止页姿态读取课本控制器默认状态的九条旋转曲线。

桌面命令在首个剧情动作前执行。书本装饰由[书本布置](opening-book-layout.md)独立管理。桌面过渡、翻页事件及与角色的连续动作使用各自的动态回放。

## 原生回放

任务 1866 运行两个独立 Player。测试房间从原 `Book` 末帧开始，以常规教学入口执行 `Pc` 命令。检查读取三个道具切换前后的原生组件。最大位置误差为 0.000000044 米，最大欧拉角分量误差为 0.000000785°。

键盘区域有 9,128 个像素变化，切换后与默认 `Pc` 画面逐像素一致。窗区 49,860 个对照像素保持一致。回放保留书本装饰存档状态。两个进程退出后 GPU 缓冲和图像归零。

报告和截图位于本机工程 `room-opening-desk/validation/opening-desk-pose/`。

任务 1867 使用同一房间完成两章常规剧情回放。书本切换与读档、七部件动作同步、32 段原语音及音乐均通过。报告及截图位于 `room-opening-desk/validation/opening-desk-regression/`。

| 记录 | SHA-256 |
| --- | --- |
| 原 `FromBookToPc` 片段 | `927b4d7667463b1efb4dd5366d841efbfc0adbcbb9350df7a70fc9faab3206a5` |
| 课本 GLB | `bf52a66e56074047e9da8d77657c134d5809e3b572272bf4956179c4c30138fb` |
| 房间装配清单 | `a3a3b03e7f6d49efe905e3c5252602140fc4173e2f2c7ad1b0c33174471649d6` |
| 桌面回放 | `6ab2d8a700a3163e22ef2d2709f5ee630301fa8c0d17673bc16ab5aa056e0d42` |
| 常规剧情回归 | `e0b9c4fd5d4d2050548c7f71c028dd6b57274ed352a1173acd6875b48e687dc6` |

## 复验

```powershell
python -B -m tools.npr_room.build_story_desk_room --source $unitySource --catalog $assetCatalog --room $storyMotionRoom --output $deskRoom
python -B tools/npr_room/test_story_desk_player.py --player $player --room $deskRoom --resource-root $runtimeResources --output $deskReplay
```
