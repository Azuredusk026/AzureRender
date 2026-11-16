# 角色与窗景资源转换

> 状态为 P1 局部验收。角色部件通过 Player 资产读取。

角色转换器读取 `SkinnedMeshRenderer` 的骨骼引用。
骨骼层级沿场景父级关系装配。
GLB 保留关节顺序、反向绑定矩阵和每顶点权重。
场景和网格使用同一个 Z 轴反射。

角色夹具包含脸、身体、头发、上衣、下装和袜子。
每项均携带原基本颜色纹理。
脸含 57 个形变通道，头发含 66 个关节和两个形变通道。
于同一坐标场景中计算的角色高度约为 1.67 米。

Release Player 通过六项资源及角色关卡校验。
GPU 预览经共享队列执行。
实际造型、角色动作及同步时间以原版对照为准。

窗景夹具使用郁金香场景的原网格。
网格有 13,633 个顶点，并保留原几何范围。
场景灯光、风、窗框和切换时序进入窗景验收。

## 复验

在 AzureRender 根目录执行命令。
目标文件写入受管任务的新路径。

```powershell
python -B -m unittest discover -s tools/npr_room
python -B -m tools.npr_room.skinned_mesh_export --source $unitySource --scene $unityScene --renderer $rendererFileId --base-color-png $texture --output $targetGlb
```

转换支持单个渲染器的绑定层级。
形变轨道使用单帧、全权重的形变输入。
原网格引用和部件身份通过 Recovery 的文件 ID 核对。
