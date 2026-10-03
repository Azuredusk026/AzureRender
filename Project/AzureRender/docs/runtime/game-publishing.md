# Windows 游戏项目与发布

> 文档类型：使用说明
> 适用范围：Windows x64、Release、项目 Schema v1
> 更新日期：2026-10-04

## 创建与编辑

游戏模板包含两个关卡、角色控制脚本、触发器、动画、界面和音效。项目使用公开资产，按挂载路径访问文件。每次创建生成独立项目标识和资产侧车。

```powershell
& "<引擎安装目录>/bin/AzurePlayer.exe" --create-game "D:/Games/MyGame"
& "<引擎安装目录>/bin/AzureRender.exe" --editor-project "D:/Games/MyGame/project.azureproject"
```

AzureRender 同样提供 `--create-game <空目录>`。编辑器的 Build Game 面板提供模板创建目录。新目录必须为空，创建失败时清理候选目录。

模板角色通过 WASD 移动，空格跳跃，进入传送区域后切换关卡。界面 Pause 按钮控制暂停，P 恢复运行，O 在暂停时单步推进。动画由角色运动状态驱动，传送和目标关卡触发音效。

## 准备引擎安装树

发布需要 Release x64 安装树，以及构建机上的 Python 3.11+。安装树包含 Player、依赖 DLL 和 MSVC 运行库。着色器、公开资源、字体和第三方许可随包分发。独立游戏的运行依赖为 Windows 与 Vulkan 驱动。

```powershell
cmake --build "<Release 构建目录>"
cmake --install "<Release 构建目录>" --config Release --prefix "D:/AzureEngine"
```

`share/AzureRender/runtime-build.json` 记录引擎版本、实际构建配置、平台和架构。构建工具检查该记录，并要求 Release x64。完整发布门禁见 [开发与发布](../development-and-release.md)。

## 构建游戏

编辑器处于编辑态时，在 Build Game 面板填写引擎安装目录和游戏输出目录。点击 Build Windows game 后保存当前关卡并启动后台构建。面板显示结果与耗时，日志保存在项目 `.azure/game-build.log`。

构建期间锁定场景编辑。已有游戏包可通过 Replace existing game package 选项替换。目标目录必须与项目和引擎输入目录分离。

也可在终端执行安装树内的构建工具：

```powershell
python "D:/AzureEngine/share/AzureRender/tools/build_game.py" `
  --project "D:/Games/MyGame/project.azureproject" `
  --install "D:/AzureEngine" --output "D:/Delivery/MyGame"
```

工具先验证关卡、组件、模型和资产依赖，再复制到相邻候选目录。候选 Player 再次验证项目，成功后提交目录。替换使用备份目录，目录提交失败时恢复已有结果。普通用户目录发生冲突时保留原内容。

## 包结构与运行

| 路径 | 内容 |
| --- | --- |
| `start-game.cmd` | 使用包内相对路径启动 Player |
| `bin/` | Player 和 Release DLL |
| `game/project.azureproject` | 独立项目配置 |
| `game/<挂载名>/` | 关卡、脚本、模型、侧车及其他项目资源 |
| `share/AzureRender/` | 着色器、公开资源、字体、许可证与构建元数据 |
| `game_manifest.json` | 项目标识、启动入口、版本与文件大小和 SHA-256 |

分发整个游戏目录，双击 `start-game.cmd` 运行。目录可移动，也支持含空格的路径。运行时在可写目录生成缓存和诊断记录。

```powershell
python "D:/AzureEngine/share/AzureRender/tools/build_game.py" --verify "D:/Delivery/MyGame"
```

清单验证检查不可变文件，并允许 `game/.azure/` 缓存和 `captures/` 诊断输出。发布复制规则排除缓存、临时文件和版本控制目录。私有资产、捕获与作品集目录触发发布边界错误。游戏包只分发 Player 可执行文件。

## 验收与能力边界

`AzureEngine.GamePackageRules` 覆盖目录事务、哈希、配置与发布边界。Release 的 `AzureEngine.GamePackage` 从实际编辑器构建包，移动目录并移走输入。它在隔离 PATH 下运行角色、切关、声音、界面和动画，并检查退出释放。

游戏表现遵循 [项目编辑与游戏界面](editor-game-ui.md)的动画和界面能力边界。模板使用公开测试模型展示玩法链路。Android 状态为 Deferred。

发布工具由项目自行实现，复用 AssetDatabase、Level、安装许可和 Player。Python 标准库承担文件复制、进程调用和 SHA-256 计算。
