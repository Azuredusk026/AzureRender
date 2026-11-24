# 移植基线与验收范围

> 交付状态为 P0 进行中。静态目录与 Windows 产物已锁定。

## 参考身份

Recovery 工程版本为 Unity `2022.3.62f2`。
本机 Windows 播放器为 Recovery `1.17.3`。
项目使用 Mono、URP、线性色彩空间和 D3D11。
[基线清单](baseline.json)记录播放器和目录摘要。

启动场景依次登记为 `Boot`、`AssetLoad`、`Entry`、`RoomScene` 和 `Transition`。
`BootBehavior` 叠加加载 `AssetLoad`，再初始化音频和项目作用域。
`EntryBehavior` 加载存档及音乐，并预加载房间。
`SceneLoadHandle` 负责切换过渡画面和激活房间。

现有实机记录使用 Recovery 独立存档身份。
运行路径以 `Nestopi/Chill With You Recovery` 为末段。
AzureRender 验收使用独立测试档。
参考存档保留于 Recovery 的证据目录。

## 资产清单

五场景的序列化引用覆盖 3,053 个文件。
合并目录和 `Resources` 候选后共有 5,123 个文件。
导出工程共索引 7,180 个带 GUID 的文件。
未纳入候选的文件仍保留待定状态。

Addressables 有 3,687 个键和 1,845 个加载位置。
包含 12 个资源包、1,568 个包内位置、260 个 `Resources` 位置和五个场景位置。
`MasterData` 标签包含 20 个表。
导出目录与 Windows 目录的摘要一致。

资源依据精确路径、导出文件名和资源键映射。
音频的 WAV 目录路径可对应导出的 OGG。
完整私有清单登记原键、提供器、依赖、GUID 和文件 ID。
文件名映射仍需在转换时核验类型及负载身份。

场景闭包的 GUID 缺失数和重复数均为零。
附加候选有一处缺失，来自 `SRDebugger` 设置。
Recovery 记录了相同 GUID 及缺少的程序集。
运行时按加载路径核对该项是否需要移植。

## 动态加载入口

Recovery 应用显式调用 Unity `Resources` 或 Addressables 初始化和加载的入口共有七处。
启动流程从 `AssetLoadHolder` 获取作用域设置，从 `MasterData` 标签加载 20 个表。
内置消息含一个本地图像键，其 Sprite 加载位置与同名纹理位置分别登记。
`WebEnv` 运行配置和三个环境档均可按键对账。
项目作用域的两个 `Resources` 备用键在导出目录中无对应文件。场景直接持有 `AssetLoadHolder` 与 `RoomLifetimeScope`，Recovery 独立运行已验证从 Boot 到达 RoomScene。备用分支的可达性按故障路径复验。

Windows 构建的 `StreamingAssets` 有 18 个实际文件，涵盖 12 个 Addressables 资源包、来源着色器包和启动配置。清单登记相对路径、大小和摘要。
本机私有清单 `source-lock/dynamic-loads.json` 的 SHA-256 为 `7d6da7d097057dc5491893881c8932dfac46c9ca6f50a7b952664ddb44e253ce`。

### 主数据加载对账

`MasterData` 标签的 20 个位置逐一对应 `MasterDataLoader` 的类型分支。清单记录每项的源 GUID、路径和摘要，并核对 `EntryBehavior` 等待 `IsLoaded` 的入口。候选的 `work-progression-receipt.json`、`music-receipt.json` 和 `ambient-receipt.json` 分别匹配工时参数、音乐和环境主表的源摘要。其他主表的消费者与运行覆盖按玩法继续对账。

本机 `master-data-audit/master-data.json` 两次生成一致，SHA-256 为 `dc4be3ab32a0c751273421dd61c3cd2a402b990daf3ed1b3d2e4a4dd70eab074`。

消费清单记录加载器写入的字段及带 `MasterDataLoader` 类型声明的读取位置。20 个主表均有对应读取位置，共核对 103 组字段与源文件。候选装配可追溯工时参数、音乐和环境三份直接记录，以及剧情组、剧情命令、本地化、装饰解锁和环境解锁五张派生表。派生表通过源摘要、生成文件摘要及房间装配摘要核对。

本机 `master-data-audit/master-consumers.json` 两次生成一致，SHA-256 为 `50a183798053b6e41c9588cd0a29c0cbe9e02a48c1b489dbd50e3039589f4346`。各消费者的实际运行范围按对应玩法验收。

```powershell
python -B -m tools.npr_room.master_data_inventory --source $unitySource --inventory $referenceInventory --project $roomProject --output $masterDataReport
python -B -m tools.npr_room.master_data_inventory --source $unitySource --inventory $referenceInventory --project $roomProject --locks $sourceLock --output $masterConsumerReport
```

### 音效主表与候选挂载

`TimerSeMaster` 的 7 个条目和 `SystemSeMaster` 的 12 个条目均有原枚举、GUID、文件 ID 和源摘要。完整房间验证 6 个挂载，购买候选验证 7 个挂载。计时默认三条音效和购买音效的 OGG 字节与源文件一致。确认、取消和完成待办所用的 WAV 副本通过长度字段和原 PCM 样本核验。其余音效的界面入口与播放状态按对应玩法验收。

本机 `sound-cue-audit/playable.json` 与 `sound-cue-audit/purchase.json` 各复验两次。SHA-256 分别为 `b9844e319d8ef38a1ab5179a92c98ee46aa0fe8970974a6a6060268ed9ed6ac2` 与 `98982d1e48c10b097cf78d7db4d9bb02310116de94981e42633ec4db0615e79b`。

```powershell
python -B -m tools.npr_room.sound_cue_inventory --source $unitySource --inventory $referenceInventory --room $roomDirectory --audio-repairs $audioRepairReport --output $soundCueReport
python -B -m tools.npr_room.sound_cue_inventory --source $unitySource --inventory $referenceInventory --room $purchaseRoomDirectory --audio-repairs $audioRepairReport --output $purchaseCueReport --purchase
```

## 功能验收矩阵

表内的静态来源位于 Recovery 导出工程。
输入列用于建立原版和 Player 的同条件回放。
本机参考证据只用于表中对应的行为。

| 项目 | 前置状态与输入 | 输出与持久数据 | 参考来源与证据 | AzureRender 接入 |
| --- | --- | --- | --- | --- |
| 启动与转场 | 空白测试档启动，分别执行正常进入、重复进入、取消及退出 | 主数据和音频就绪，激活房间，错误路径可恢复 | `BootBehavior`、`EntryBehavior`、`SceneLoadHandle`。已有启动进房证据 | 关卡预加载、UI 与项目状态 |
| 番茄钟 | 设置工作和休息时间，开始、暂停、恢复、跳过及完成 | 工作和休息交替，提示音、进度和工作时长一致 | `TimerCoreService`、`PomodoroTimerService`。已有倒计时、暂停恢复与重置证据 | 固定步脚本、项目动作、音频 |
| 正计时 | 切换模式，开始、暂停、恢复与结束 | 已用时间及统计入档，切换后设置可恢复 | `CountupTimerService`、`TimerCoreService`。等候实机回放 | 固定步脚本、保存服务 |
| 笔记 | 新建、输入、保存、重排、删除及重开 | 文本和排序持久化，保持焦点及键盘操作 | `NoteList`、`NoteUIContentsView`。已有面板证据 | [房间笔记](notes.md)登记独立 Player 输入、存档重开与写入失败恢复 |
| 待办 | 新建多列表，添加、完成、取消完成、拖动和删除 | 列表归属及任务顺序持久化 | `FacilityTodo`、`TodoAllData`。已有面板证据 | [房间待办](todo.md)登记本地编辑、排序、音效和重开 |
| 统计与解锁 | 完成工作并跨日，按条件开启剧情和环境 | 统计、等级、解锁、通知及进度入档 | `StatisticsService`、`UnlockConditionService`。等候实机回放 | 领域事件、可控时钟、保存服务 |
| 剧情 | 首次进入、满足解锁、选择分支及读档 | 台词、动作、场景及选择状态一致 | `ScenarioReader`、`ScenarioGroupMaster`、`NovelMaster` | [工后短对话](work-conversation.md)登记组内分支及读档续接 |
| 音乐与环境声 | 切曲、调整混音、暂停、淡化及断开音频设备 | 播放位置、列表和设置持久化，设备失败可恢复 | `MusicService`、`AmbientMaster` 和音频主数据 | [原版曲库](music-playlist.md)和[环境声](ambient-sound.md)登记独立 Player 操作及设置回放 |
| 窗景 | 满足解锁条件，切换天气、时段及自动模式 | 同步灯光、云、天气、音轨和设置 | `WindowViewService`。已有 29 种场景的隔离渲染证据 | 场景资源、材质和时间驱动 |
| 装饰和角色 | 解锁装饰，切换摆设，触发计时和剧情动作 | 资源槽、挂点、姿态、表情和房间偏好一致 | `DecorationService`、`HeroineAI`。椅子采样可复用 | Prefab、动画和运行事件 |
| 界面和显示 | 三档分辨率，切换语言、窗口尺寸和设置 | 界面布局、字体、焦点和图形设置一致 | `SettingData`、`UiResolutionAcceptance`。已有三档布局证据 | RmlUi 布局、文本与平台窗口 |
| 退出与存档 | 保存、退出、重开，检查损坏及并发实例 | 版本校验、失败恢复及存档隔离生效 | `SaveDataManager`、`SaveDataIO`。已有正常退出和存档身份记录 | 应用存储、错误与实例策略 |
| 平台与账户 | 测试身份登录、离线与掉线，触发成就和同步 | 成就、消息、账户流程及同步结果明确 | `LoginFlow`、`SteamAchievements`、`SaveDataSync`。真实服务待验 | 平台适配与本地实现 |
| 消息与外部内容 | 打开内置消息，触发远端消息及带图内容 | 内置和远端消息的阅读状态一致 | `ImageContent`、`NewsMaster`。远端输入待验 | 本地资源与授权服务 |

剧情主表包含 610 个组和 2,289 条命令。
命令值共有 44 种，按实际使用语义迁移。
[剧情命令与跳转](story-routes.md)登记 90 个组内目标、选项区间、日期分支和循环台词入口。
本地化主表含 2,111 条记录。
Windows 的 `SettingUI` 绑定七种语言按钮，覆盖简繁中文、日语、英语、巴西葡萄牙语、韩语和俄语。
剧情与本地化主表提供七种语言的读取分支。
Windows Addressables 目录另登记六种界面字符串表，葡萄牙语的实际界面回退行为由运行回放确认。

剧情清单保留原始顺序及 26 个重复命令 ID。
组内选项和跳转使用首个匹配项。
清单登记三个空组、两条组外命令和六条未解析的文本引用。
组外命令与回退行为进入动态验证。

主数据有 46 个环境及其解锁条目。
`WindowViewType` 定义 29 种窗景表现。
装饰表有 29 个模型项和 99 条解锁记录。
音乐表有 43 项，环境持续声 30 项，环境音效 56 项。

这些数量来自表记录或枚举。
剧情组受触发时机、内容条件和平台分支约束。
各组的可达状态随回放核验后登记。

## 画面与动作对照

| 组别 | 固定输入 | 比较与证据 |
| --- | --- | --- |
| 原版房间 | 高质量、最大渲染比例、1280×720 设置。保留天气、场景和镜头 | 原版 `capture-manifest.json` 记录逻辑像素缩放。裁剪和配准通过后使用静态阈值 |
| 窗景 | 单独固定窗景、相机、时间、光线、天气及粒子条件 | Recovery `WindowMatrixIsolated` 有 29 张图。时间相位、种子及配对帧需补采 |
| 界面 | 1280×720、1920×1080、1280×800。固定语言、缩放和面板 | `ui-resolution-result.json` 和三档截图提供布局依据 |
| 椅子 | 固定片段、时间、资产来源和姿态 | `chair-continuity-after-result.json` 覆盖 71 个片段和 2,467 个采样点 |
| 角色 | 固定图层、场景动作、表情、道具及光照 | 须补关键姿态、转场及事件序列 |
| 后处理 | 固定场景、曝光、色彩空间及输出区域 | 稳定区域使用 SSIM ≥ 0.95，亮度中位误差 ≤ 10% |

原版截图的已知尺寸随采集策略变化。
现有原图清单用于选机位和核查构图。
[窗景参照采集](window-references.md)登记 29 种隔离窗景和 13 张原版实机画面的实际编码、尺寸和摘要。
严格像素对照需原始尺寸与同步渲染输入。
椅子角度容差为 0.1°，位置容差为 0.0005。

## 平台验证条件

[平台场景与验证条件](platform-scenarios.md)登记 Windows 窗口、文件选择、Steam 成就、游戏账户、同步和消息使用的源接口及测试条件。
真实成就、云存档和账户按授权身份与实际版本单独验收。

恢复工程登记版本为 Unity `2022.3.62f2`。
已找到的本机同系列编辑器为 `2022.3.62f1`。
固定版本验收可使用已构建的 Recovery Player。
需要原编辑器采样的项目以匹配版本为复验条件。

## 实施依赖与估算

| 阶段 | 关键输入 | 初步人日 |
| --- | --- | --- |
| P0 | 可达剧情分支、运行加载记录和配准输入 | 2–4 |
| P1 | 家具、角色、片段、窗景和首条独立回放 | 3–6 |
| P2 | 可重复转换、资源槽及五场景对账 | 6–12 |
| P3 | 应用状态、剧情解释器、界面和存档 | 10–20 |
| P4 | 动画控制图、挂点、事件及场景联动 | 8–16 |
| P5 | 帧合成、固定输入和双设备资料 | 8–20 |
| P6 | 服务身份、离线故障回放和可移动包 | 4–8 |
| P7 | 冻结候选、全量证据和正式采样 | 4–8 |

预计空间由首批模型、动画、音频和包分别实测。
当前目录包含约 480 MiB 的 12 个 Addressables 包。
持久媒体由摘要去重，工具按资源组写入私有目录。
动作图、项目 UI 事件和持久化接口是候选引擎缺口。

## 复验命令

在 AzureRender 根目录执行。

```powershell
python -B -m unittest discover -s tools/npr_room -p "test_*inventory.py"
python -B -m unittest discover -s tools/npr_room -p "test_catalog_locations.py"
python -B tools/npr_room/reference_inventory.py --recovery $recovery --output $localInventory
python -B tools/npr_room/story_inventory.py --source $unitySource --output $localStory
python -B -m tools.npr_room.dynamic_load_inventory --source $unitySource --inventory $localInventory --recovery $recovery --output $localDynamicLoads
```

输出指向项目受管目录中的新文件。
基线验证包括目录、引用、候选键和 Windows 载荷摘要。
Debug 与 Release 各通过四项导入和动画注册用例。
上述短检使用主工作树的现有二进制。

当前待项为逐条可达回放、动态资源加载、配准输入及授权平台实测。
后续阶段以实际回放和原资产身份填写验收记录。
