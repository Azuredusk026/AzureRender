# 角色与窗景资源转换

> 状态为 P1 局部验收。角色部件通过 Player 资产读取。

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
