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
```

转换支持单个渲染器的绑定层级。
形变目标采用单帧、全权重的变形输入，片段转换支持目标渲染器的直接曲线。
原网格引用和部件身份通过 Recovery 的文件 ID 核对。
