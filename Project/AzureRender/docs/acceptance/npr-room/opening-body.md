# 教学与第一章角色动作

> 状态为 P4 局部验收。两组剧情引用的九种身体动作已完成资源装配和七部件原生回放。

`HeroineService.AnimationType` 为剧情提供动作编号。九种动作通过原控制器绑定到 18 个不同的片段。清单保留各状态的原编号、片段摘要、循环标志和出边，包括带条件的返回动作及归一化退出时刻。后续剧情执行使用这些出边验证混合、过渡和中断。

## 采样与画面回放

任务 1795 至 1800 在 Unity 2022.3.62f2c1 使用同一原 Avatar，以 60 Hz 读取六批片段。任务 1801 将 16 段新片段加入当前房间角色，并复用已装配的基础坐姿与点头片段。任务 1802 核对七个角色部件的 590,018 个局部变换。最大旋转误差为 0.00000566°，最大位移误差为 0.000000104 米，满足项目动作门槛。任务 1803 复验同一套角色上的 16 种计时动作和 1,019,590 个局部变换。

任务 1804 在原生 Player 回放 37 次输入，逐一选择 18 个剧情片段。215 帧中的七个角色部件使用相同的片段索引和局部时钟。每个片段切换后的时刻从首帧开始。任务 1805 验证正常启动、取消加载和重入。两次运行结束后 GPU 图像和缓冲区计数归零。

| 记录 | SHA-256 |
| --- | --- |
| 原控制器剧情动作清单 | `37126b798501c7143dae40730bc7591e0fc06188be543760d1e6aa0cc9b3dc33` |
| 房间动作装配清单 | `62806cca13d69fc9aed20c4539a418e1750407981e44ce06d7573acab076191f` |
| 原生逐帧报告 | `401bc143d4436e3d852608cc85496c6ac386ce335959441d2cf8b9ca7a995657` |

本机 `story-motion-reference` 保存六批原采样和摘要，`room-opening-body` 保存房间资产。回放使用独立的动作选择入口。剧情表情、视线和状态混合使用剧情视觉矩阵验收。

## 复验

```powershell
python -B -m tools.npr_room.story_body_sources --source $unitySource --chapters $openingChapters --graph $animatorGraph --output $bodyCatalog
python -B -m tools.npr_room.story_body_room --room $timerRoom --output $storyBodyRoom --catalog $bodyCatalog --samples $openingSamples --rig $rig
python -B -m tools.npr_room.verify_timer_motion --room $storyBodyRoom --sample-root $openingSamples --rig $rig --receipt opening-body-receipt.json --output $poseReport
python -B tools/npr_room/test_story_body_player.py --player $player --room $storyBodyRoom --resource-root $resources --output $bodyReplay
```
