# Windows 房间家具导入

> 状态为 P1 局部验收通过。验证范围为桌椅、贴图和椅子采样片段。

## 来源与转换

来源为 Recovery 的 `RoomScene` 中 `Platform_PC` 子树。
测试包保留桌椅与其祖先的七个节点。
两项资源使用项目内稳定槽位 `desk` 和 `chair`。

转换工具读取 Unity 内联网格。
坐标在 Z 轴反射，三角面顺序随之转换。
网格各子网格保留独立材质槽。
原 PNG 按槽位嵌入基本颜色。

桌面网格含 919 个顶点和 531 个三角面。
椅子网格含 5,306 个顶点和 4,978 个三角面。
PNG 字节随 GLB 导入渲染。
完整原材质与灯光按画面对照阶段验收。

## 片段对照

椅子 GLB 含 `anim_wild_window_1` 的平移与旋转轨道。
轨道使用原构建取样记录的 67 个时刻。
转换后的最大角度误差为 0.000005° 以下。
位置误差为零，最大时间舍入差约 0.00000048 秒。

此结果覆盖实际取样点和 GLB 的加载。
轨道间插值、播放时序和房间动作联动继续验收。
原参考的角度容差为 0.1°，位置容差为 0.0005。

## 生产路径

| 检查 | 结果 |
| --- | --- |
| 生产编辑器 `scene.import` | 七个节点和两项资源进入关卡 |
| 不存在的模型引用 | 生产操作拒绝，关卡节点数保持一致 |
| 保存、重开与重复导入 | 节点标识、父级和平移通过对账 |
| Player 项目校验 | 两项 GLB 和保存的关卡均通过 |
| Release Player 独立运行 | 16 帧，脚本和表现诊断均为空 |
| 退出释放 | 帧缓冲与图像资源计数均归零 |

GPU 批次为共享队列任务 1548。
验证使用主工作树的 Release 编辑器和 Player。
二进制和候选产物摘要见[验收清单](furniture.json)。
原资产、GLB 和项目夹具留在本机受管目录。

## 工具入口

转换环境使用 Python 3.11 和 PyYAML 6.0.3。
在 AzureRender 根目录运行以下命令。

```powershell
python -B -m unittest discover -s tools/npr_room
python -B -m tools.npr_room.mesh_export $unityMesh $targetGlb --base-color-png $unityBaseColor
```

每个材质槽按顺序传入一个 PNG。
椅子采样可按 `--motion-reference` 和 `--clip` 选择。
输出使用受管任务内的新路径。

计时输入、原角色、窗景、音频及存档使用[城市窗景与房间集成](city-window.md)的工程复验。
