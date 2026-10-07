# 引擎复用工作流

> 文档类型：使用教程
> 适用范围：Windows x64、Release 安装树

## 准备安装

先完成[构建与运行](../getting-started.md)。
使用完整 Release 构建生成接口快照。
安装树包含工具、资源、文档与许可。
示例安装目录为 `D:/AzureEngine`。

```powershell
cmake --build build/ninja-msvc-release --config Release
cmake --install build/ninja-msvc-release --config Release --prefix D:/AzureEngine
python D:/AzureEngine/share/AzureRender/tools/write_engine_contracts.py --verify --output D:/AzureEngine/share/AzureRender/contracts
```

## 制作探索项目

使用空目录创建项目并进入编辑器。
关卡、角色和任务规则归项目内容。
输入、相机、交互和脚本通过配置组合。
编辑步骤见[关卡制作教程](editor-first-game.md)。

```powershell
& D:/AzureEngine/bin/AzurePlayer.exe --create-game D:/Games/Exploration
& D:/AzureEngine/bin/AzureRender.exe --editor-project D:/Games/Exploration/project.azureproject
```

在编辑器中制作对象并验证撤销与重做。
保存后重开项目，核对数据和资产引用。
通过 Build Game 面板发布独立游戏。
发布条件见[游戏发布](../runtime/game-publishing.md)。

## 使用场景检视项目

仓库示例位于 `assets_public/scene_inspector/`。
该项目通过交互和脚本标注场景对象。
它使用公共生命周期与组件访问契约。
专用检视行为归项目脚本及装配配置。

在工程目录发布检视包。
输出目录须与项目和安装输入分别存放。
目录名称中的空格通过路径引号保留。

```powershell
python tools/build_game.py --project assets_public/scene_inspector/project.azureproject --install D:/AzureEngine --output "D:/Games/Scene Inspector"
& "D:/Games/Scene Inspector/start-game.cmd"
```

两种项目均可移到其他路径验证资源定位。
新增机制先声明类型、配置和生命周期。
扩展通过注册及公共服务进入宿主。
装配规则见[运行系统装配](../runtime/system-composition.md)。
