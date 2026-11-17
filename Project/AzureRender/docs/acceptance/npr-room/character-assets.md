# 角色与窗景资源转换

> 状态为 P0、P1 与 P4 局部验收。角色控制器已登记，角色部件与两段人体动画通过 Player 回放。

## 角色动画控制器

Recovery 角色控制器包含五个有序图层、30 个参数、32 个状态机和 390 个状态。
源记录包含 504 条状态转场、59 条入口与状态机转场，以及 42 项状态行为。
目录以文件 ID 区分同名状态，并保留转场顺序、条件、入口、混合参数和状态行为。
静态引用覆盖 352 项动画和一个行为脚本，全部文件 ID 与外部 GUID 均可解析。
运行时可达性通过剧情、计时、交互与设置回放核验。

本机目录 `assets_private/npr-room-dev/source-lock/animation-graph.json` 保存完整清单。
两次独立扫描的清单摘要均为 `30799bb37e1d92757f94df4ee10a4a12876d2e259c009f4d8214ae75378cbdf6`。

```powershell
python -B -m tools.npr_room.animator_inventory --source $unitySource --output $animatorManifest
```

## 角色骨架与部件

角色转换器读取 `SkinnedMeshRenderer` 的骨骼引用。
骨骼层级沿场景父级关系装配。
GLB 保留关节顺序、反向绑定矩阵和每顶点权重。
场景和网格使用同一个 Z 轴反射。

角色夹具包含脸、身体、头发、上衣、下装、袜子和外套。
每项均携带原基本颜色纹理。
脸含 57 个形变通道，头发含 66 个关节和两个形变通道。
于同一坐标场景中计算的角色高度约为 1.67 米。

Release Player 通过基础部件及角色关卡校验。
加入 65 关节的外套后，正面绑定姿态通过共享队列完成回放和截图，袖口和手腕可见。
截图存放于本机私有目录的 `reference-preview/heroine-bind-pose.png`。
实际造型、角色动作及同步时间以原版对照为准。

## 人体动画采样

隔离采样工程使用 Recovery 的 `CharacterAvatar` 和 371 个源场景变换。
Recovery 工程版本为 `2022.3.62f2`，采样器实测运行于 `2022.3.62f2c1`。
采样器核对髋部、头部、左手和右脚的人体骨骼绑定，按 60 Hz 记录局部变换。
待机片段 `anim_story_base001_base` 含 182 个采样点，报告书写起始片段 `anim_work_report_base_start` 含 218 个采样点。
采样使用 Unity 的手动动画图。 [AnimationClipPlayable](https://docs.unity3d.com/2022.3/Documentation/ScriptReference/Animations.AnimationClipPlayable.html) · [PlayableGraph.Evaluate](https://docs.unity3d.com/2022.3/Documentation/ScriptReference/Playables.PlayableGraph.Evaluate.html)

转换器沿父级关系匹配 GLB 节点，经 Z 轴反射写入平移、旋转和缩放。
七个原角色部件共比较 114,400 个局部变换。
旋转最大误差为 0.0000057°，平移最大误差为 0.000000104 米，满足 0.1° 和 0.0005 米的采样容差。
数据及精度报告以本机夹具记录为准。

共享队列任务 1642 以 Release Player 从待机切换到报告书写片段。
七个部件在同一帧切换，采集 12 帧供核对。
回放期间无脚本或表现错误，退出后缓冲与图像计数均为零。
本机可复验工程位于 `assets_private/npr-room-dev/heroine-body-motion`，移入后通过项目校验。
待机与书写截图位于 `reference-preview/heroine-idle.png` 和 `reference-preview/heroine-writing.png`。
控制器的图层混合、动画事件、道具约束及完整房间时序按对应验收项实现。

| 验收文件 | SHA-256 |
| --- | --- |
| 人体逐帧变换 | `3dd906e7f933c497d375b4734064418739dd500b03b5e0d93db3f537915b9fb6` |
| GLB 采样对账 | `99dd3c7a8ff36eaf51e09367814fb89468a4a42deb878e35e6b1ed23e4660250` |
| Player 回放 | `61c011574ec2f90c9d1352f85a0eb75ba48551855f36c1566fb76dde240c26bb` |

```powershell
python -B -m tools.npr_room.humanoid_clip_export --rig $sourceRig --samples $humanoidSamples --input $originalSkinnedGlb --output $animatedGlb
```

## 说话表情片段

原 `anim_face_talk` 片段将九条曲线绑定到 `Character/Face`。
导出器按源采样率与有效关键帧取样，将权重写入脸部 57 个目标的原顺序。
片段使用线性采样帧，保留循环标记和其余目标的绑定权重。 [glTF 2.0 动画规格](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations)

有效区间的 548 个原关键帧均与 GLB 对账。
最大归一化权重误差约为 `2.93 × 10⁻⁸`。
两个片段相位分别通过 Release Player 呈现，各写入两张截图。
两张首帧有 105 个像素变化，位于嘴部区域。脚本与表现诊断为空，退出后缓冲与图像计数均为零。
截图、GLB 和源采样核对存放于本机私有目录的 `reference-preview/heroine-talk`。
人体动作、语音同步和原画面配对按角色专项验收。

## 脸部表情库

Recovery 的 19 个 `anim_face_*.anim` 直接绑定同一脸部网格。
导出文件依源片段名称保留 853 条曲线，合计 5,617 个采样时刻。
有效区间的 9,574 个原关键帧逐项复核，最大归一化误差约为 `2.97 × 10⁻⁸`。
全部片段随包含 57 个形变目标的 GLB 移入私有目录 `face-expressions`，并通过移动后 Player 项目校验。
剧情触发表、表情混合及原版画面对照按角色专项验收。

## 开场表情层采样

第一段教学和第一章使用同一角色控制器。九种入口由 `Facial` 参数选择，固定混合时长为 0.25 秒。嘴型层由 `Enable_Talk` 控制，以 0.1 秒进入，以 0.25 秒退出。

任务 1895 在 Unity 2022.3.62f2c1 采样 631 帧。三组角色分别记录底层、表情层和表情叠加嘴型的 57 项权重。快速指令覆盖测试了混合期间的条件变化。嘴型层在这条路线改变七种嘴部权重。

测试工程使用原控制器、原 Avatar 和源片段。形变测试网格保留源目标名称和初值。常规房间叠加及画面对照按各自验收运行。

本机 `story-motion-reference/opening-face/` 保存以下记录。

| 记录 | SHA-256 |
| --- | --- |
| 表情来源清单 | `7f6fb354a50e76cf7141c4d434893720a3388749dd6ee36c3adafb7a88e7dfce` |
| Unity 表情采样 | `06f31e0be98472844e61a02758030aeb5c2aa69c38cb7bc4a66531936e048ac0` |

## 开场表情与嘴型回放

正常房间在剧情指令到达时切换表情。嘴型跟随原语音的播放状态。表情和嘴型按原控制器顺序叠加，混合期间按源默认权重补齐缺失目标。未参与混合的目标继续使用身体片段的权重。

任务 1905 以固定帧步回放 631 帧。35,967 项权重与 Unity 对照，最大归一化误差为 0.000014。任务 1906 从常规房间入口跑完教学和第一章。两章均验证眉部表情、语音嘴型和收尾清理。

本机 `story-motion-reference/opening-face/facial-layer/` 保存原生骨架检查、图层输入和回放报告。身体附加层及原版画面配对分别采证。

| 记录 | SHA-256 |
| --- | --- |
| 图层输入 | `beca16548e75de5ab061036545de2289c238e64b03f2eeeb8d7bcf8be007ba3a` |
| 原生固定帧步回放 | `bb38760605e88c814f0930446fc3e4a31da0fb5ca753ea6ff791ee86deb5ac7a` |
| 常规房间回放 | `0339ab6f7221d5fe3ae33ec927bba074539b4ca14a25fc4637336886c06157a0` |

## 身体片段的脸部曲线

装配读取 20 个开场身体片段。其中 18 段携带脸部曲线，共有 955 条有效绑定。六条无目标曲线经源哈希、目标表和零权重核对后登记在清单。装配按源目标顺序写入脸部 GLB，并使用该身体片段的现有时钟。

任务 1898 使用 Unity 分别采样 20 个片段。GLB 与 139,992 项源权重的最大归一化误差为 0.00000451。任务 1899 从常规房间入口跑完教学和第一章，检查七部件、相机、语音和存档。任务 1904 在同一时间点呈现两组脸部姿态，脸部区域有 1,872 个像素变化。

本机 `story-motion-reference/opening-face/body-bindings/` 保存模型、源采样、回放和图片。

| 记录 | SHA-256 |
| --- | --- |
| 模型装配清单 | `ae50bde51eb820a096378bbb36b24e17dc5eda6cf1d56e8ee8209c221083f45e` |
| Unity 片段采样 | `cb9322499bc5edc5f37cfca4c6f493fcaaaedbe4301b7341e17f97847a3e9986` |
| 权重对照 | `aeb481cfc35720221ab1ca5fa67acbd0d99db746c9c44b976eb392ee5a929b8a` |
| 常规房间回放 | `132652365c9b2d6d8fcd20a58a7117b7b4209cefb24b8fc36dbc4caa216f31bd` |
| 原生画面对照 | `6f84771454e637a815567d46cf4194b17eace8e3ea39caf7588a8f0d935bf6dc` |

## 开场视线与身体附加层采样

原 `LookPlayer` 层以 0 到 1 的 `LookScale` 选择片段时刻。原 `plankshake` 层以 0.5 权重循环。任务 1907 使用原 Avatar、两个源片段和八种身体指令，采样 720 帧。每帧记录七种视线强度及身体附加层的单独结果。

视线影响胸、颈、头部旋转和左视线形变。强度为 1 时，颈部最大转角为 8.7795 度。相同强度随身体姿态产生的局部旋转差异最高为 0.8153 度。视线装配按当前人体姿态求值，对照容差为 0.1 度。

本机 `story-motion-reference/opening-face/additive-layers/` 保存源采样。常规房间的附加层装配及叠加画面按各自验收运行。

| 记录 | SHA-256 |
| --- | --- |
| 附加层来源清单 | `da02736dfd412b59b85a2bc4f6aceca1a4404a92b6b88c5df0a33f08d1e28e6c` |
| Unity 附加层采样 | `f78f9e47c738fdcf508b59817d66864abf3d197c38d48502baa1a8173b33a88e` |

## 人体视线旋转口径

任务 1908 对原 Avatar 采样 375 个校准姿态。胸、颈、头使用两轴摆动和一轴扭转。视线在人体通道中叠加，再转换为局部旋转。

按原片段和当前姿态复算 15,120 个旋转。最大角度误差为 0.000041 度，眼部归一化权重误差小于 0.00000011。原生求值及常规入口按后续回放验收。

本机 `story-motion-reference/opening-face/humanoid-basis/` 保存采样和旋转口径。

| 记录 | SHA-256 |
| --- | --- |
| 原 Avatar 采样 | `83447b5a1c50fc7752308e0f2782036ecf139990bf56fff8d8400a10bea1c502` |
| 视线旋转对照 | `7a19d2422bd9d88706bc37afebde4f5fe2f1c916dd901bfb6f8d8fcc506442c2` |

## 原生人体旋转求值

表达式第二版接收校准姿态、三个正交轴、正负通道速率及偏移。引擎从当前局部旋转分解摆动和扭转，再按各轴速率叠加偏移。形变目标可声明加法，保留身体片段已有权重。

任务 1912 使用 Release 工具链验证新旧表达式读写。原生测试对 15,120 个源旋转求值，最大误差为 0.000046 度。受检程序和构建清单位于本机 `story-motion-reference/opening-face/humanoid-basis/native/`。常规房间视线入口见下节。

| 记录 | SHA-256 |
| --- | --- |
| 独立原生测试程序 | `fa8839d530f1835f37f6ba5362ece0d81e5f18192db359974c9b73e47fd2cd16` |
| 构建清单 | `22ed45071baed1855448f819ab767539cc902f5c4dacaa717685561dcc7a5b04` |

## 开场视线回放

教学和第一章按剧情指令缓动视线强度。引擎对六个有目标关节的部件叠加胸、颈和头部旋转。原眼部增量进入表情和嘴型计算前的权重。较上层的已绑定目标使用该层结果。

任务 1920 通过两次正常房间启动。两章均记录六部件视线状态和收尾清理，并通过语音、相机、嘴型和存档回归。构建、图层输入及运行报告位于本机 `story-motion-reference/opening-face/gaze-validation/`。

| 记录 | SHA-256 |
| --- | --- |
| 原生 Player | `5134a7c70c7c61de6834e7fcbb061c21ec24098027e6251c1af0043efa9b0a11` |
| 诊断构建清单 | `8b294c1030275e302ac91d970e076d434dfd76b49fd93c8477cb181b25da34c1` |
| 视线与表情输入 | `7ed64587e1838eae1aefaf98f922baa92f18887586e34cfae0da7223471797e2` |
| 常规房间回放 | `27ab3081003e48fb58a3a37b20ef46f9cdd5358fa3f46b57d3f58d6274b7a0bb` |

## 窗景网格

窗景夹具使用郁金香场景的原网格。
网格有 13,633 个顶点，并保留原几何范围和纹理，已通过独立 Player 加载校验。
场景灯光、风、窗框和切换时序进入窗景验收。

## 形变网格目录

转换器遍历五个场景及 98 个预制体，为 34 个源网格导出 36 个渲染器绑定变体。
其中 34 个变体携带骨骼绑定，两个变体携带独立形变。
单关节顶点使用 Unity 的隐式全权重规则，多关节顶点从原权重数据读取。
全部 36 个 GLB 合计 8,407,268 字节，已通过 Release Player 读取。
[场景校验记录](scene-assets.json)登记清单摘要，动作图和实时姿态按角色专项验收。

## 复验

在 AzureRender 根目录执行命令。
目标文件写入受管任务的新路径。

```powershell
python -B -m unittest discover -s tools/npr_room
python -B -m tools.npr_room.skinned_mesh_export --source $unitySource --scene $unityScene --renderer $rendererFileId --base-color-png $texture --output $targetGlb
python -B -m tools.npr_room.skinned_mesh_export --source $unitySource --scene $unityScene --renderer 9483 --base-color-png $faceTexture --morph-clip $faceClip --morph-path Character/Face --output $faceGlb
python -B -m tools.npr_room.deformer_catalog --source $unitySource --catalog $meshManifest --output $deformerCatalog
python -B -m tools.npr_room.story_face_probe --source $unitySource --project $tutorialProbe --graph $animatorGraph --chapters $openingChapters --model $faceGlb
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod StoryFaceControllerProbe.Run -logFile $faceLog
python -B -m tools.npr_room.story_body_morphs --source $unitySource --room $deskRoom --models $tutorialModels --catalog $bodyCatalog --source-receipt $tutorialSource --output $bodyMorphModels
python -B -m tools.npr_room.story_face_probe --source $unitySource --project $tutorialProbe --graph $animatorGraph --chapters $openingChapters --model $faceGlb --body-morphs $bodyMorphReceipt
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod StoryFaceControllerProbe.RunBodyClips -logFile $bodyMorphLog
python -B -m tools.npr_room.story_face_probe --source $unitySource --project $tutorialProbe --graph $animatorGraph --chapters $openingChapters --model $faceGlb --body-morphs $bodyMorphReceipt --additive-overlays
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod StoryFaceControllerProbe.RunAdditiveLayers -logFile $additiveLayerLog
& $unity -batchmode -nographics -quit -projectPath $tutorialProbe -executeMethod StoryFaceControllerProbe.RunHumanoidBasis -logFile $humanoidBasisLog
python -B -m tools.npr_room.opening_look_basis --source $unitySource --receipt $additiveSource --basis $humanoidBasis --additive $additiveTrace --output $lookBasisAudit --native-fixture $nativeLookCases
& $expressionTest --audit-humanoid $nativeLookCases
python -B -m tools.npr_room.verify_story_body_morphs --models $bodyMorphModels --original $tutorialModels --samples $bodyMorphSamples --output $bodyMorphAudit
python -B tools/npr_room/test_story_body_morph_player.py --player $player --room $deskRoom --resource-root $resources --original $tutorialModels --models $bodyMorphModels --output $bodyMorphRender
& $expressionTest --inspect $bodyMorphFaceGlb $faceSkeleton
python -B -m tools.npr_room.opening_face_layers --source $unitySource --source-receipt $faceSource --graph $animatorGraph --models $bodyMorphModels --skeleton $faceSkeleton --output $faceCatalog
python -B -m tools.npr_room.opening_face_layers --source $unitySource --source-receipt $faceSource --graph $animatorGraph --models $bodyMorphModels --skeleton $faceSkeleton --look-basis $lookBasisAudit --look-source-receipt $additiveSource --expression-inspector $expressionTest --output $gazeCatalog
python -B tools/npr_room/test_story_face_player.py --player $player --room $deskRoom --resource-root $resources --models $bodyMorphModels --catalog $faceCatalog --source-trace $faceTrace --output $faceReplay
```

转换支持单个渲染器的绑定层级。
形变目标采用单帧、全权重的变形输入，片段转换支持目标渲染器的直接曲线。
原网格引用和部件身份通过 Recovery 的文件 ID 核对。
