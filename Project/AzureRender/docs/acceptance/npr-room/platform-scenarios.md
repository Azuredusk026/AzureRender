# 平台场景与验证条件

> 已登记 P0 平台场景，并通过 P6 本机窗口回归及成就夹具回放。平台实机结果按应用构建和测试身份单独归档。

移植基线为 Recovery Windows 1.17.3。平台采证可使用 [Steam 1.18.1 参考构建](steam-reference.md)。[源接口清单](platform-scenarios.json)保存相关程序集类型和导出文件摘要。

| 场景 | 入口与源行为 | 验证条件 |
| --- | --- | --- |
| Windows 显示与窗口 | `SettingService` 管理全屏、分辨率及前后台帧率。 `ScreenSystem` 管理窗口置顶，`ScreenSleepModeExtensions` 管理休眠策略。 | 本机 Windows 与独立配置档。记录切换、失焦、最小化、恢复及重开后的状态。 |
| 本地音乐与图像 | `FileBrowser` 提供文件多选和文件夹递归筛选。音乐筛选 WAV、MP3、FLAC、OGG、M4A，图像筛选 JPG、JPEG、PNG。 | 使用独立目录中的有效媒体、空目录和损坏样本。记录取消、过滤、加载失败和重开。 |
| Steam 启动与扩展内容 | `SteamManager` 负责初始化、回调和已安装扩展内容查询。 | 可用性、失败与恢复使用本地测试接口。Steam 客户端重启路径和实际扩展资格使用已授权测试会话。 |
| Steam 成就统计 | `SteamAchievements` 读取并更新用户统计，写入成功后提交统计。初始化或统计写入失败时返回失败状态。 | 本地测试覆盖不可用、读失败和写失败。真实统计变更和重开后的统计状态使用独立授权身份。 |
| 游戏账户 | `LoginFlow` 按设备标识与教程进度决定是否进入登录，处理首次同意、取消、既有账户、维护和账户状态变更。 | 本地响应夹具覆盖状态与取消。真实首次注册、已有账户和绑定使用独立授权身份。删除与迁移场景使用专用可丢弃档案。 |
| 游戏账户存档同步 | `SaveDataSync` 通过游戏发行方接口下载文件列表及存档，上传变更与删除项。登录流程调用迁移和快照恢复。 | 本地夹具覆盖上传与下载、断网、重试及失败后的恢复。真实读写使用两个受控环境及同一授权测试档，记录文件身份与同步结果。 |
| Steam 客户端云存档 | 实机核对应用的云存档能力、实际同步目录和客户端状态。 | 需要同一构建及授权测试账号的两端同步记录，或可核对的同版本实机记录。Steam 安装文件只用于核验构建身份。 |
| 消息与图片 | `LoginFlow` 更新消息提示，`ImageContent` 按来源加载本地资源或网络图片并处理加载错误。 | 本地夹具覆盖提示、过期内容、取消、连接失败和损坏图片。发行方消息的真实返回按实际服务记录登记。 |

窗口和本地媒体场景使用本机测试档。服务夹具仅记录本地执行结果。外部服务记录包含应用构建、脱敏测试身份、执行步骤和结果；成就统计、游戏账户同步与 Steam 客户端云存档分别归档。

## Windows 窗口回归

任务 1858 从项目的启动关卡进入常规房间。测试进程使用按显示器感知的 DPI 上下文读取实际像素。客户区依次为 1280×720、960×540、恢复后的 960×540 及 1920×1080；各阶段的 Vulkan 截图尺寸与客户区一致。系统报告窗口进入最小化状态并成功恢复。房间关卡在每次操作后继续出帧，并保持相同修订号；加载、脚本和呈现错误均为空。

玩家在紧凑窗口启动正计时，在 1920×1080 窗口结束，记录 3 秒。第二个 Player 读到同一计时模式和记录，并能打开笔记。两个进程退出后，GPU 缓冲区和图像计数均为零。前后台帧率、休眠和独立安装包各自采证。

本机存档目录为 `room-opening-playable/validation/windows-lifecycle/`。回放报告 SHA-256 为 `49e09993f741c1e86ca48c9b4ed02107c5c0540508c9484fbd21cd05c312fec7`，Player SHA-256 为 `efbc5a1d27c9b3db0cb91695dff770c4dfaa1de5f66f4d529e14a89fa44898c6`。

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $windowReplay
```

## Windows 启动显示设置

`runtime.windowMode` 采用窗口模式 0 或无边框全屏 1。`runtime.windowResolution` 使用 0、1、2 选择 1280×720、1600×900、1920×1080，或使用 3 保留自定义窗口尺寸。`runtime.alwaysOnTop` 接受布尔值。启动时通过 `--settings-file` 加载设置，命令行的同名 `--set` 优先。无边框窗口使用当前显示器范围，窗口模式恢复自定义位置、尺寸和边框。

Win32 探针任务 1972 核对三档客户区像素、自定义窗口往返、窗口边框及置顶状态。任务 1974 核对设置类型、范围和加载顺序。任务 1976 使用三个独立 Player 进程进入常规房间，分别核对 2560×1440 无边框置顶、命令行覆盖为 1600×900 普通窗口、以及重开后的无边框置顶。两次截图尺寸与客户区一致，三个进程退出后的 GPU 缓冲区和图像计数均为零。

任务 1977 使用相同构建通过普通窗口的尺寸调整、最小化、恢复和计时存档重开回归。

证据位于本机 `platform-achievement-reference/window-preferences/`。

| 记录 | SHA-256 |
| --- | --- |
| 原版 `SettingService.cs` | `8b3019e51eb2bf85462e4c7e39ceaac3679a9e64d4ccf28da4dc474c488d00bb` |
| 原版 `ScreenSystem.cs` | `da6a4f88abc06e6dd249c5a474131dc19de231409961cb72c10f870a5819fe84` |
| 常规房间回放 | `137034fc77488cbe62ef8aa2b354c61dd31e8761e0e4f03cc7dc9c947d74fe16` |
| 普通窗口回归 | `142a4e4fcd541b50c6eacd1e8af1744e33985443f4cee59cd9193acb1166ee5e` |
| 原生诊断构建清单 | `5ce8eaf6633e6fa34ec0bd08c4da0010c30d3f0e9f2ea6f6422dcba131814da6` |

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $windowPreferencesReplay --preferences
```

## 房间显示面板

组装常规房间时使用 `tutorial_camera_body stage --window-settings` 接入显示面板。从房间左下角打开 Display，可切换窗口模式、选择分辨率和设置置顶。使用普通窗口时显示分辨率设置。手动调整窗口会将分辨率记为自定义。设置保存在版本为 1 的 `room-window-settings` 存档中。写入成功后应用选项。交换链重建期间保留当前界面及控件状态。

任务 1982 从普通窗口进入房间，从 1280×720 切至 1600×900 并启用置顶。面板保持打开，实际截图为 1600×900。无边框窗口覆盖当前 2560×1440 显示器，返回普通窗口时恢复 1600×900。将客户区手动改为 1100×700 后存档记录自定义分辨率。第二个 Player 恢复无边框置顶，并能在面板关闭置顶。两个进程退出后的 GPU 缓冲区和图像计数均为零。

任务 1983 核对普通窗口调整尺寸、最小化、恢复及计时存档。任务 1984 核对启动设置、命令行覆盖和独立进程恢复。房间 Lua 回归覆盖设置写入失败及较新存档版本。

本机证据位于 `platform-achievement-reference/window-preferences/panel/`。

| 记录 | SHA-256 |
| --- | --- |
| 面板回放 | `0d434a79ca9e945878e60b6e6ade4024132360d299c226119b421c45c98b0519` |
| 普通窗口回归 | `eeb8fb9f0c5f1d6bbc393a2a69a5200b7e3ce97459c660d4901987b0fab8d54f` |
| 启动设置回归 | `01788dd10b6252faa7acf14fcdd83104843aff04a5db632513ddb36683ee1b0e` |
| 房间截图 | `dab286fcdf449bc4399e3ad3ffe155430f4c903b871411f051524397e84b9cd5` |
| 原生诊断构建清单 | `82e7725cfb10128d1ee81dc73c3dff036e914fbca8330133b862b01743939bee` |
| 诊断 Player | `4362a8616436e5195f9cdf3ef780bafc03715b7c65d861b852ca2f4b0cd372dd` |

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $windowPanelReplay --settings-panel
```

## Windows 系统菜单置顶

房间普通窗口的系统菜单首项使用原版命令编号 8191。选择后切换置顶并保存房间显示设置。菜单标题显示下一步可执行的操作。英语验证使用 `Always on Top(On)` 与 `Always on Top(Off)`。

任务 1987 通过两个独立 Player 回放。显示面板关闭时，从系统菜单开启和关闭置顶，两次均核对 Windows 扩展样式、菜单标题和本地存档。随后通过显示面板、无边框窗口及设置重开回归。两个进程退出后的 GPU 缓冲区和图像计数均为零。

本机证据位于 `platform-achievement-reference/window-preferences/system-menu/`。

| 记录 | SHA-256 |
| --- | --- |
| 系统菜单及面板回放 | `d2c218d0c9306d60a3959f083dff700505302efded3d03f3e63547823fdc09e4` |
| 原生诊断构建清单 | `428d3de1efc4789cf4bac3df6361a7539dba6916e59bd8a7c5c477d9d0bc2bc5` |
| 诊断 Player | `b1339c7394d730305432a46f77d23bd20c4dff92cfd57a82998e1351107013ff` |

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $windowSystemMenuReplay --system-menu
```

## 成就统计夹具

`SteamAchievements` 在平台初始化后写入统计。增加进度时先读当前值；写入成功后更新本地缓存，再提交统计。是否已持久化以重开后读到的结果为准。

`AchievementService` 从教学、章节和本地事件记录恢复一次性成就。小时统计采用已完成工作时长的整小时数。房间记录使用版本为 1 的 `room-achievements` 存档。

| 来源 | SHA-256 |
| --- | --- |
| `SteamAchievements.cs` | `396b1a6bb6b8a0cc8e0e3cb2a5846017e0a8e2fb4216a31ea08cbcd117e98296` |
| `AchievementService.cs` | `39f0ca193fde0e89ee375d93317db6ec7c109ae386bfbb747ae70e785e6c3811` |
| `AchievementNoticeService.cs` | `e5adede9775a7a49bae770aff5f216c0e5976ac402f3976e6c99fb7deac132a8` |

Lua 回归注入初始化、读取、写入和提交失败。任务 1878 在四个独立 Player 中测试待办成就。离线触发只写房间记录；提交失败时可读到缓存值。第三次启动补交成功，第四次读回统计且未重复写入。

回放报告位于本机 `platform-achievement-reference/local-validation/`。报告 SHA-256 为 `a0ffc5d5f74229f78d9886a45154bd14092e19ada9545092dbd6cea89a2504c0`。

```powershell
python -B tools/npr_room/test_achievement_store_player.py --player $player --resource-root $resources --output $localAchievementReplay
```

## 房间成就记录

教学通关、首次完成待办、提交便签正文和结束计时都写入本地成就存档。教学进度、待办、便签和工时存档各自提供恢复依据。重开时恢复缺失事件，每项只记录一次。便签标题修改、待办重开使用各自的房间行为。

任务 1927 使用两个原生 Player 进程，从启动关卡进入常规房间。首个进程完成教学、完成并重开待办、保存便签。第二个进程读回三项记录，结束正计时并完成第一章。最终存档包含四条事件。Lua 回归覆盖待办与便签的主存档写入失败，以及三项房间工具完成后成就存档写入失败的重开恢复。

本机回放位于 `platform-achievement-reference/room-validation/`，SHA-256 为 `0a360397a00f0bbf4982364818bf168e23319c6dab859287947aa57126c9a874`。房间事件使用本地存档，Steam 统计、云存档和账户场景使用授权测试会话另行采证。

```powershell
python -B -m tools.npr_room.tutorial_camera_body stage --room $deskRoom --catalog $bodyCatalog --controller $animatorGraph --chapters $openingChapters --source-receipt $tutorialSource --models $bodyMorphModels --camera-trace $cameraTrace --camera-audio-source $unitySource --face-catalog $plankCatalog --achievements --output $roomCandidate
python -B tools/npr_room/test_story_signal_room_player.py --player $player --room $deskRoom --resource-root $resources --catalog $bodyCatalog --graph $animatorGraph --chapters $openingChapters --source-receipt $tutorialSource --models $bodyMorphModels --camera-trace $cameraTrace --camera-audio-source $unitySource --face-catalog $plankCatalog --achievements --output $roomAchievementReplay
```
