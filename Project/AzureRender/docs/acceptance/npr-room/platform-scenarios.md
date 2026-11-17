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

玩家在紧凑窗口启动正计时，在 1920×1080 窗口结束，记录 3 秒。第二个 Player 读到同一模式和计时记录，并能打开笔记。两个进程退出后，GPU 缓冲区和图像计数均为零。本次使用房间验证构建；无边框全屏、窗口置顶、前后台帧率、休眠和独立安装包各自采证。

本机存档目录为 `room-opening-playable/validation/windows-lifecycle/`。回放报告 SHA-256 为 `49e09993f741c1e86ca48c9b4ed02107c5c0540508c9484fbd21cd05c312fec7`，Player SHA-256 为 `efbc5a1d27c9b3db0cb91695dff770c4dfaa1de5f66f4d529e14a89fa44898c6`。

```powershell
python -B tools/npr_room/test_room_window_player.py --player $player --project $roomProject --resource-root $resources --output $windowReplay
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

回放报告位于本机 `platform-achievement-reference/local-validation/`。报告 SHA-256 为 `a0ffc5d5f74229f78d9886a45154bd14092e19ada9545092dbd6cea89a2504c0`。记录使用本地夹具，常规房间事件接线及 Steam 真实统计仍需分别验收。

```powershell
python -B tools/npr_room/test_achievement_store_player.py --player $player --resource-root $resources --output $localAchievementReplay
```
