# AzureRender 游戏完整移植计划

> 状态为实施中。以本机 Recovery 的 Windows 版本为参照。
> 当前完成静态目录核对。功能可达性和动态证据继续验收。

[移植基线与验收范围](../acceptance/npr-room/baseline.md)登记本机参考与各验收项。

## 目标与交付

在 AzureRender 中完整复现《Chill with You: Lo-Fi Story》的可达游戏逻辑、资产和美术效果。首轮交付是随原素材运行的本机 Windows x64 版本。它应由 AzureRender 编辑器制作，由独立 Player 启动并跑完整流程。

按五个场景和运行时实际依赖建立迁移清单。可达内容覆盖完整剧情、角色、房间、计时工具、进度、音乐、设置与存档。界面、动画和渲染效果通过原版及 Recovery 的证据验证。预加载和场景过渡按实际时序复现。

原素材和由其转换的产物保存在 D 盘私有目录。资源槽与游戏逻辑通过稳定标识绑定，为后续自制素材保留替换契约。完整移植验收使用原资产版本。

此项目承担 FYP 的引擎工程验收。论文实验矩阵和现有引擎画质、性能门禁按各自计划执行。 [引擎开发总计划](azure-engine-plan.md) · [冻结画质回归](2026-11-13-frozen-public-visual-regression.md) · [论文实验负载](../../experiments/thesis/docs/workload.md)

## 参考基线

移植资产与行为按 Recovery Windows 1.17.3 锁定。
[Steam 参考版本](../acceptance/npr-room/steam-reference.md)登记 1.18.1 的平台参照和内容差异。

| 项目 | 已核实的本地依据 |
| --- | --- |
| 源工程 | Unity `2022.3.62f2`，Windows Mono、URP 与 D3D11。构建入口为 `Boot`、`AssetLoad`、`Entry`、`RoomScene` 和 `Transition`。 |
| 内容规模 | 恢复报告登记 98 个预制体、496 个动画片段、27 个动画控制器、258 个材质、586 个图像文件和 1,554 个音频文件。P0 用静态引用与实际加载确定交付分母。 |
| 角色与道具 | 角色控制器有 390 个状态、504 条转场、5 个图层。椅子现有参考覆盖 71 个片段和 2,467 个取样点。 |
| 画面 | 原构建恢复包有 113 个着色器及 8 个计算着色器。可用窗景参照有 29 种。界面布局有 1280×720、1920×1080、1280×800 三档证据。 |
| 业务入口 | 启动与房间见 `BootBehavior`、`EntryBehavior`、`RoomGameManager`。行为与数据见 `TimerCoreService`、`ScenarioReader`、`HeroineAI`、`MasterDataLoader`、`SaveDataManager`。 |
| 音频与环境 | 音乐、装饰、窗景入口为 `MusicService`、`DecorationService` 和 `WindowViewService`。现有证据含计时、笔记、待办、音频采样和独立启动。 |

[Recovery 恢复报告](<D:/Unity/Unity Project/Recovery/README.md>) · [工程实现导读](<D:/Unity/Unity Project/Recovery/UnityProject/ExportedProject/docs/project-summary.md>) · [角色动画说明](<D:/Unity/Unity Project/Recovery/UnityProject/ExportedProject/docs/animation-module.md>) · [原版画面采集](<D:/Unity/Unity Project/Recovery/Evidence/OriginalGameCapture>)

## 工作区与工程归属

- 从可审阅的集成提交创建 `codex/npr-room-reproduction`。拟用工作树 `D:/Projects/FYP-worktrees/npr-room-reproduction`。开工时核对 Git、改动归属、活动进程和可用空间。制定必要的检出范围后再创建工作树。
- 项目源码拟放在 `Project/AzureRender/examples/npr-room/`。应用玩法、界面、关卡与资源映射进入独立项目。公共缺口补在所属引擎模块，按仓库规则验证另一种独立使用方式。
- 项目按实际需要组织启动与房间、玩法脚本、动画、界面和平台适配。清单记录逻辑标识、Unity GUID 与文件 ID、目标槽位、转换方式、来源摘要和损失。私有路径及可还原媒体清单留在本机。
- Recovery 源目录保持只读。派生资源使用 `Project/AzureRender/assets_private/npr-room-dev/` 下的一份持久存储。开发项目用生产支持的挂载方式装配。正式本机包仅从完整且经过核验的原资产挂载组装。
- 持久资产按内容摘要去重。运行目录复用资产路径。临时转换、截图和日志进入受管任务。写入前核对单任务 2 GiB、受管池 16 GiB 和卷余量。预计超额时先分批、去重并调整产物布局。
- 现用 Debug、Release 和安装树按源码身份复用。独立工作树使用与候选匹配的构建配置。重编译与 GPU 采集通过共享队列登记。 [目录维护](../../tools/skills/azurerender-artifact-maintenance/SKILL.md) · [共享资源排队](D:/Projects/FYP-coordination/PARALLEL_DEVELOPMENT.md)

## 实现入口

| 能力 | 采用入口 |
| --- | --- |
| 项目、关卡与发布 | 以现有 `.azureproject`、`.azurelevel` 和独立发布为入口。 [项目制作](../tutorials/editor-first-game.md) · [Windows 发布](../runtime/game-publishing.md) |
| 模型与场景 | 使用 `scene.import`、GLB 与场景交换实现。场景交换包使用现行版本和包内相对路径。 [格式解析](../../src/assets/scene/SceneExchange.cpp) · [导入事务](../../src/editor/scene/SceneImportService.cpp) |
| 动画与脚本 | 使用现有蒙皮、Morph、混合、固定步和反射字段。 [动画](../runtime/third-person-animation.md) · [Lua](../runtime/scripts-gameplay.md) |
| 界面与声音 | 复用 RmlUi、`azure.game-ui` 和 `azure.audio-source`。现行宿主的 `data-action` 处理包含全局暂停和重载。通过生产输入和脚本事件校验项目动作缺口。 [游戏界面](../runtime/editor-game-ui.md) · [宿主代码](../../src/app/AzureRenderGameplay.cpp) |
| NPR 与灯光 | 复用配置驱动的色带、光照、阴影、透明和后处理能力。以实际画面差异确定通用扩展。 [色带](../runtime/npr-ramp-composition.md) · [光照](../runtime/npr-illumination.md) |

## P0 基线、清单与验收数据

[窗景参照采集](../acceptance/npr-room/window-references.md)登记 29 种窗景和原版画面证据。
[剧情命令与跳转](../acceptance/npr-room/story-routes.md)登记原命令编号、选项区间和组内跳转目标。
[环境与装饰解锁](../acceptance/npr-room/unlock-catalog.md)登记源条件、点数价格和截止时间。
[平台场景与验证条件](../acceptance/npr-room/platform-scenarios.md)登记操作系统能力、外部服务及各场景的采证条件。
[功能矩阵](../acceptance/npr-room/feature-matrix.md)登记七类源条目、各项验证条件和已有运行证据。

- [x] 锁定 Recovery 的源版本、构建、存档口径和原版证据。逐项读取五个场景、主数据、Addressables、`Resources`、`StreamingAssets` 和动态加载入口。先复用参考目录的现有统计、引用索引和资源身份。
- [ ] 建立功能矩阵。每行登记入口、前置状态、输入、预期输出、存档影响、源证据和引擎缺口。得出剧情章节、窗景、装饰、解锁、语言、设置与平台分支的实际分母。
- [ ] 建立资产依赖清单。从场景、预制体、剧情和资源键沿静态引用展开，再用运行记录覆盖动态引用。记录模型、材质、动画、字体、图集、音频和配置。不可达项须有引用或运行证据。
- [ ] 建立画面与动作对照矩阵。登记镜头、曝光、输出色彩空间、分辨率、原资产身份、动画时间、窗景、随机种子及采样容差。以原版采集为首选，标注使用 Recovery 补充的项目。
- [x] 列出外部服务、操作系统能力、测试身份与可达场景。区分参考已确认行为和待采证行为；决定哪项可用本地测试源，哪项需要合法的真实服务身份。
- [ ] 按盘点结果给出阶段人日、预计占用、关键路径及共享引擎改动。以可验证交付的依赖排序，随后直接推进 P1。提交 `feat(p0): 固定完整移植的基线与验收范围`。

首轮估算以 P0 分母为准。最先验证导出保真、动画图语义、原后处理复现和外部服务这四类风险。

## P1 原资产集成路线

[家具导入](../acceptance/npr-room/furniture.md)记录 Windows 子树的导入和播放宿主短检。
当前已核对原家具、贴图及椅子片段采样。
[角色与窗景资源转换](../acceptance/npr-room/character-assets.md)登记角色关节、说话表情片段及窗景网格。
[房间计时回放](../acceptance/npr-room/room-probe.md)登记生产输入、原音效、关卡就绪与偏好存档重开。
[城市窗景与房间集成](../acceptance/npr-room/city-window.md)登记带原城市窗景的房间、角色与存档复验。

- [x] 以原房间家具、角色、一个片段和一种窗景验证资产入口。记录比例、手性、共享实例、材质槽、骨架映射和关键姿态。验证 `scene.import` 拒绝坏引用，以及保存重开后标识与变换保持有效。
- [x] 实现启动到房间的首条路径。接入真实计时输入、一次角色相位切换和音频。保存、退出、重开，并在独立 Player 中回放。这一阶段验证各跨模块入口能在同一项目工作。
- [x] 按已测缺口确定 GLB 转换或原曲线烘焙路线。需要 FBX 时仅作中间格式。验证 Humanoid 骨架限制和替代导出结果。 [Unity FBX 导出说明](https://docs.unity.cn/Packages/com.unity.formats.fbx@4.2/manual/exporting.html)
- [x] 原资源阻塞时记录最小复现与重试条件。允许夹具先验证通用契约。原素材的验收项保留在矩阵中，待真实输入复核。提交 `feat(p1): 打通原资产房间与独立运行`。

## P2 场景和资产管线

[原音频目录与资源槽](../acceptance/npr-room/audio-catalog.md)登记本机音频来源、播放副本和原生解码结果。
[静态网格目录](../acceptance/npr-room/static-meshes.md)登记 Windows 场景的合批、内置网格及生产编辑器导入。
[角色与窗景资源转换](../acceptance/npr-room/character-assets.md)登记形变网格变体与 Player 校验。
[纹理与源字体目录](../acceptance/npr-room/image-catalog.md)登记位图、精灵、字体资源槽和剧情字符覆盖。
[原材质目录](../acceptance/npr-room/material-catalog.md)登记源参数、着色器、纹理和渲染器槽位。
[房间启动与转场](../acceptance/npr-room/startup-flow.md)登记五关卡顺序、取消、重进与模型修复后的同进程重试。

- [ ] 按五场景的入口与实际加载关系实现启动、预加载、房间和转场。复现正常进入、重进、取消、退出与失败恢复。区分持续服务、场景实例和共享资源的所有权。
- [ ] 批量转换运行可达的网格、贴图、材质参数、动画、界面图集、字体和音频。保留预制体、实例覆盖和动态资源键。用稳定标识关联替换槽。记录单位、坐标、压缩、颜色空间和循环点。
- [ ] 验证分批导出可重试、同源重复运行稳定、资源共享和缺失诊断。检查字体轮廓、字形覆盖、图集透明边距及音频通道。先完成同类资产，再集中校验该类引用。
- [ ] 用清单对账所有可达资源。聚合已映射、已校验和待修复数量。达到全引用覆盖且保存重开后通过引用检查，提交 `feat(p2): 接入全场景与原资产依赖`。

## P3 玩法、界面与存档

[正计时](../acceptance/npr-room/countup.md)登记模式切换、暂停、结束及独立 Player 重开。
[房间笔记](../acceptance/npr-room/notes.md)登记中文输入、页面编辑和独立 Player 重开。
[房间待办](../acceptance/npr-room/todo.md)登记多列表、任务顺序、完成时间和音效。
[工时统计](../acceptance/npr-room/work-calendar.md)登记按日累计、跨日分配和重开对账。
[工时奖励与等级](../acceptance/npr-room/work-progression.md)登记原参数结算、存档升级和退出恢复。
[音源播放控制](../acceptance/npr-room/audio-transport.md)登记原版音乐的流式读取、暂停、音量调整和切曲回放。
[房间曲库](../acceptance/npr-room/music-playlist.md)登记原曲与默认曲目范围、播放控制及本地设置恢复。
[房间环境声](../acceptance/npr-room/ambient-sound.md)登记原音频编号、独立混音和本地设置恢复。
[角色空闲语音](../acceptance/npr-room/room-voice.md)登记原语音、字幕和播放完成时序。
[工后对话与选项](../acceptance/npr-room/work-conversation.md)登记短对话触发、组内分支及独立进程续接。
[环境声解锁与购买](../acceptance/npr-room/ambient-access.md)登记原条件、原价、扣点、写入失败及房间重开。
[房间语言设置](../acceptance/npr-room/room-language.md)登记七种语言的计时器、对白、原字体与深路径存档回放。
[计时语音](../acceptance/npr-room/timer-voice-sources.md)登记原语音、选择规则及动作提示点。
[计时语音提示点实机对照](../acceptance/npr-room/timer-voice-cues.md)登记番茄钟与正计时的正式语音库、原生回放和起声时刻。
[教学与第一章剧情命令](../acceptance/npr-room/opening-story.md)登记原命令、章节解锁、原语音和字幕切换。常规启动路线的原生回放覆盖首次教学、升至等级 2 后进入第一章，以及教学重播音乐。

- [ ] 从 `TimerCoreService` 复现计时、工作休息循环、通知和暂停恢复。覆盖输入重复、焦点变化、切换阶段、退出重开和默认配置。界面、键盘和自动回放共享业务动作。
- [ ] 实现笔记、待办、统计、进度、解锁、成就及设置。按源状态验证增删改、排序、解锁事件和持久状态。存档采用独立 D 盘根、版本校验与原子提交。检查损坏文件回退、版本升级和并发实例行为。
- [ ] 按 `ScenarioReader` 与主数据复现章节条件、文本、表情、选项及完成状态。逐条映射实际剧情命令、参数和触发条件。以可复验回放覆盖首次启动、解锁、分支和读档续接。
- [ ] 实现音乐播放列表、环境声、语音和界面音效。验证播放状态、音量、淡化、切曲、循环、暂停和无设备处理。保留原音频与时序参照。
- [ ] 重建全部可达界面、动效、焦点和文本输入。校验三档已知分辨率和本机安装的目标语言。以真输入检查三档布局、窗口缩放与设定恢复。提交 `feat(p3): 复现完整玩法界面与本地存档`。

P3 按功能组实施与验收。完成一个功能组后更新矩阵，再继续最高优先级的未覆盖功能组。

## P4 角色、道具与房间状态

[城市窗景开关与恢复](../acceptance/npr-room/city-window.md#窗景开关与恢复)登记实际购买后的画面联动和两种开关方式。

[人体动画采样](../acceptance/npr-room/character-assets.md#人体动画采样)登记原 Avatar、两段逐帧动作及七个角色部件的同步回放。
[计时动作采样](../acceptance/npr-room/timer-voice-cues.md)登记 16 种原版计时动作的七部件变换和实机提示点。
[教学与第一章角色动作](../acceptance/npr-room/opening-body.md)登记源动作、固定转场和条件转场。常规入口回放覆盖教学身体动作、Windows 相机轨迹和第一章。
[开场表情与嘴型](../acceptance/npr-room/character-assets.md#开场表情与嘴型回放)登记台词、原图层叠加和收尾清理。
[开场视线](../acceptance/npr-room/character-assets.md#开场视线回放)登记原 Avatar 旋转、六部件叠加和常规入口回放。
[房间身体附加层](../acceptance/npr-room/character-assets.md#房间身体附加层)登记七部件的连续循环，以及叠加视线时的姿态误差。
[开场书本布置](../acceptance/npr-room/opening-book-layout.md)登记五套布置、剧情临时状态和存档恢复。
[开场桌面姿态](../acceptance/npr-room/opening-desk.md)登记原道具末帧、课本蒙皮和教学切换回放。

- [ ] 迁移角色完整行为与动画控制器。覆盖 5 图层上的混合、遮罩、重定向、转场和中断。核对身体、视线、表情、语音和动作事件。按 P0 确认的状态分母验证可达路线。
- [ ] 还原桌椅、书、杯子、窗口及互动道具的挂点和联动。以原采样定位连续性与姿态问题。使用现有椅子参考的 0.1° 角度和 0.0005 单位位置容差。
- [ ] 完成装饰、光照设置、房间偏好与窗景切换。动画、布料和粒子按真实路径验证正常播放、暂停、过渡和退出释放。人物动作与剧情、计时和道具事件按时间对账。
- [ ] 对每类动作采集关键姿态、转场与事件表。覆盖原有控制器的可达状态和原资产。首次集成同时执行公共动画回归，提交 `feat(p4): 复现角色控制器和房间联动`。

## P5 美术与渲染对照

[阴天房间光照](../acceptance/npr-room/cloudy-lighting.md)登记原版灯具、固定镜头渲染对照和房间回归。
[房间底色与主纹理](../acceptance/npr-room/room-base-surfaces.md)登记静态房间和道具的 170 个原材质绑定及固定镜头回放。
[Windows 窗景大气来源](../acceptance/npr-room/windows-atmosphere.md)登记原着色器、资源绑定和四个时段的运行参数。白天与傍晚的天空通道含原程序采样和 Vulkan 房间对照。
[开场全屏特效](../acceptance/npr-room/opening-effects.md)登记五个原版程序、全屏通道顺序和原材质。单色噪声通过原生组件合成，覆盖默认画面、游戏界面区域和开场两章的剧情时序。
[黑屏硬件参照](../acceptance/npr-room/opening-effects.md#黑屏特效硬件参照)登记比率、时间和百叶窗的五组原版画面。
[Vulkan 黑屏合成](../acceptance/npr-room/opening-effects.md#vulkan-黑屏合成)登记 Intel 核显和 RTX 4060 上的同设备画面对照。
[房间黑屏时序与合成](../acceptance/npr-room/opening-effects.md#房间黑屏时序与合成)登记正常入口的全黑、半黑、画面恢复和界面叠加。

- [ ] 按帧确认人物、房间、窗景与 UI 的合成顺序。重建使用中的色带、阴影、发丝、轮廓、透明、窗外环境、光照和后处理。原 D3D11 着色器及配置用于识别参考效果，Vulkan 实现按对应结果验收。
- [ ] 逐一加载 P0 确认的窗景，并覆盖现有 29 种参考窗景。对每类截图记录镜头、时间、色彩、光线和动画姿态。核查桌椅、皮肤、头发、窗与 UI 的区域结果。
- [ ] 对可严格配准的有效静态区域使用 SSIM ≥ 0.95、关键区域亮度中位误差 ≤ 10%。把区域和对齐条件预先固定。动态内容以轨迹、事件时序、帧序列及人工对照判定。
- [ ] 完成整组效果后采样 CPU/GPU 整帧与主要通道。依据成本占比、工作量和预期收益决定一项可观测的优化。预期够量后集中执行同条件配对，检查画面与性能。
- [ ] 记录所有可见效果的来源、实现契约和验收证据。通过跨设备受影响检查，提交 `feat(p5): 还原全场景非写实画面与后处理`。

## P6 平台能力与独立打包

[Windows 窗口回归](../acceptance/npr-room/platform-scenarios.md#windows-窗口回归)记录常规房间调整尺寸、最小化、恢复和计时存档重开。
[Windows 启动显示设置](../acceptance/npr-room/platform-scenarios.md#windows-启动显示设置)登记三档客户区像素、无边框全屏、置顶和命令行覆盖回放。
[房间显示面板](../acceptance/npr-room/platform-scenarios.md#房间显示面板)登记常规入口的显示切换、手动缩放和设置重开。
[成就统计夹具](../acceptance/npr-room/platform-scenarios.md#成就统计夹具)记录离线、提交失败、恢复和重开。
[房间成就记录](../acceptance/npr-room/platform-scenarios.md#房间成就记录)登记教学、待办、便签和计时的本地事件回放。

- [ ] 对照 Windows 参考的显示模式、窗口交互、平台成就、云存档、网络及登录状态。依据 P0 的实际可达范围复现客户端流程、成功失败状态和持久数据。
- [ ] 外部服务使用独立的授权测试身份。可控本地实现用于验证状态机、离线行为和失败恢复。真实服务证据、局部验证和缺少条件的项目分别登记，作为最终结论的依据。
- [ ] 使用 Release x64 安装树和 `build_game.py` 组装原资产本机包。核查挂载文件、包清单、来源与体积。包及其媒体写入 D 盘私有路径。 [发布器](../../tools/build_game.py)
- [ ] 从包的分发入口启动移动后的版本。验证源工程不可用条件下的启动、全流程、存档、资源加载和退出释放。保存一份包的指纹和复现命令，提交 `feat(p6): 完成平台对照与原资产独立打包`。

## P7 全量验收

| 维度 | 通过依据 |
| --- | --- |
| 游戏逻辑 | 功能矩阵可达项全部有实际运行证据。覆盖首次启动、房间、工具、进度、剧情分支、装饰、音频、设置和退出重开。 |
| 资产 | 原资产资源清单覆盖所有可达项。每项可追溯来源与目标。依赖齐全，场景保存和打开保持有效。 |
| 美术 | P5 全视觉矩阵与动态对照通过。冻结场景的画质和一致性按既有门禁复核。 |
| 性能 | 1080p60，预热 300 帧后采 1,800 帧。房间交付使用 CPU P95 ≤ 8 ms、GPU P95 ≤ 16.6 ms。正式十五轮、设备覆盖与三十分钟长跑按原门禁排程。 |
| 交付与兼容 | 私有包移动后独立运行。受影响的既有公开项目继续通过。按仓库规则完成 Debug、Release 和完整注册回归。 |

- [ ] 候选冻结后汇总相关修改，再按共享队列排正式批次。实现中使用目标编译与受影响短检。相同源码、二进制、资源锁和环境的证据可按覆盖范围引用。
- [ ] 环境条件不满足时保存诊断、影响范围和重试触发条件。继续独立工作，正式状态按实际证据填写。只有有效变化或新证据才重测对应批次。
- [ ] 归档 `Project/AzureRender/docs/acceptance/npr-room/` 下的精简报告。保留功能、资产、画面、性能、平台和发布各项结论，以及对应提交。未复现、参考缺失和外部待验项进入同一台账。
- [ ] 提交 `feat(p7): 归档完整移植验收与交付`。复核差异、Git 暂存、受管任务收尾和现有用户改动。向主线提供可集成补丁和适用范围。

完整复刻以实际证据为准。外部服务、参考缺口或任何可达内容未验收时，报告对应未完成项及其依赖。全量通过后，用户可按资源槽清单开始换入自制素材。

## 命令入口

先绑定 `$gameBuildDir`、`$gameRunRoot` 和预估新增量。受管任务在主项目登记。构建命令通过现有队列运行。

```powershell
$gameMainEngine = 'D:/Projects/FYP/Project/AzureRender'
python "$gameMainEngine/tools/artifact_maintenance.py" status
python "$gameMainEngine/tools/artifact_cleanup.py" --new-run recovery-port-p0 --planned-bytes 268435456
python "$gameMainEngine/tools/artifact_budget.py" --run $gameRunRoot --planned-bytes $gamePlannedBytes
cmake --build $gameBuildDir --target AzureSceneImportTests AzureAnimationPoseTests AzureAnimationCurveTests
ctest --test-dir $gameBuildDir -R 'AzureAssets.SceneImport|AzureEditor.SceneImportWorkflow|AzureRender.AnimationPose|AzureRender.AnimationCurve' --output-on-failure
python "$gameMainEngine/tools/artifact_cleanup.py" --finish $gameRunRoot
```

正式构建、回放与测量命令随候选记录在验收报告。相关门禁与已登记的批次流程见[协作与验收](../development-workflow.md)。
