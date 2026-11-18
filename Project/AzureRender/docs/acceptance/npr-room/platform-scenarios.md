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

组装常规房间时使用 `tutorial_camera_body stage --window-settings-inventory $storyInventory` 接入显示面板和原版文本。从房间左下角打开图形设置，可切换窗口模式、选择分辨率和设置置顶。使用普通窗口时显示分辨率设置。手动调整窗口会将分辨率记为自定义。设置保存在版本为 1 的 `room-window-settings` 存档中。写入成功后应用选项。交换链重建期间保留当前界面及控件状态。`--window-settings` 可组装采用内置英语文本的房间。

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

## 显示设置语言

显示面板和 Windows 系统菜单使用与房间相同的 Recovery 语言清单，包含窗口模式、分辨率、置顶、帧率与垂直同步文本。语言选择生效时同步更新面板与菜单。组装清单 `window-settings-receipt.json` 记录文本来源、计时器脚本与面板资产的摘要。

任务 1990 在常规入口读到默认简体中文，依次切换日语、英语、繁体中文、葡萄牙语、韩语和俄语，逐项核对 Windows 菜单标题与本地语言存档。俄语下执行系统菜单置顶开关、分辨率调整和无边框切换，并保存 1600×900 的面板截图。第二个 Player 恢复俄语和置顶。任务 1991 验证内置英语文本与窗口流程。

本机证据位于 `platform-achievement-reference/window-preferences/localized/`。

| 记录 | SHA-256 |
| --- | --- |
| 原始文本清单 | `20021f5f8e2087916b99ce98cac00ec5ae408faf9c4283db74b27e42e72d6381` |
| 七种语言回放 | `6ccf444b8ab6f2118d8c444d834affe5eee115b9bbf6a582a1f71d0c3cdc354d` |
| 内置英语回归 | `493791fe6b42d742be70ea28941b576b0ecef73e13cb79d1f77e87a6c04a02e1` |
| 俄语显示面板截图 | `a6bb40d85f6ac0b0ea56f892e58e171671cfc62e34859be1e68073623b46c018` |
| 原生诊断构建清单 | `28ef30394eab0004034961eac32076bad509ccd07a3fee7c8de2cec7a53873f6` |

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $localizedWindowReplay --system-menu --display-inventory $storyInventory
```

## Windows 前后台帧率

从房间图形设置输入前台和后台帧率，按回车或移开输入焦点后保存。两项按原场景限制为 10–250 FPS 的整数，Windows 默认值为 60 和 24。版本为 1 的 `room-frame-rates` 保存两个值。重复打开面板时显示已保存的值。

可见且位于系统前台的窗口采用前台帧率，其余状态采用后台帧率。显式配置 `runtime.maxFrameRate` 时使用该值。固定步长采集沿用采集配置。 [Win32 前台窗口接口](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getforegroundwindow?WT_mc_id=SEC-MVP-5005282)

任务 1997 使用三个独立 Player 进程核对常规入口、存档重开和命令行覆盖。首个进程由回车保存 20 FPS，由焦点转移保存 10 FPS。前台、隐藏后台各连续观察 16 帧，分别耗时 0.859 秒与 1.735 秒。第二个进程读回两项限制，第三个进程采用显式的 15 FPS 并保留房间存档。三个进程退出后的 GPU 缓冲区和图像计数均为零。Lua 回归覆盖越界整数、非整数和写入失败。

任务 1998 通过中文多行笔记输入与重开回归。任务 1999 通过七种语言菜单、置顶和窗口切换回归。

本机证据位于 `platform-achievement-reference/window-preferences/frame-pacing/`。

| 记录 | SHA-256 |
| --- | --- |
| 原版 `SettingData.cs` | `b1eb6de749848fab7b1cf512dcefbcf8d806a860ad3577976f091cb613d9e002` |
| 原版 `ClampIntegerInputField.cs` | `beb91c8c3128fbca0ddf0fa2491273994bae249b7aef0eaabe320338831ff928` |
| 帧率回放 | `37185b0b2578f573e78d2bf2ab1ff6aa5e99dce492a1d814fe865bf6abba81de` |
| 帧率面板截图 | `0fcf4bd36f620e111c8db00ae4a04ffc29bde6fd64032e57cd281df185401d6e` |
| 笔记输入回归 | `9983f3d735b24e976dbc908ddd822c092ca33fe3135d14a43efb320516e4f3ee` |
| 窗口语言回归 | `ea9299d1e5a28c483a772d528ca43cefba0ee89345aa5665457b1ce918cd8439` |
| 原生诊断构建清单 | `ca79508c13e078bb95c711108bc66ded1d22925ddfc310c734c6a550abdbb914` |
| 诊断 Player | `86698b6b327107f5b2cedd81587df628ae13e65689fb0820b92f99ac4d8f8010` |

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $framePacingReplay --frame-pacing
```

## Windows 屏幕休眠

房间遵循 Recovery Windows 的屏幕休眠默认值。组装时将 `screenSleepMode` 设为 1，运行期间持有 Windows 的显示器电源请求。值 0 由系统电源设置决定显示器待机。离开房间或退出时释放请求。 [Windows 显示器电源请求](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powersetrequest)

任务 2002 从常规入口进入房间，确认 Windows 接受显示器电源请求。切换到系统默认待机关卡后，应用释放请求。返回房间时再次取得请求，退出后的 GPU 缓冲区和图像计数均为零。任务 2003 通过前后台帧率、设置重开和覆盖回归。系统电源请求列表需要提升权限，屏幕实际待机时刻可通过独立系统观察复验。

本机证据位于 `platform-achievement-reference/window-preferences/display-sleep/`。

| 记录 | SHA-256 |
| --- | --- |
| 原版 `ScreenSleepModeExtensions.cs` | `226dca8842d1375dc625c58ac08df66d18762d629b572783ffae348f9f22ead4` |
| 显示器电源请求回放 | `240b12957ba869e3eb59b4c289d2dc319fd5b77b8a2009e22d8b754686a8de8e` |
| 前后台帧率回归 | `cc002802a2d63105ee3a9cee3316a35f7ef979c7e94e1f7d56e3f6176e84ce92` |
| 原生诊断构建清单 | `7ca8a80b581fb9c9f4bcb612c974016e7889c32684cb0419422109a2abb90127` |
| 诊断 Player | `24453af807b790ac97d05b14288c65dd92f7f38091177787ba96fb559c49dd16` |

```powershell
python -B tools/npr_room/test_room_display_sleep_player.py --player $player --project $roomProject --resource-root $resources --output $displaySleepReplay
```

## Windows 垂直同步

房间显示面板的垂直同步默认关闭，状态保存在版本为 1 的 `room-vsync` 存档。开启时使用 Vulkan FIFO 呈现，由显示器刷新节奏控制。关闭时优先采用 MAILBOX，设备只提供 FIFO 时沿用该呈现方式，并采用已配置的前后台帧率。显式配置 `runtime.vsync` 和 `runtime.maxFrameRate` 时分别使用该值。 [Unity 帧率与垂直同步](https://docs.unity3d.com/2022.3/Documentation/ScriptReference/Application-targetFrameRate.html)

任务 2005 通过三个独立 Player 回放。首个进程从 MAILBOX 切换至 FIFO，再切回 MAILBOX。第二个进程恢复已保存的 FIFO 与显示器节奏。第三个进程使用显式的 MAILBOX 和 15 FPS 覆盖，保留房间存档。三个进程退出后的 GPU 缓冲区和图像计数均为零。Lua 回归覆盖写入失败和较新存档版本。

任务 2006 在同一构建中通过七种语言的系统菜单、分辨率、无边框模式及置顶回归，保存俄语显示面板截图。

本机证据位于 `platform-achievement-reference/window-preferences/vsync/`。

| 记录 | SHA-256 |
| --- | --- |
| 垂直同步回放 | `a4a4ecc23eda992d8056a485ad730540247da9e4382f888520f640fe7ccdf552` |
| 窗口语言回归 | `3b5c0b8c3e1fd0428decec3f34b4c908486a911d127b6d8dcb884dd3f20b700b` |
| 原生诊断构建清单 | `1d207d0e5f1f3c19020a3ad01280be13d5f8159372365025126b6f8b491b6956` |
| 诊断 Player | `e179f387919016015a1ffe8f6050f2dd7c34b81aca27e2cda4e45185aed39d1d` |

```powershell
python -B tools/npr_room/test_room_vsync_player.py --player $player --project $roomProject --resource-root $resources --output $vsyncReplay
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

## Windows 独立包预检

本机房间候选的 `assets` 挂载导出 1,248 个文件，总大小为 493,907,772 字节。启动关卡为 `assets:/boot.azurelevel`。发布器随源资源复制 `.azmeta` 旁车，诊断临时文件的旁车留在源目录。静态检查覆盖 325 条显式挂载引用、202 个模型和 7 个界面资源引用。各引用可从挂载内解析，模型使用内嵌资源。

空间预约使用同一候选的资源挂载与 Release x64 安装树。已盘点的房间挂载与一份历史安装树的着色器、公开资源及许可证合计 928,466,092 字节。正式出包还需按当前安装树计入 Player、启动器、动态库和验证临时文件。

发布器在运行项目校验前核对原生启动器。包含已验证房间 Player 的 Release x64 安装树齐备后，执行整包生成及移动后启动验收。预检的输入与程序摘要见本机 `full-acceptance-next-gates/windows-package-inputs-20261118.json`。
