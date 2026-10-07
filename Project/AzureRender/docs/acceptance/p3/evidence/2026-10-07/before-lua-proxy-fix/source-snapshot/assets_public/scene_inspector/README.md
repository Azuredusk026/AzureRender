# 场景检视项目

本项目使用共享轨道相机与普通资产交互。运行系统和按键由项目配置声明。两个方块具有通用交互组件和 Lua 脚本。公共模型许可见 `assets/ASSET-LICENSE.txt`。

从工程目录执行：

```powershell
./build/ninja-msvc-release/AzureRender.exe --editor-project assets_public/scene_inspector/project.azureproject
./build/ninja-msvc-release/AzurePlayer.exe --project assets_public/scene_inspector/project.azureproject
```

制作时将项目复制到可写目录。编辑器可选择资产、修改属性、保存并重开。点击 Play 运行项目，按 F 选择最近的资产。选择后该资产的交互关闭，下次选择另一资产。

鼠标旋转相机，滚轮调整距离。Esc 释放鼠标，点击视口恢复控制。Stop 恢复编辑文档。重开关卡恢复资产的可选状态。
