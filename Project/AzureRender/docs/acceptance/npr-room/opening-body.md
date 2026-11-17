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

## 条件转场

动作图第四版通过 `signals` 声明指令，通过 `branches` 声明条件出边。指令更新当前条件。同一状态的每种指令对应一条出边；标为 `once` 的出边在触发后消耗本次指令。直接选择片段状态会清除当前条件。

出边等待源片段到达 `afterSeconds`，并等待当前混合完成。条件出边和固定出边在同一时刻可用时，优先使用条件出边。混合时长和目标起始时刻分别由 `blendSeconds` 和 `startSeconds` 指定。

任务 1870 验证指令等待、目标偏移、一次触发和退出混合。任务 1871 在原生 Player 记录 170 帧，七个部件的动作和混合时钟一致。回放包含一次手势和指定的收尾动作，GPU 资源在退出时归零。此记录使用独立夹具，常规剧情按下节所列入口运行。

| 记录 | SHA-256 |
| --- | --- |
| 原生条件转场报告 | `4d246d714caea23745510d8bf0e16a9257dda52ee78c109c16b0be66d286ac7b` |
| 原生构建清单 | `22834ea2d000b6338ad22518b8cabd37e0143c5b3f190c2d671e03347e362fce` |

本机 `story-motion-reference/signal-validation/` 保存逐帧记录、输入清单和受检程序。

## 常规剧情入口

常规房间按剧情命令选择原动作入口。准备段读取首个动作指令，未指定时进入坐姿。动作编号 `-1` 保持当前动作，连续相同编号保持播放进度。剧情结束后恢复房间待机。

九个原编号使用同一份装配清单。七个角色部件共享动作状态和八条自动转场。受条件驱动的返回、触发器消耗及其他图层单列验收。

任务 1865 在常规剧情记录 583 次帧状态。七个部件的片段、时钟及混合进度逐次一致。教学按源顺序进入 401、12、15、3。第一章按源顺序进入 5、1、3、1000、1003。

回放同时通过书本布置、章节读档、原语音与音乐检查。三个进程退出后 GPU 缓冲和图像归零。运行报告位于本机工程 `room-opening-body-route/validation/opening-body-route/`。

| 记录 | SHA-256 |
| --- | --- |
| 常规房间装配清单 | `08b79d44119ee9d9d73c51ae5c602c574db0b8f660b9904c15636935e743b0ff` |
| 常规剧情回放 | `73b28c56f51c74d6de3cedcedea406c95cb762eb719fcaa3ea019b5fd1335c07` |

## 第一章动作返回

第一章动作图根据原控制器的 `MotionType` 条件生成入口和返回路径。坐姿指令保留当前动作的收尾片段。1003 指令在等待状态触发一次手势，手势结束后返回等待。教学沿用本页登记的动作入口。

任务 1874 使用 Unity 2022.3.62f2c1 读取原控制器和 13 个源片段。以 60 Hz 记录 1,140 帧，每条记录包含原状态路径及混合时机。程序覆盖提前返回、入场期间发出手势指令，以及手势结束后回坐姿。

任务 1875 在原生 Player 上用同一指令序列回放 1,150 帧。18 次转场顺序一致，起始时刻最多相差 2 帧。七个部件逐帧同步，1003 手势进入一次，退出时 GPU 图像和缓冲区计数归零。

| 记录 | SHA-256 |
| --- | --- |
| 原控制器采样输入 | `f1543508e58055947ddf2fc67dc76551716d70983dd7db7388e0fb825c7e0fe7` |
| Unity 状态与混合记录 | `9d16aed313f3715ee546aa612e54ea8a66a9c5ecdab228b0b43227b23a7331fa` |
| 房间动作图 | `820312b7e6e1acab68ba8c69594ec1e3813503798ca2ba73c0af40d623b0318f` |
| 原生对照报告 | `838e28952486b3e249bba263b18f7929bb06dfe5bbe2c7824bbcef947be7f273` |
| 房间第一章回放 | `b107e93d3a4c847f173d019d411aadf96b6e000d18fd3efb3971dbb3fc8e1cee` |

任务 1880 通过五关卡启动链进入实际房间。测试档已完成教学，玩家使用正计时升至 2 级，随后解锁并读完第一章。150 次采样核对七个部件、动作收尾及一次手势。10 段原语音、原曲、书本和章节存档均与该路线一致。

本机 `story-motion-reference/first-chapter-controller/` 保存采样及回放记录。房间诊断以临时关卡引用共享资产。独立包仍需从首次教学重跑完整路线。图层叠加和教学条件转场各自采证。

## 教学相机段身体动作

教学第 16 行的相机指令直接启动原身体动作。初始片段播放完成后，以 0.3 秒混合进入循环。第 26 行的 401 指令触发收尾。随后到达的点头指令等待收尾，再从原入口起播。

任务 1881 以原控制器记录 1,030 帧。任务 1882 使用同一原 Avatar 采样启动和循环片段，共 413 帧。Unity 记录的片段时长分别为 4 秒和 2.85 秒。本机 `story-motion-reference/tutorial-camera/` 保存以下输入。

| 记录 | SHA-256 |
| --- | --- |
| 源控制器与片段清单 | `dc0f80764490863adbf8ce14e5f1eb27e3f82751d14a03c803ac3e71fa5d464d` |
| Unity 状态与混合记录 | `e10215c46dcf0d77e06ddd8a3b355acc76fef2f28459d8783b723176753d18f3` |
| 两段动作采样 | `3972ed2885218792df21152cb8b1bb6a700f60eb0dad0857889c64f2b022258f` |

原控制器在收尾后仍收到 401 时，每四帧重启坐姿。任务 1884 记录延迟指令时的 36 次重启。动作图用同一坐姿片段的两条入口表达这段循环。点头指令到达后，动作图进入点头片段。

任务 1883 核对七个部件的 118,118 个变换。位置最大偏差为 0.000000104 米，旋转最大偏差为 0.00000561 度。任务 1885 和 1886 各跑 1,040 帧。两条原生路线各处转场与 Unity 最多相差两帧，七个部件始终同步。

任务 1887 从实际房间入口跑完教学和第一章。两次启动分别核对 22 和 10 段源语音，并检查解锁、布置存档和配乐。两个进程均正常退出，GPU 缓冲和图像计数归零。独立包、表情、视线及画面各有单列验收。

本机 `story-motion-reference/tutorial-camera/native-validation/` 保存以下记录。

| 记录 | SHA-256 |
| --- | --- |
| 延迟指令 Unity 回放 | `7153bbec4946e5b22d5d511ba2ff5d6be8f42719a7444cd3a1d27c07837fb49d` |
| 模型与动作装配清单 | `465a0356cb2db22a507e18ca613121c7d18e8d4d94e119ab1c9c758ce58d5c60` |
| 七部件姿态对照 | `96de612f2cf754905b1aca39881cc429295f8e7c79f289a11b4af9ba1194f7f4` |
| 教学与第一章动作图 | `3eb274f55d486f8754519f6965e8e5cd303f4f37c5a5ababa914152f784853f6` |
| 原生及时指令回放 | `42a276d28189c2dc574de27ebd42d6ef404886aa1384442f9f5b6c2d09e68e5f` |
| 原生延迟指令回放 | `bf6ee00d1fedc5b3d3ad08901b05f58020684439179bed45488479a5d97d7ca1` |
| 常规入口回放 | `656158e33f30ef1a0be28c9ff8c5d32b6ead5d8be3ea0390691c682f3f97c0d5` |

## Windows 教学相机

第 16 行的相机指令将相机移至书桌锚点。原控制器随后播放两秒旋转，位置保持在锚点。播放结束后，相机保持末帧。剧情收尾恢复开场旋转，保留书桌位置。

任务 1888 采样 151 个 Unity 姿态。任务 1890 核对 156 个原生姿态，位置逐帧一致，最大旋转偏差为 0.000012 度。任务 1891 从房间入口跑完教学和第一章。教学相机活动及收尾姿态均符合源样本，同时通过身体动作、语音和存档回归。

本机 `story-motion-reference/tutorial-camera/windows-camera/` 保存采样、回放及运行日志。

| 记录 | SHA-256 |
| --- | --- |
| 来源清单 | `aa0407d601b3585ae921ea391ad3e9a6b2d3a264dcf4ec4669729ba9d0565092` |
| Unity 姿态 | `adaad1c95cec3313b8472840fbc81df8e42646c87f2028932e120da42be45242` |
| 原生固定帧步回放 | `a0551d0e7cdf61775940bcd6ca5c2365e2a117cf226ba903f35abaccbf8f3a21` |
| 常规入口回放 | `1142947fd6ba68d1a7ff67c9d1adad20618fd9413ce523cc8066a5cee9af9664` |

## 复验

```powershell
python -B -m tools.npr_room.story_body_sources --source $unitySource --chapters $openingChapters --graph $animatorGraph --output $bodyCatalog
python -B -m tools.npr_room.story_body_room --room $timerRoom --output $storyBodyRoom --catalog $bodyCatalog --samples $openingSamples --rig $rig
python -B -m tools.npr_room.verify_timer_motion --room $storyBodyRoom --sample-root $openingSamples --rig $rig --receipt opening-body-receipt.json --output $poseReport
python -B tools/npr_room/test_story_body_player.py --player $player --room $storyBodyRoom --resource-root $resources --output $bodyReplay
python -B -m tools.npr_room.story_body_timeline --room $storyBodyRoom --catalog $bodyCatalog --output $openingTimeline
python -B tools/npr_room/test_story_body_player.py --player $player --room $storyBodyRoom --resource-root $resources --output $timelineReplay --timeline $openingTimeline
python -B -m tools.npr_room.build_story_motion_room --room $bookRoom --catalog $bodyCatalog --chapters $openingChapters --output $storyMotionRoom
python -B tools/npr_room/test_animation_signals_player.py --player $player --room $storyMotionRoom --resource-root $resources --output $signalReplay
python -B tools/npr_room/test_story_player.py --normal-room --verify-books --verify-body --player $player --room $storyMotionRoom --resource-root $resources --chapters $openingChapters --inventory $storyInventory --output $motionRouteReplay
python -B -m tools.npr_room.story_body_probe --source $unitySource --layout $avatarProbe --catalog $bodyCatalog --graph $animatorGraph --output $unityProbe
& $unity -batchmode -nographics -quit -projectPath $unityProbe -executeMethod StoryBodyControllerProbe.Run -logFile $unityLog
python -B tools/npr_room/test_story_signals_player.py --player $player --room $deskRoom --resource-root $resources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --source-trace $unityTrace --output $firstChapterReplay
python -B tools/npr_room/test_story_signal_room_player.py --player $player --room $deskRoom --resource-root $resources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --output $firstChapterRoomReplay
python -B -m tools.npr_room.build_story_signal_room --room $deskRoom --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --output $signalRoom
python -B -m tools.npr_room.story_body_probe --tutorial-camera --source $unitySource --layout $avatarProbe --catalog $bodyCatalog --graph $animatorGraph --output $tutorialProbe
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod StoryBodyControllerProbe.Run -logFile $tutorialLog
$env:AZURE_NPR_MOTION_BATCH = "tutorialCamera"
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod TimerHumanoidSampler.Run -logFile $tutorialPoseLog
python -B -m tools.npr_room.tutorial_camera_body bake --room $deskRoom --rig $rig --source-receipt $tutorialSource --samples $tutorialSamples --output $tutorialModels
python -B tools/npr_room/test_story_signals_player.py --player $player --room $deskRoom --resource-root $resources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --source-trace $tutorialTrace --source-receipt $tutorialSource --models $tutorialModels --output $tutorialReplay
python -B tools/npr_room/test_story_signal_room_player.py --player $player --room $deskRoom --resource-root $resources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --source-receipt $tutorialSource --models $tutorialModels --output $tutorialRoomReplay
```

复验 Windows 教学相机时，使用同一份教学采样工程。正式装配和常规入口回放接收同一份相机姿态记录。

```powershell
python -B -m tools.npr_room.opening_camera_probe --source $unitySource --project $tutorialProbe
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod OpeningCameraProbe.Run -logFile $cameraLog
python -B tools/npr_room/test_opening_camera_player.py --player $player --room $deskRoom --resource-root $resources --source-trace $cameraTrace --output $cameraReplay
python -B tools/npr_room/test_story_signal_room_player.py --player $player --room $deskRoom --resource-root $resources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --source-receipt $tutorialSource --models $tutorialModels --camera-trace $cameraTrace --output $cameraRoomReplay
python -B -m tools.npr_room.tutorial_camera_body stage --room $deskRoom --catalog $bodyCatalog --controller $animatorGraph --chapters $openingChapters --source-receipt $tutorialSource --models $tutorialModels --camera-trace $cameraTrace --output $cameraRoom
```
