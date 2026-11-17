# 教学与第一章角色动作

> 状态为 P4 局部验收。两组剧情引用的九种身体动作已完成资源装配和七部件原生回放。八条自动转场已通过逐帧校验。

`HeroineService.AnimationType` 为剧情提供动作编号。九种动作通过原控制器绑定到 18 个不同的片段。清单保留各状态的原编号、片段摘要、循环标志和出边，包括带条件的返回动作及归一化退出时刻。

## 采样与画面回放

任务 1795 至 1800 在 Unity 2022.3.62f2c1 使用同一原 Avatar，以 60 Hz 读取六批片段。任务 1801 将 16 段新片段加入当前房间角色，并复用已装配的基础坐姿与点头片段。任务 1802 核对七个角色部件的 590,018 个局部变换。最大旋转误差为 0.00000566°，最大位移误差为 0.000000104 米，满足项目动作门槛。任务 1803 复验同一套角色上的 16 种计时动作和 1,019,590 个局部变换。

任务 1804 在原生 Player 回放 37 次输入，逐一选择 18 个剧情片段。215 帧中的七个角色部件使用相同的片段索引和局部时钟。每个片段切换后的时刻从首帧开始。任务 1805 验证正常启动、取消加载和重入。两次运行结束后 GPU 图像和缓冲区计数归零。

| 记录 | SHA-256 |
| --- | --- |
| 原控制器剧情动作清单 | `37126b798501c7143dae40730bc7591e0fc06188be543760d1e6aa0cc9b3dc33` |
| 房间动作装配清单 | `62806cca13d69fc9aed20c4539a418e1750407981e44ce06d7573acab076191f` |
| 原生逐帧报告 | `401bc143d4436e3d852608cc85496c6ac386ce335959441d2cf8b9ca7a995657` |

本机 `story-motion-reference` 保存六批原采样和摘要，`room-opening-body` 保存房间资产。回放使用独立的动作选择入口。

## 自动转场

动作图第三版的 `timeline` 保存固定混合时长、源片段退出时刻和目标片段起始时刻。转换工具以各片段的源时长换算归一化时刻，选择目标明确、无条件且无中断的直接出边。混合以运行时间推进，退出时刻和起始偏移使用片段时钟。

任务 1841 验证跨帧触发、起始偏移、混合完成时触发和环形出边校验。任务 1843 在原生 Player 上运行 1,244 帧，逐帧核对七个部件的片段、时钟和混合进度。八条转场均在源时刻所在帧触发，`anim_story_subbase001_base_start` 的目标片段从 3.247723 秒起播。任务 1845 复验全部 18 个独立片段。动作图、原生构建清单和回放报告保存在本机 `story-motion-reference/timeline-validation`。

| 记录 | SHA-256 |
| --- | --- |
| 动作图 | `7bacbc879ba80be08aa99ea0b904e7b0da07a5907d164e8651da09d9b994efe4` |
| 原生自动转场逐帧报告 | `587de5eba3c8f602862c0431999f82f5f58b76f950950446555ce30722dd2bea` |
| 原生独立片段回归报告 | `5ea92bef21d5b6ab4dc59e477ead673b764d9718f60ae131de34b2edd2697a90` |

剧情验收覆盖动作指令、带条件的返回和中断，以及与表情、视线的叠加。

## 复验

```powershell
python -B -m tools.npr_room.story_body_sources --source $unitySource --chapters $openingChapters --graph $animatorGraph --output $bodyCatalog
python -B -m tools.npr_room.story_body_room --room $timerRoom --output $storyBodyRoom --catalog $bodyCatalog --samples $openingSamples --rig $rig
python -B -m tools.npr_room.verify_timer_motion --room $storyBodyRoom --sample-root $openingSamples --rig $rig --receipt opening-body-receipt.json --output $poseReport
python -B tools/npr_room/test_story_body_player.py --player $player --room $storyBodyRoom --resource-root $resources --output $bodyReplay
python -B -m tools.npr_room.story_body_timeline --room $storyBodyRoom --catalog $bodyCatalog --output $openingTimeline
python -B tools/npr_room/test_story_body_player.py --player $player --room $storyBodyRoom --resource-root $resources --output $timelineReplay --timeline $openingTimeline
```
