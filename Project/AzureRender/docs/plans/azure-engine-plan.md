# Azure Engine 开发总计划

> 文档类型：开发计划
> 状态：F3、R6、G7、U2 Complete，P2 Active。F1 至 P1 完成。Android Deferred
> 更新日期：2026-10-05
> 适用范围：Windows 编辑器与 Windows 运行时。Android 目标暂缓
> 实现状态依据：源码、测试与阶段验收记录

## 产品目标与决策

Azure Engine 面向小型第三人称游戏，提供渲染、关卡、物理、脚本、反射代码生成、资产管线和编辑器。AzureRender 作为渲染核心的现有实现基础。当前代码仍使用 AzureRender 名称，模块重组阶段落实引擎命名。

用户已确认以下方向。基础设施采用自研轻量 ECS、Jolt、Lua 与 sol2，以及 C++ 元数据代码生成工具。资产采用 glTF、JSON 场景与 UUID 资源标识。

编辑器使用 Dear ImGui，游戏界面使用 RmlUi。许可证优先选择宽松协议。第三方版本与 Windows 编译兼容性在模块准入时验证。

最高优先级是渲染核心（Render Core）。首先完成场景数据收口、渲染图和渲染能力，再建设游戏运行时。平台生命周期和能力降级在渲染阶段进入契约，Android 适配仅在用户重新安排后规划。

首期编辑器支持进程内运行、暂停与单步。发布目标包括独立 Player，编辑器依赖只能进入编辑器构建目标。

当前产品目标包含莱万汀角色定稿与第三人称可玩关卡。主角完成面部、眉毛、头发和真实 idle/walk 动画。关卡提供基础 3C、多角色、场景实体和简单交互玩法。

工程范围包含构建复现、独立动画实例、资产加载与性能加固。实施与验收依据见 [第三人称角色与可玩关卡计划](third-person-playable-plan.md)。

## 文档入口与状态管理

本文是产品范围、阶段优先级、依赖和执行状态的唯一入口。历史技术路线位于 `docs/archive/plans/`，不参与当前阶段判断。[文档规范](../documentation-standard.md)约束全部新增计划和运行时说明。

阶段使用 Planned、Ready、Active、Complete、Blocked、Deferred 六种状态。一次一个 Active 阶段。Complete 必须附带提交、测试命令、环境和证据路径。已有 E0 至 E2 完成记录保留，R0 专门验证其运行期集成缺口。

## 阶段编码规则

阶段代码用于可独立验收的宏观目标。首字母标识产品边界。`F` 为基础设施，`R` 为 Render Core，`G` 为引擎运行时。

`U` 表示编辑器与交互，`P` 表示发布。`E0 至 E7` 保留为历史渲染技术编号。编号表示交付顺序，不代表内部接口数量。

内部类、测试、着色器、资源池和工具任务不创建新的阶段代码。它们必须归属于当前 Active 阶段，并在该阶段的验收记录中列出。历史 E0 至 E7 和旧 R0 至 R6 保留为技术追踪编号，不再新增同级细分阶段。

每个宏观阶段只允许一次 Complete 提交。阶段完成必须同时满足实现、端到端验收、性能记录、运行时文档、清理检查和 Git 提交六项条件。

## 平台与模块边界

| 模块 | 职责 | 依赖方向 |
| --- | --- | --- |
| Foundation | 日志、时间、文件、任务、标识、配置 | 标准库与平台设施 |
| Platform | 窗口、表面、输入、应用生命周期 | Foundation |
| Render Core | RHI、渲染图、GPU 场景、材质与光照 | Platform、Foundation |
| Runtime | World、Level、组件、资源、物理、脚本、动画、音频 | Foundation、Render Core |
| Editor | 项目、层级、属性、资产、操作历史、运行调试 | Runtime |
| Player | 启动、资源挂载、游戏循环与发布 | Runtime、Platform |
| Tools | 反射生成、资产转换、校验与打包 | 宿主机工具链 |

Windows 提供编辑器与 Player。Android 目标、原生窗口、生命周期、最低系统和设备性能预算无限期 Deferred，只有用户明确安排后才进入计划。桌面 GLFW 保留在 Windows 平台实现内。

资源访问通过统一虚拟路径支持桌面目录。Android 包内资产支持随平台阶段 Deferred。任务工具和反射生成器在宿主机运行，生成结果参与目标平台交叉编译。

## 当前执行路线

| 阶段 | 依赖 | 统一目标 | 完成门禁 |
| --- | --- | --- | --- |
| F0 基础设施与质量基线 |  → ，Complete | 构建、测试、文档、资源和发布门禁 | Debug/Release、CTest、文档检查、安装清单通过 |
| R0 Render Core 基础 | F0，Complete | RHI、场景数据、同步、实例化和平台生命周期 | 三场景视觉回归、窗口恢复、NullRHI 和性能基线通过 |
| R1 Render Graph 与帧流程 | R0，Complete（2026-09-26） | Render Graph、资源状态、瞬态资源、真实公共帧 Pass | 三场景接入同一图编译流程，历史帧和捕获通过 |
| R2 材质、Compute、光照与阴影 | R1，Complete（2026-09-30） | GPU IBL、多级 Bloom、Compute 蒙皮与 Morph、OpenEXR、多光源、聚簇光照、级联阴影和 PCSS | 固定表/Bindless、一致视觉、多光源和性能报告通过 |
| R3 并行提交与 GPU 驱动 | R2，Complete | 帧快照、并行录制、间接绘制和能力降级 | 单线程/并行确定性一致，CPU/GPU 指标达标 |
| R4 Windows Render Core 验收 | R3，Complete | 复杂场景、质量档位、安装、长跑和窗口恢复 | Debug/Release、Validation、安装包、长跑和恢复全部通过 |
| G0 引擎基础 | R4，Complete | Foundation、Platform、Runtime、Player 的库边界和项目配置 | 新建项目、独立 Player、资源挂载和 Windows 发布通过 |
| G1 反射与序列化 | G0，Complete | 代码生成、稳定类型标识、Inspector 元数据和版本迁移 | 增量生成、错误定位、序列化往返通过 |
| G2 资产、关卡与 Prefab | G1，Complete | AssetDatabase、Level、Prefab、依赖和热重载 | 资产移动、关卡切换和实例覆盖往返通过 |
| G3 物理与输入 | G2，Complete | Jolt、固定步长、碰撞查询、触发器和输入动作 | 角色、碰撞、触发器和实体删除回归通过 |
| G4 脚本与玩法 | G3，Complete | Lua、事件、反射绑定、错误隔离和受控重载 | 脚本角色控制、触发器和关卡切换通过 |
| U0 编辑器与游戏界面 | G4，Complete | 层级、Inspector、资源浏览、操作历史、RmlUi、动画状态机与音频 | 导入 → 放置 → 编辑 → 运行 → 停止闭环通过 |
| P0 Windows 游戏发布 | U0，Complete | 模板、打包、许可证清单和 Player 交付 | 安装、启动、游玩、关卡切换和退出通过 |
| F1 构建复现与资产准入 | P0，Complete | 当前源码构建、发布基线与素材清单 | Debug/Release、产物一致性、CI 测试发现与素材记录通过 |
| R5 角色外观与混合场景渲染 | F1、主角素材，Complete | 莱万汀面部、眉毛、头发与混合材质场景 | 主角七视角、光照、动画及实体共同渲染通过 |
| G5 独立动画与基础 3C | R5、idle/walk 素材，Complete | 每实体动画、角色控制与第三人称相机 | 主角真实动作、多资源动画与固定 3C 路线通过 |
| G6 关卡实体与交互玩法 | G5，Complete | NPC、收集、开门、目标反馈与重新开始 | 实际关卡完整流程、状态与实体生命周期通过 |
| U1 关卡制作与调试流程 | G6，Complete | 角色、动画、3C、碰撞与交互编辑 | 空项目制作、保存重开、运行调试与构建通过 |
| F2 运行时性能与工程加固 | U1，Complete | 后台加载、资源复用、性能预算与诊断 | 标准、压力、预加载切关与资源恢复门禁通过 |
| P1 可玩关卡交付验收 | F2，Complete | 独立可玩包、本机主角与设备验证记录 | 移动包游玩、30 分钟长跑与完整回归通过 |
| F3 仓库与文档基础 | P1，Complete | 目录、忽略规则、README 与 CI | 精确清单、活动回退、链接与两配置回归通过 |
| R6 场景可见性与裁剪 | F3，Complete | 剔除、深度、镜像与相机范围 | 默认路径、边界场景、GPU 图像与性能通过 |
| G7 动画方向与冲刺 | R6，Complete | 前向动画、双 Shift 与输入恢复 | 方向、速度、迁移、焦点和任务路线通过 |
| U2 编辑器工作区 | G7，Complete | UE 风格组织、停靠、面板与教程 | DPI、小窗口、真实控件与制作流程通过 |
| P2 展示与独立交付 | U2，Active | 截图、README、游戏包与完整证据 | 完整回归、长跑、隔离包与清单通过 |

`R2` 已完成，端到端证据见 [R2 阶段验收记录](../acceptance/r2/2026-09-26.md)。`R3` 当前为 Complete，具备帧快照、并行录制和 GPU 驱动提交。阶段门禁与当前证据见 [R3 阶段验收](../acceptance/r3/2026-10-03.md)。

`R4` 为 Complete，复杂场景、质量档位、长跑与发布验收通过。证据见 [R4 阶段验收](../acceptance/r4/2026-10-03.md)。`G0` 为 Complete，黑洞优化准入任务为 Complete。

## G0 前置任务

[黑洞性能优化](2026-10-03-blackhole-optimization.md)为 Complete。电影档在本机 Release、1280×720、正面相机下，三轮整帧 GPU 平均值的中位数为 18.071215 ms，满足 20 ms 门禁。

任务保留四射线与现行视觉流程，图像、引擎开销和发布验收通过。G0 的交付与验收依据为 [引擎基础实施计划](g0-implementation.md)。

## G0 完成结果

G0 模块库、项目配置、独立 Player、运行时生命周期和性能观测通过验收。Debug 与 Release 各 41 项回归通过，三场景输出与冻结参考一致。结果见 [G0 验收](../acceptance/g0/2026-10-03.md)，模块使用见 [引擎基础](../runtime/engine-foundation.md)。

G1 至 P0 为 Complete。U0 提交为 `0248e54`，证据见 [编辑器与界面验收](../acceptance/u0/2026-10-04.md)。实施记录依次为 [G1](g1-implementation.md)、[G2](g2-implementation.md)、[G3](g3-implementation.md)和 [G4](g4-implementation.md)。

P0 交付公开游戏模板、编辑器构建入口和可移动 Windows 游戏包。Debug 57 项与 Release 58 项完整回归通过，证据见 [P0 验收](../acceptance/p0/2026-10-04.md)。阶段提交为 `feat(p0): 完成游戏模板与Windows独立发布闭环`。

G4 的双关卡脚本玩法闭环与发布门禁通过，Debug 和 Release 各 49 项回归通过。黑洞电影档 GPU 中位数为 18.851835 ms。证据见 [G4 验收](../acceptance/g4/2026-10-04.md)。

## 第三人称可玩关卡路线

F1 提交为 `9ce99e6`。R5 为 Complete，结果见 [R5 验收](../acceptance/r5/2026-10-04.md)。G5 为 Complete，结果见 [G5 验收](../acceptance/g5/2026-10-04.md)。

U1 为 Complete，结果见 [U1 验收](../acceptance/u1/2026-10-05.md)。F2 为 Complete，结果见 [F2 验收](../acceptance/f2/2026-10-05.md)。P1 为 Complete，结果见 [P1 验收](../acceptance/p1/2026-10-05.md)。

构建与素材准入见 [F1 验收](../acceptance/f1/2026-10-04.md)。R5 至 P1 按依赖依次进入，文件级任务见 [本轮实施计划](third-person-playable-plan.md)。

R5 以莱万汀实际画面验收眉毛、面部和头发。G5 以真实 idle/walk 验收角色表现，支持多个实体独立播放。基础 3C 包含相机相对移动、胶囊角色、跟随旋转与相机遮挡。

G6 交付包含 NPC、三件收集物、门与目标区的可玩关卡。U1 贯通制作和调试，F2 达到固定负载预算。P1 验收移动游戏包与长跑，并登记可用额外 GPU 的扩展结果。

标准关卡以 1080p、四个动画角色和一百个实体验收。GPU P95 预算为 16.6 ms，Player CPU 工作 P95 为 8 ms。完整测量口径、压力负载和资源预算在实施计划中定义。

本机主角与用户提供的 idle、walk 已通过 F1 准入。动作使用 Mixamo 骨架，G5 执行离线重定向。公共项目使用许可明确资源，本机主角项目保存私有素材范围。

## 验收环境与性能口径

本轮优化依据为[编辑器与可玩体验优化计划](2026-10-05-quality-round.md)。F3 整理仓库与文档基础。R6、G7 验证场景显示与角色动作。U2 完成工作区和教程，P2 生成最终展示与独立交付。

Windows 验收使用当前本机环境，记录设备、驱动和工具链。设备信息属于证据元数据，阶段准入依据同机对照和现行预算。R3 使用 `r3-budget-v2`，具体指标见[渲染核心实施计划](render-core-implementation.md)。

## 阶段执行规则

阶段状态以当前执行路线表为准。文件级工作清单位于 [渲染核心实施计划](render-core-implementation.md)。G0 文件级任务位于 [引擎基础实施计划](g0-implementation.md)。每阶段完成实现、测试、性能采集、文档同步与提交。

提交标题使用 `feat(<phase>): <中文摘要>`。验收覆盖 Windows Debug/Release、CTest、Validation、安装资源、三场景与固定描述符路径。

## 渲染阶段的关键契约

R0 让 SceneDocument 承担序列化职责，运行期 World 持有实体组件，渲染快照提供不可变的逐帧实例数据。GPU 资源以资源标识共享，主角色与附加角色的动画能力分别列入测试。

R1 明确外部导入、跨帧持久、帧内瞬态三种资源类别。历史帧、交换链、捕获回读各自声明所有权与起止状态。资源池复用以 GPU 完成信号为依据。先实现单图形队列，接口描述计算与传输访问，后续队列扩展由性能证据驱动。

黑洞结构迁移保持已冻结的着色与视觉输出，迁移前保存确定性基线。涉及着色算法调整时单独立项。七个角色视图均为本地验收项，CI 覆盖范围在报告中明确。

性能记录区分 CPU 提交、GPU pass、整帧耗时、显存、对象数与可见比例。实例化收益以测量结果报告。Windows 性能采集记录设备、驱动、分辨率和质量档位。移动端采集随 Android 阶段 Deferred。

## 编辑器交互契约

各模块加入时同时实现基础交互，U0 负责贯通与打磨。统一使用主菜单、工具栏、层级、视口、Inspector、资源浏览和日志布局。命令具有稳定标识、快捷键、启用条件与撤销语义。

编辑态和运行态场景隔离。停止运行恢复编辑态。选择、拖放、删除、重复、重命名、多选、脏状态与保存反馈采用统一规则。

耗时导入展示进度与失败原因，允许安全取消。输入焦点决定快捷键归属，文本编辑和游戏操作分别验证。

验收使用任务脚本：新建项目、导入 glTF、放置物体、编辑属性、撤销、保存重开、运行角色、修复脚本错误、构建 Player。记录阻断问题、步骤数、完成时间和误操作恢复结果。

## 外部参考与代码来源

调研日期为 2026-09-26。候选仓库的 GitHub 元数据标注为 MIT。采纳前逐文件核验许可证、第三方子目录和平台条件，并固定版本。

| 来源 | 计划采纳方式 | 验证重点 |
| --- | --- | --- |
| [Piccolo](https://github.com/BoomingTech/Piccolo) | 参考 RenderScene/RenderResource、交换数据、关卡与反射生成链路。按文件评估移植 | 所有权、测试、宿主工具交叉编译 |
| [Lumix Engine](https://github.com/nem0/LumixEngine) | 参考轻量引擎模块、编辑器、资产与 Lua 工作流 | 接口规模与编辑器操作闭环 |
| [Godot](https://github.com/godotengine/godot) | 参考场景、资源、Inspector。Android 生命周期参考 Deferred | 概念适配、设备行为、交互一致性 |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 集成物理库 | Windows 构建、任务调度、回调与实体生命周期 |
| [sol2](https://github.com/ThePhD/sol2) | 集成 Lua C++ 绑定 | 异常、生命周期、编译成本与 Windows 兼容性 |
| [RmlUi](https://github.com/mikke89/RmlUi) | 集成游戏 UI，提供引擎渲染与输入适配 | 字体、触摸、DPI、安全区和资源访问 |
| [Android 游戏开发文档](https://developer.android.com/games) | 平台规范与工具入口 | Deferred：生命周期、输入、性能分析与发布 |

每个子系统计划必须列出来源 URL、版本或提交、参考文件、采纳方式、适配理由和验收方法。直接移植保留版权与许可证，记录本地修改和上游同步方式。具体算法与代码引用在实施时补充到文件级，仓库级调研只表示候选来源。

## 当前待固定事项

Android 目标无限期 Deferred，不配置 Android SDK、NDK、真机或移动端性能预算。G1 使用 C++17 受限注解生成器，语法范围、增量输出和诊断由契约测试覆盖。

G2 使用 UUID 与指纹分版本缓存，以及版本化目录资源包。U0 使用 miniaudio 与 RmlUi。P0 使用 Python 标准库生成 Windows 游戏目录。

R6 验收见[场景可见性与裁剪验收](../acceptance/r6/2026-10-05.md)。
