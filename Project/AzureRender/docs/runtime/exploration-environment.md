# 探索路线与环境制作

> 文档类型：设计与操作说明
> 范围：公开探索项目与本机角色副本

## 路线与尺度

项目位于 `assets_public/exploration/`。
起点的引导角色通过 E 开始任务。
三个遗物分别位于道路两侧的短支路。
收集完成后打开蓝色灯光标记的大门。

遗物位于 z 为负十八、负四十五和负七十四米处。
检查点位于负五十八米，大门位于负九十四米。
大厅目标位于负一百零八米。
设计路线总长一百三十八米。

步行速度为每秒两米，冲刺为每秒五米。
通道净宽为 4.6 米，主路宽四米。
坡道提供地形观察与跳跃练习。
短支路使用金色遗物与石柱共同标记。

## 环境与材质

地面、沙土路、岩石、植被和残柱形成层次。
远景山体位于可行走地面的两侧。
门墩、门楣及蓝光帮助辨识终点方向。
实际素材由本机 Blender 生成。

生成器使用固定种子 `260910`。
模型采用 glTF，地面尺寸与碰撞体一致。
Blender 的 Z 轴转换为运行时的 Y 轴。
生成素材使用原创 CC0 内容。

颜色贴图使用 sRGB。
法线贴图为线性切线空间，采用正 Y 方向。
ORM 的绿色表示粗糙度，蓝色表示金属度。
材质通过现有 PBR 采样路径加载。

## 生成与运行

在工程目录执行：

```powershell
python tools/generate_exploration_environment.py --output build/my-exploration --blender E:/Blender/blender.exe
build/ninja-msvc-release/AzurePlayer.exe --project build/my-exploration/project.azureproject
```

`--character` 可指定包含 idle、walk 的本机模型。
生成结果保存资源、规则、设计与操作说明。
生成清单记录脚本哈希、工具版本和输出哈希。
目标目录中的其他文件由其所属工作流维护。

## 验证

```powershell
python tools/test_exploration_completion.py --executable build/ninja-msvc-release/AzurePlayer.exe --project assets_public/exploration/project.azureproject --output build/exploration-qa --capture
```

验证使用键盘输入完成任务与两次重开。
报告记录位置、检查点、遮挡、冲刺和生命周期。
截图检查地形朝向、地标、比例与大厅构图。
阶段证据见[G15 验收](../acceptance/g15/2026-10-10.md)。
