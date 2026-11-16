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
python -B -m tools.npr_room.deformer_catalog --source $unitySource --catalog $meshManifest --output $deformerCatalog
```

转换支持单个渲染器的绑定层级。
形变轨道使用单帧、全权重的形变输入。
原网格引用和部件身份通过 Recovery 的文件 ID 核对。
