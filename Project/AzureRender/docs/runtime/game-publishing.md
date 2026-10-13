# Windows 游戏项目与发布

> 文档类型：使用说明
> 适用范围：Windows x64、Release、项目 Schema v1
> 更新日期：2026-10-12

## 创建与编辑

双击 `bin/AzureRender.exe` 进入项目中心。
中心支持新建项目、打开文档和最近项目。
完整 GUI 步骤见[项目中心指南](../tutorials/project-center.md)。

游戏模板包含两个关卡、角色控制脚本、触发器、动画、界面和音效。项目使用公开资产，按挂载路径访问文件。每次创建生成独立项目标识和资产侧车。

```powershell
& "<引擎安装目录>/bin/AzurePlayer.exe" --create-game "D:/Games/MyGame"
& "<引擎安装目录>/bin/AzureRender.exe" --editor-project "D:/Games/MyGame/project.azureproject"
```

AzureRender 同样提供 `--create-game <空目录>`。编辑器的 Build Game 面板提供模板创建目录。新目录必须为空，创建失败时清理候选目录。

模板角色通过 WASD 移动，空格跳跃，进入传送区域后切换关卡。界面 Pause 按钮控制暂停，P 恢复运行，O 在暂停时单步推进。动画由角色运动状态驱动，传送和目标关卡触发音效。

## 准备引擎安装树

发布使用 Release x64 安装树内的工具与 Python。
安装树包含编辑器、Player、游戏入口和依赖 DLL。
着色器、公开资源、字体和第三方许可随包分发。
独立游戏的运行依赖为 Windows 与 Vulkan 驱动。

```powershell
cmake --build "<Release 构建目录>"
cmake --install "<Release 构建目录>" --config Release --prefix "D:/AzureEngine"
```

`share/AzureRender/runtime-build.json` 记录引擎版本、实际构建配置、平台和架构。构建工具检查该记录，并要求 Release x64。完整发布门禁见 [开发与发布](../development-and-release.md)。

## 构建游戏

编辑器处于编辑态时，打开 Build Game 面板。
填写引擎安装目录和独立的游戏输出目录。
点击 Build Windows game，保存关卡并启动后台构建。
面板显示真实阶段、结果与耗时。

面板提供输出目录和日志入口。
日志保存在项目 `.azure/game-build.log`。
缺少构建工具或 Python 时，任务返回错误。
任务结束后恢复场景编辑。

构建期间锁定场景编辑。已有游戏包可通过 Replace existing game package 选项替换。目标目录必须与项目和引擎输入目录分离。

也可在终端执行安装树内的构建工具：

```powershell
& "D:/AzureEngine/share/AzureRender/tools/python/python.exe" `
  "D:/AzureEngine/share/AzureRender/tools/build_game.py" `
  --project "D:/Games/MyGame/project.azureproject" `
  --install "D:/AzureEngine" --output "D:/Delivery/MyGame"
```

工具先验证关卡、组件、模型和资产依赖，再复制到相邻候选目录。候选 Player 再次验证项目，成功后提交目录。替换使用备份目录，目录提交失败时恢复已有结果。普通用户目录发生冲突时保留原内容。

## 包结构与运行

| 路径 | 内容 |
| --- | --- |
| `<项目名称>.exe` | 原生游戏入口，读取包内启动描述 |
| `game-launch.json` | 包内项目、Player 与运行参数 |
| `start-game.cmd` | 使用包内相对路径启动 Player |
| `bin/` | Player 和 Release DLL |
| `game/project.azureproject` | 独立项目配置 |
| `game/<挂载名>/` | 关卡、脚本、模型、侧车及其他项目资源 |
| `share/AzureRender/` | 着色器、公开资源、字体、许可证与构建元数据 |
| `game_manifest.json` | 项目标识、启动入口、版本与文件大小和 SHA-256 |
| `GAME-GUIDE.md` | 项目提供的操作与任务说明 |
| `QUALITY.json` | 项目提供的质量配置与启动分辨率 |

分发整个游戏目录，双击项目命名的 exe 运行。
Monica 项目的入口为 `Monica.exe`。
入口依据自身位置加载包内 Runtime 与项目。
目录支持移动，以及中文和空格路径。

运行时在可写目录生成缓存和诊断记录。
启动失败的日志为 `captures/game-startup.log`。

```powershell
& "D:/AzureEngine/share/AzureRender/tools/python/python.exe" `
  "D:/AzureEngine/share/AzureRender/tools/build_game.py" --verify "D:/Delivery/MyGame"
```

清单验证检查不可变文件，并允许 `game/.azure/` 缓存和 `captures/` 诊断输出。发布复制规则排除缓存、临时文件和版本控制目录。私有资产、捕获与作品集目录触发发布边界错误。游戏包包含原生入口、Player 及其运行依赖。

项目根目录可提供 `GAME-GUIDE.md` 与 `QUALITY.json`。构建复制这两份普通文件，并登记 SHA-256。质量说明的 `width` 与 `height` 为 1 至 8192 的整数。启动脚本使用该分辨率，命令行参数可覆盖它。

探索项目的操作说明包含移动、相机、交互、任务和重开。质量说明包含四级 2048 阴影、完整材质与目标帧率。公开标准项目使用 100 实体、四角色与八灯。

## 验收与能力边界

引擎安装树提供 `tools/ai_bridge.py` 与 `tools/ai/`。
编辑器按配置启用 Python 模型服务。
Python 路径和凭据由部署环境提供。
独立 Player 使用其运行模块装配。

内容提案经制作流程应用后成为项目资产。
生成资产的来源元数据进入发布核验。
模型配置的具体接入见[内容提案](content-proposals.md)。

`AzureEngine.GamePackageRules` 覆盖目录事务、哈希、配置与发布边界。Release 的 `AzureEngine.GamePackage` 从实际编辑器构建包，移动目录并移走输入。它在隔离 PATH 下运行角色、切关、声音、界面和动画，并检查退出释放。

`AzureEngine.PlayablePackage` 验证完整探索关卡。实际编辑器构建包后，测试移动目录并移走自建输入。隔离 PATH 下完成全部任务与两次重开。随后执行 20 次切关和 20 次重开，核验驻留与退出释放。

长跑使用包内 Player 与真实时钟。工具正常关闭自有进程窗口，并检查完整运行时间。运行期间只执行一个 GPU 验收任务，源码保持固定。

```powershell
python tools/test_playable_package.py `
  --build-dir build/ninja-msvc-release `
  --editor build/ninja-msvc-release/AzureRender.exe `
  --output "build/delivery/Azure Exploration" `
  --evidence build/delivery/package-evidence

python tools/run_playable_long_run.py `
  --package "build/delivery/Azure Exploration" `
  --output build/delivery/long-run --seconds 1800
```

长跑覆盖最小化、恢复、960×540 与 1920×1080 切换。预热 120 秒后比较稳定资源和工作集。全部 GPU CSV、周期资源记录与每分钟窗口进入报告。CPU 详细样本保存最后 8192 帧。

常规 CPU 百分位使用每 240 帧的固定间隔记录。切关额外记录按提交门禁单独检查。主线程额外耗时 P99 至多为 8 ms，提交帧 P99 至多为 33.3 ms。GPU 百分位使用全部有效帧和每分钟窗口。

原始采样与分析分别记录源码指纹。复核使用 `--reanalyse --provenance <构建关联清单>`。工具核验运行时源码、Player 和游戏包哈希。原始数据保持完整，复核报告记录其 SHA-256。

短时探针使用 `--probe --seconds 60`，结果登记为探针。正式验收要求连续运行至少 1800 秒。完整预算与结果见[可玩关卡性能与加载](playable-performance.md)和 [P1 验收](../acceptance/p1/2026-10-05.md)。

探索发布包的后继平台使用实时路线长跑。
工具为 `tools/run_platformer_long_run.py`。
它核验跳跃、承载、金币、恢复与重复通关。
命令和覆盖见[平台长跑入口](../acceptance/p5/closure/platformer-long-run.md)。

游戏表现遵循 [项目编辑与游戏界面](editor-game-ui.md)的动画和界面能力边界。模板使用公开测试模型展示玩法链路。Android 状态为 Deferred。

发布工具由项目自行实现，复用 AssetDatabase、Level、安装许可和 Player。Python 标准库承担文件复制、进程调用和 SHA-256 计算。
