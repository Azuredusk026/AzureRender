# Hazel 与 Fermion 借鉴评估

> 文档类型：源码调研与采用建议
> 日期：2026-10-09
> 状态：调研完成，实施候选待安排
> 读者：引擎开发者与编辑器使用者
> 配套：[编辑器产品化计划](../plans/2026-10-09-editor-productization.md)
> 追踪：[借鉴清单](../plans/hazel-fermion-manifest.json)

## 调研结论

Hazel 适合学习小型编辑器的基本制作闭环。
Fermion 适合参考资产制作界面与三维辅助工具。
两者都有可采用的局部实现和交互组织。
采用单位是公共能力、控件或算法。

本轮最值得建设的是内容制作与编辑工具。
现有渲染、资产、脚本和编辑服务提供实施基础。
独立材质、纹理设置和资产操作需要完整契约。
每项交付同时验证通用性与实际使用流程。

本轮列出三十四项候选。
其中十二项核心，十四项强烈推荐，八项可选或暂缓。
新增能力与现有完成项分别记录。
每项来源、成本、风险与验收见下文。

## 版本、关联与范围

| 仓库 | 本地位置 | 来源提交 | 提交日期 | 根许可证 |
| --- | --- | --- | --- | --- |
| Hazel | `D:/Project/Hazel` | `1feb70572fa87fa1c4ba784a2cfeada5b4a500db` | 2023-10-27 | Apache-2.0 |
| Fermion | `D:/Project/Fermion` | `eeac811ae4b6dd7827d24c255b4fa0241ac603af` | 2026-04-07 | MIT |

两仓库的检查时工作树均干净。
Hazel 来源为 `TheCherno/Hazel`。
Fermion 来源为 `Yang-Junjie/Fermion`。
评估仅适用于上述本地提交。

Fermion 的中英文 README 明确说明 Hazel 关联。
源码中的相机、场景复制与编辑流程也有对应结构。
它在此基础上加入三维制作与资产模块。
本报告将它视作教程基础上的扩展项目。

Hazel README 区分公开版本与私有 Hazel-dev。
本轮检查范围为本地公开版本。
README 中的未来规划只作为项目意图。
功能事实以本地源码为准。

本轮读取了编辑器、资产和核心模块。
还检查了场景、脚本、物理与动画入口。
界面图来自 Fermion 仓库的 `ScreenShots/`。
截图用于核对组织，实际交互依据源码。

本轮未编译或启动两个外部引擎。
运行稳定性和性能不属于本轮实测结论。
直接移植前需建立对应构建与行为夹具。
来源行号可由完整提交固定复查。

## 许可与移植归属

Hazel 的根协议为 Apache-2.0。
代码移植需保留协议、版权与适用归属。
修改文件需有显著的修改说明。
若来源提供 NOTICE，分发需携带适用内容。

Fermion 的根协议为 MIT。
移植其原创内容需保留版权与协议。
派生片段仍需追查 Hazel 和第三方来源。
根 MIT 声明不能替代逐文件来源核验。

两仓库本次检索未发现根 NOTICE 文件。
字体、图标、模型和截图单独核验许可。
源代码许可不能直接证明这些素材可再分发。
本轮通过路径引用截图，未复制展示素材。

Fermion 内置 ImViewGuizmo 单头文件。
文件头含 Marcel Kazemi 的 MIT 授权。
方向控件可从该上游实现独立接入。
接入时固定文件哈希、来源和修改说明。

ImguiNodeEditor 在 Fermion 中为子模块。
子模块记录的提交为 `b302971455b3719ec9b5fb94b2f92d27c62b9ff0`。
本地子模块缺少可直接核验的完整文件。
其许可和版本需在采用阶段核实。

画布接入以核验完成后的依赖版本为准。

## 当前引擎能力与缺口

当前源码基础提交为 `a070b0ba99016137e85f4faa33b9527cd7ba28b1`。
工作树含 P4 验收及角色渲染在途改动。
本轮按当前文件读取能力，未重新运行引擎门禁。
历史验收不能替代这些改动的构建证据。

| 范围 | 已有基础 | 本轮拟完善 |
| --- | --- | --- |
| 工作区 | 停靠、主题、DPI、面板注册、布局恢复 | 制作布局、职责分区、项目入口 |
| 选择与导航 | 右键导航、W/E/R、多选、F 与双击构图 | 正交方向控件、速度反馈 |
| 属性 | 组件元数据、字段校验、重置、引用下拉 | 统一属性行、吸管、混合值 |
| 内容 | UUID、依赖、导入、目录筛选、射线放置 | 目录树、类型打开、文件事务 |
| 材质与纹理 | glTF 材质、着色器模块、纹理加载 | 独立资产文档、材质槽、导入设置 |
| 预览 | 离屏视图、模型缩略图、相机预览 | 材质与图片提供者、共享缓存失效 |
| 运行 | Play、Pause、Step、Stop、编辑态恢复 | 独立物理模拟模式 |
| 制作 | Prefab、动画状态、Lua 与托管扩展 | 集中编辑与诊断工作流 |
| 自动工具 | 编辑事务、观察、提案与验证服务 | 新制作能力的操作与领域描述 |

大纲单击只选择，双击才定位。
F 定位全部选择，右键导航拥有移动按键。
这些规则属于现行验收契约。
本轮作为回归项持续验证。

## 分级与成本依据

核心项直接决定常用制作路径是否完整。
它们优先满足可发现、可修改、可保存与可恢复。
强烈推荐项补充专业编辑效率和基础制作范围。
可选项依赖特定项目需求或专项收益证明。

契合度综合技术栈、现有接口和数据模型。
高表示可复用现有服务，中表示需增加较多契约。
低表示整体迁移与现有技术主线冲突。
采用方式同时考虑源码来源和状态所有权。

成本按单人开发工作日粗估。
实现、局部测试和文档包含在范围内。
共同基础会让项目成本发生重叠。
阶段成本以配套计划为准，不累加各项数字。

风险分为低、中、高。
低主要影响局部控件，中涉及状态与跨面板协作。
高涉及持久数据、资源生命周期或兼容承诺。
风险高的内容先完成服务与失败恢复夹具。

## 优先级总览

| 编号 | 借鉴内容 | 级别 | 采用方式 | 阶段 |
| --- | --- | --- | --- | --- |
| A01 | 工作区与设置分区 | 核心 | 参考设计 | U8 |
| A02 | 统一属性控件与轴重置 | 核心 | 小段适配移植 | U9 |
| A03 | 资产与对象引用选择器 | 核心 | 参考交互并重写服务 | U9 |
| A04 | 属性面板上下文与批量编辑 | 核心 | 参考设计；批量语义为本项目扩展 | U9 |
| A05 | 内容目录树与导航 | 核心 | 适配布局；导航服务自主实现 | U10 |
| A06 | 按类型打开资产 | 核心 | 参考设计并扩展注册契约 | U10 |
| A07 | 受控内容文件操作 | 核心 | 参考交互；文件事务自主实现 | G12 |
| A08 | 纹理导入设置与批处理 | 核心 | 适配数据契约与交互 | G12 |
| A09 | 独立材质资产编辑 | 核心 | 参考固定参数设计并重写 | U11 |
| A10 | 子网格与材质槽编辑 | 核心 | 适配交互；覆盖数据自主实现 | U11 |
| A11 | 多类型缩略图与预览服务 | 核心 | 参考提供者设计；复用现有服务 | U11 |
| A12 | 编辑、运行与模拟会话 | 核心 | 参考生命周期并适配现有会话 | G13 |
| B01 | 方向控件与正交视图 | 强烈推荐 | 直接接入上游控件；相机适配 | U12 |
| B02 | 网格与编辑辅助显示 | 强烈推荐 | 参考算法；Vulkan 着色器重写 | U12 |
| B03 | 导航速度与模式反馈 | 强烈推荐 | 小段交互适配 | U12 |
| B04 | 基础几何与制作预设 | 强烈推荐 | 适配纯几何生成代码 | U10 |
| B05 | 资产诊断与引用影响 | 强烈推荐 | 适配表格；诊断自主扩展 | G12 |
| B06 | 项目入口与最近项目 | 强烈推荐 | 参考流程；最近项目为本项目扩展 | U8 |
| B07 | 场景文档与工作区恢复 | 强烈推荐 | 参考基础流程；多文档为自主扩展 | U13 |
| B08 | 动画片段与骨架检视 | 强烈推荐 | 参考控件并复用现有动画 | U13 |
| B09 | 物理形状与约束制作 | 强烈推荐 | 适配相同物理库的设计与算法 | G13 |
| B10 | 脚本字段与调试制作流程 | 强烈推荐 | 参考体验并复用 ScriptRuntimeRegistry | U13 |
| B11 | 结构化问题列表 | 强烈推荐 | 参考排版；结构化诊断自主实现 | U13 |
| B12 | 面板上下文与扩展装配 | 强烈推荐 | 参考拆分；沿用公共编辑接口 | U8 |
| B13 | AI 辅助接入新制作能力 | 强烈推荐 | 本项目横向要求；非外部实现移植 | U13 |
| B14 | Prefab 制作与覆盖面板 | 强烈推荐 | 已有能力产品化；非外部 Prefab 移植 | U13 |
| C01 | 材质连接画布 | 可选或暂缓 | 可选接入上游画布；固定参数图适配 | U14 |
| C02 | 二维精灵与世界文字 | 可选或暂缓 | 参考批处理；Vulkan 渲染重写 | G14 |
| C03 | 异步 GPU 对象拾取 | 可选或暂缓 | 参考对象标识；Vulkan 异步实现 | R10 |
| C04 | 渲染图可视化与诊断 | 可选或暂缓 | 参考错误展示；保留现有编译器 | U14 |
| C05 | 更多模型源格式 | 可选或暂缓 | 参考导入分解；可选工具适配 | G14 |
| C06 | CPU 时间线导出 | 可选或暂缓 | 小段格式与作用域适配 | U14 |
| C07 | 自绘窗口标题栏 | 可选或暂缓 | 暂缓；保留样式参考 | Deferred |
| C08 | 整套底层模块替换 | 可选或暂缓 | 暂缓；仅作边界反例 | Deferred |

## 核心借鉴项

### A01 工作区与设置分区

来源：[Fermion：onImGuiRender](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/BosonLayer.cpp#L174)。
文件为 `Boson/src/BosonLayer.cpp`，定位行 174。

- 源码事实与价值：停靠面板围绕中央视口组织，属性与环境设置分开。
- Azure 现状：已有停靠、主题、DPI 和布局恢复。属性面板同时包含对象与全局渲染设置。
- 拟采用内容：建立制作、调试布局。对象、场景、项目和用户设置各有入口。
- 采用方式：参考设计。契合度：高。成本：2–3 工作日。
- 风险：中：面板焦点和布局迁移。
- 验收：双项目、六组尺寸与 DPI 下操作可达。布局保存和恢复正确。
- 阶段归属：`U8`。状态：候选。

### A02 统一属性控件与轴重置

来源：[Fermion：drawVec3Control](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/ImGui/BosonUI.hpp#L26)。
文件为 `Fermion/Sources/ImGui/BosonUI.hpp`，定位行 26。

- 源码事实与价值：属性行对齐，三轴着色，每个轴有重置按钮。
- Azure 现状：已有 Widgets、语义主题和字段重置。变换使用普通 DragFloat3。
- 拟采用内容：扩展公共属性控件。默认值、单位和范围来自元数据。
- 采用方式：小段适配移植。契合度：高。成本：1–2 工作日。
- 风险：低：窄面板、单位和撤销合并。
- 验收：拖动合并为一次撤销。单轴重置正确，中文标签与窄面板可读。
- 阶段归属：`U9`。状态：候选。

### A03 资产与对象引用选择器

来源：[Fermion：deliverPickedEntity](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/InspectorPanel.cpp#L376)。
文件为 `Boson/src/Panels/InspectorPanel.cpp`，定位行 376。

- 源码事实与价值：支持视口吸管选择关联对象，支持资源拖放。
- Azure 现状：节点和资产引用已用下拉框编辑。缺少通用吸管与资源揭示流程。
- 拟采用内容：按引用元数据提供搜索、拖放、吸管、清空和揭示。
- 采用方式：参考交互并重写服务。契合度：高。成本：2–3 工作日。
- 风险：中：失效对象、跨项目和拖放类型。
- 验收：Esc 取消无写入。非法类型与失效引用被拒绝，成功写入可撤销。
- 阶段归属：`U9`。状态：候选。

### A04 属性面板上下文与批量编辑

来源：[Fermion：drawComponents](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/InspectorPanel.cpp#L460)。
文件为 `Boson/src/Panels/InspectorPanel.cpp`，定位行 460。

- 源码事实与价值：组件分组与专门属性控件提供清晰阅读顺序。
- Azure 现状：已有组件注册与多选变换。组件字段主要消费主选中对象。
- 拟采用内容：显示共同组件与混合值。按字段声明批量修改规则。空选择显示引导。
- 采用方式：参考设计；批量语义为本项目扩展。契合度：高。成本：2–4 工作日。
- 风险：中：混合值、只读字段和部分失败。
- 验收：两个不同类型对象批量编辑。一个非法值使事务完整恢复。
- 阶段归属：`U9`。状态：候选。

### A05 内容目录树与导航

来源：[Fermion：onImGuiRender / drawFolderTree](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/ContentBrowserPanel.cpp#L37)。
文件为 `Boson/src/Panels/ContentBrowserPanel.cpp`，定位行 37。

- 源码事实与价值：左侧目录树与右侧网格协作，目录双击进入。
- Azure 现状：已有搜索、类型筛选和网格。目录入口为下拉筛选。
- 拟采用内容：添加挂载树、面包屑、前进后退与可调缩略图。枚举通过目录模型缓存。
- 采用方式：适配布局；导航服务自主实现。契合度：高。成本：2–3 工作日。
- 风险：中：目录规模、挂载和中文路径。
- 验收：空目录可见，项目移动后导航有效。大量条目使用裁剪与增量刷新。
- 阶段归属：`U10`。状态：候选。

### A06 按类型打开资产

来源：[Fermion：HandleDragDrop / double-click dispatch](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/ContentBrowserPanel.cpp#L285)。
文件为 `Boson/src/Panels/ContentBrowserPanel.cpp`，定位行 285。

- 源码事实与价值：场景、材质、纹理等资源提供类型化交互。
- Azure 现状：当前可放置资产的双击行为是直接放到默认射线上。
- 拟采用内容：注册资产默认打开操作。单击选择资产，双击打开编辑或预览。拖到视口显式放置。
- 采用方式：参考设计并扩展注册契约。契合度：高。成本：2–3 工作日。
- 风险：中：打开与放置的语义迁移。
- 验收：模型、关卡、材质、图片、脚本各有结果。大纲单击与双击规则保持有效。
- 阶段归属：`U10`。状态：候选。

### A07 受控内容文件操作

来源：[Fermion：Create New Folder / Open in File Manager](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/ContentBrowserPanel.cpp#L120)。
文件为 `Boson/src/Panels/ContentBrowserPanel.cpp`，定位行 120。

- 源码事实与价值：内容区提供新建目录、文件管理器揭示和复制资产标识。
- Azure 现状：已有 UUID、依赖、导入和放置。面板缺少完整目录制作操作。
- 拟采用内容：统一新建、重命名、移动、复制和删除。预览引用影响并维护元数据。
- 采用方式：参考交互；文件事务自主实现。契合度：高。成本：3–5 工作日。
- 风险：高：磁盘与数据库一致性。
- 验收：移动保持 UUID。重名、只读、被引用删除和写入失败都有确定结果。
- 阶段归属：`G12`。状态：候选。

### A08 纹理导入设置与批处理

来源：[Fermion：drawTextureConfig / saveAllTextures](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/TextureConfigPanel.cpp#L292)。
文件为 `Boson/src/Panels/TextureConfigPanel.cpp`，定位行 292。

- 源码事实与价值：设置纹理层级、过滤、环绕、各向异性和颜色空间。
- Azure 现状：图片类型已注册。glTF 纹理加载与采样修复在途。缺少独立设置文档与面板。
- 拟采用内容：建立纹理用途与采样配置。保存触发可取消重导入，批量字段可单独选择。
- 采用方式：适配数据契约与交互。契合度：高。成本：4–7 工作日。
- 风险：高：颜色语义、缓存失效和 GPU 生命周期。
- 验收：颜色、法线和遮罩分别验证。保存重开、取消、失败恢复及设备上限通过。
- 阶段归属：`G12`。状态：候选。

### A09 独立材质资产编辑

来源：[Fermion：compileMaterial](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/MaterialEditorPanel.cpp#L592)。
文件为 `Boson/src/Panels/MaterialEditorPanel.cpp`，定位行 592。

- 源码事实与价值：把固定 PBR 输出参数和贴图连接保存为材质。
- Azure 现状：已有模型材质、着色器模块和角色材质解释。缺少独立材质资产制作入口。
- 拟采用内容：材质类型声明参数与贴图槽。先交付表单编辑、保存和实例覆盖。
- 采用方式：参考固定参数设计并重写。契合度：高。成本：4–7 工作日。
- 风险：高：通用 PBR 与风格材质边界。
- 验收：PBR 与风格材质各完成创建、赋值、撤销、重开和 Player 发布。
- 阶段归属：`U11`。状态：候选。

### A10 子网格与材质槽编辑

来源：[Fermion：drawSubmeshMaterialsEditor](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/InspectorPanel.cpp#L238)。
文件为 `Boson/src/Panels/InspectorPanel.cpp`，定位行 238。

- 源码事实与价值：展示子网格列表、当前材质与缩略图，支持拖放赋值。
- Azure 现状：模型加载已有材质索引。对象检查器缺少材质槽工作流。
- 拟采用内容：使用稳定槽位标识保存覆盖。支持揭示、重置和批量赋值。
- 采用方式：适配交互；覆盖数据自主实现。契合度：高。成本：3–5 工作日。
- 风险：高：源模型重导入后槽位映射。
- 验收：多材质模型与实例分别保存。源槽变化有冲突报告，其他实例保持自身设置。
- 阶段归属：`U11`。状态：候选。

### A11 多类型缩略图与预览服务

来源：[Fermion：IThumbnailProvider](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Renderer/Thumbnail/IThumbnailProvider.hpp#L10)。
文件为 `Fermion/Sources/Renderer/Thumbnail/IThumbnailProvider.hpp`，定位行 10。

- 源码事实与价值：按资产类型提供材质球缩略图，浏览器和属性面板共享预览。
- Azure 现状：已有 RenderViewService 和两槽 AssetThumbnailService。面板主要请求模型预览。
- 拟采用内容：注册模型、图片、材质预览提供者。缓存键含依赖指纹与设置。
- 采用方式：参考提供者设计；复用现有服务。契合度：高。成本：3–5 工作日。
- 风险：高：缓存上限、GPU 在途释放与刷新。
- 验收：修改贴图更新关联材质预览。容量、失效、关面板与切项目均正确。
- 阶段归属：`U11`。状态：候选。

### A12 编辑、运行与模拟会话

来源：[Hazel：OnScenePlay / OnSceneSimulate / OnSceneStop](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazelnut/src/EditorLayer.cpp#L712)。
文件为 `Hazelnut/src/EditorLayer.cpp`，定位行 712。

- 源码事实与价值：运行与物理模拟使用编辑场景副本，停止回到编辑场景。
- Azure 现状：已有 Play、Pause、Step、Stop 与编辑态恢复。缺少独立物理模拟模式。
- 拟采用内容：以会话模式声明启用系统。模拟默认启用物理并关闭玩法脚本。
- 采用方式：参考生命周期并适配现有会话。契合度：高。成本：3–5 工作日。
- 风险：高：输入、脚本、物理与资源关闭顺序。
- 验收：反复 Play/Simulate/Stop 后编辑文档哈希一致。失败启动和停止均释放资源。
- 阶段归属：`G13`。状态：候选。

## 强烈推荐项

### B01 方向控件与正交视图

来源：[Fermion：ImViewGuizmo::Rotate](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/ViewportPanel.cpp#L301)。
文件为 `Boson/src/Panels/ViewportPanel.cpp`，定位行 301。

- 源码事实与价值：视口右上角提供可点击方向轴，编辑相机支持正交投影。
- Azure 现状：已有飞行、环绕、平移与构图。主编辑相机服务只暴露位置与目标。
- 拟采用内容：接入方向控件与六向视图，统一透视及正交投影。
- 采用方式：直接接入上游控件；相机适配。契合度：高。成本：3–5 工作日。
- 风险：中：Vulkan 投影、坐标系和拾取一致性。
- 验收：六向构图、透视切换、定位、拾取和操纵器使用同一相机描述。
- 阶段归属：`U12`。状态：候选。

### B02 网格与编辑辅助显示

来源：[Fermion：infinite-grid shader](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/Resources/Shaders/InfiniteGrid.glsl#L1)。
文件为 `Boson/Resources/Shaders/InfiniteGrid.glsl`，定位行 1。

- 源码事实与价值：可选择网格平面，配合相机、灯光图标与物理辅助显示。
- Azure 现状：已有选择框与 GameplayDebugGeometry。缺少完整编辑辅助类别。
- 拟采用内容：注册网格、图标、边界和相机视锥提供者。编辑辅助与游戏渲染分开。
- 采用方式：参考算法；Vulkan 着色器重写。契合度：高。成本：2–4 工作日。
- 风险：中：深度、透明排序、远近裁剪。
- 验收：辅助显示按类别开关。远近视距稳定，Player 画面保持项目表现。
- 阶段归属：`U12`。状态：候选。

### B03 导航速度与模式反馈

来源：[Fermion：onMouseScroll](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Renderer/Camera/EditorCamera.cpp#L131)。
文件为 `Fermion/Sources/Renderer/Camera/EditorCamera.cpp`，定位行 131。

- 源码事实与价值：飞行模式滚轮调节速度，视口显示速度条。
- Azure 现状：已有右键导航、Shift 加速、方向设置和可配置速度。
- 拟采用内容：飞行滚轮调基础速度。状态条显示导航模式、速度与快捷键。
- 采用方式：小段交互适配。契合度：高。成本：1–2 工作日。
- 风险：低：滚轮归属和速度持久化。
- 验收：右键外滚轮缩放，右键内滚轮调速。Shift 释放恢复速度，失焦释放捕获。
- 阶段归属：`U12`。状态：候选。

### B04 基础几何与制作预设

来源：[Fermion：CreateBox / CreateSphere / CreateCylinder](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Renderer/Model/MeshFactory.cpp#L9)。
文件为 `Fermion/Sources/Renderer/Model/MeshFactory.cpp`，定位行 9。

- 源码事实与价值：提供盒、球、圆柱、胶囊和圆锥生成函数。
- Azure 现状：已有注册生成器、参数化几何和内容来源清单。创建栏主要提供空节点。
- 拟采用内容：基础几何进入 GeneratorRegistry。相机、光源和物理对象以注册模板创建。
- 采用方式：适配纯几何生成代码。契合度：高。成本：2–3 工作日。
- 风险：中：法线、切线、绕序和参数预算。
- 验收：两个项目可生成并发布。极小尺寸、分段边界、负值和预算验证通过。
- 阶段归属：`U10`。状态：候选。

### B05 资产诊断与引用影响

来源：[Fermion：onImGuiRender](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/AssetManagerPanel.cpp#L14)。
文件为 `Boson/src/Panels/AssetManagerPanel.cpp`，定位行 14。

- 源码事实与价值：表格显示资产类型、标识、路径、加载状态，支持搜索。
- Azure 现状：已有目录状态、引用计数、依赖和来源信息。缺少完整影响分析页面。
- 拟采用内容：展示依赖与反向引用、导入来源、失败和重导入。快照按版本生成。
- 采用方式：适配表格；诊断自主扩展。契合度：高。成本：2–3 工作日。
- 风险：中：诊断快照成本与过期状态。
- 验收：改动源文件可追到依赖。只读诊断保持文档与 GPU 状态。
- 阶段归属：`G12`。状态：候选。

### B06 项目入口与最近项目

来源：[Fermion：newProject / openProject](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/BosonLayer_Project.cpp#L39)。
文件为 `Boson/src/BosonLayer_Project.cpp`，定位行 39。

- 源码事实与价值：提供项目创建与打开，配置起始场景和资产目录。
- Azure 现状：已有 Project、模板工具和打开服务。最近目录入口用于路径选择。
- 拟采用内容：欢迎页提供模板、最近项目、导入项目和错误恢复。调用项目服务。
- 采用方式：参考流程；最近项目为本项目扩展。契合度：高。成本：3–5 工作日。
- 风险：中：模板复制与切换中的未保存状态。
- 验收：全新目录完成创建与启动。中文路径、移动项目、缺失最近项目均可处理。
- 阶段归属：`U8`。状态：候选。

### B07 场景文档与工作区恢复

来源：[Hazel：NewScene / OpenScene / SaveScene](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazelnut/src/EditorLayer.cpp#L650)。
文件为 `Hazelnut/src/EditorLayer.cpp`，定位行 650。

- 源码事实与价值：场景创建、打开和保存构成制作入口。
- Azure 现状：已有单文档、保存保护和备份。缺少多文档工作区。
- 拟采用内容：提供场景标签、逐文档保存与恢复。每个文档独立拥有选择、版本和历史。
- 采用方式：参考基础流程；多文档为自主扩展。契合度：中。成本：4–7 工作日。
- 风险：高：版本、撤销栈和选择跨文档串用。
- 验收：打开两关卡分别编辑。关标签、切项目及异常重启都恢复正确归属。
- 阶段归属：`U13`。状态：候选。

### B08 动画片段与骨架检视

来源：[Fermion：animation clip controls](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/InspectorPanel.cpp#L818)。
文件为 `Boson/src/Panels/InspectorPanel.cpp`，定位行 818。

- 源码事实与价值：检查器列出动画片段，并提供播放控制。
- Azure 现状：已有语义动画预览、混合和状态机。面板按手动时间预览姿势。
- 拟采用内容：加入播放时间轴、循环、速度、骨架显示和重绑定诊断。
- 采用方式：参考控件并复用现有动画。契合度：高。成本：3–5 工作日。
- 风险：中：多实例、预览状态与玩法状态混用。
- 验收：两个不同骨架独立预览。暂停拖动与切对象正确，预览保持制作数据。
- 阶段归属：`U13`。状态：候选。

### B09 物理形状与约束制作

来源：[Fermion：CreateBoxShape / CreateMeshShape](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Physics/Physics3DShapes.cpp#L20)。
文件为 `Fermion/Sources/Physics/Physics3DShapes.cpp`，定位行 20。

- 源码事实与价值：Jolt 形状创建独立于调度，支持多类碰撞体与铰链约束。
- Azure 现状：已有 Jolt、角色、触发和查询。公共 PhysicsWorld 尚无通用约束制作接口。
- 拟采用内容：声明形状和约束描述。通过注册组件及公共编辑服务创建并模拟。
- 采用方式：适配相同物理库的设计与算法。契合度：高。成本：4–7 工作日。
- 风险：高：负缩放、动态网格与约束生命周期。
- 验收：两个机械场景验证铰链。端点删除、非法轴和重启释放都正确。
- 阶段归属：`G13`。状态：候选。

### B10 脚本字段与调试制作流程

来源：[Hazel：EntityClasses / EntityScriptFields](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Scripting/ScriptEngine.cpp#L131)。
文件为 `Hazel/src/Hazel/Scripting/ScriptEngine.cpp`，定位行 131。

- 源码事实与价值：脚本类和公开字段可发现，程序集监视支持重载。
- Azure 现状：已有 Lua、可选托管后端、绑定和重载。需要完善编辑器的字段与诊断展示。
- 拟采用内容：提供脚本模板、类搜索、公开字段、源码揭示和重载诊断。
- 采用方式：参考体验并复用 ScriptRuntimeRegistry。契合度：高。成本：3–5 工作日。
- 风险：中：后端差异与字段版本迁移。
- 验收：Lua 与托管用例共用字段契约。类型变化、无后端和重载失败可恢复。
- 阶段归属：`U13`。状态：候选。

### B11 结构化问题列表

来源：[Fermion：onImGuiRender](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/ImGui/ConsolePanel.cpp#L43)。
文件为 `Fermion/Sources/ImGui/ConsolePanel.cpp`，定位行 43。

- 源码事实与价值：控制台提供滚动日志与基础清理命令。
- Azure 现状：已有日志筛选、复制、任务和错误展示。AI 提案也在控制台区域。
- 拟采用内容：分离日志、任务、问题与内容辅助。诊断包含文件、对象、操作和关联编号。
- 采用方式：参考排版；结构化诊断自主实现。契合度：高。成本：2–3 工作日。
- 风险：中：错误去重、定位和日志容量。
- 验收：点问题揭示目标。过滤与清除保留诊断来源，长日志维持预算。
- 阶段归属：`U13`。状态：候选。

### B12 面板上下文与扩展装配

来源：[Fermion：Context / Callbacks](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/ViewportPanel.hpp#L32)。
文件为 `Boson/src/Panels/ViewportPanel.hpp`，定位行 32。

- 源码事实与价值：面板接收上下文与有限回调，宿主组织生命周期。
- Azure 现状：已有 IEditorPanel、PanelContext、PanelDocumentView 与面板注册。内置绘制仍集中到层类。
- 拟采用内容：内置面板按职责持有 UI 状态。注入只读视图与操作服务，公开可注册入口。
- 采用方式：参考拆分；沿用公共编辑接口。契合度：高。成本：2–4 工作日。
- 风险：中：回调生命周期与插件状态所有权。
- 验收：一个独立扩展面板接入双项目。关闭后状态与回调生命周期确定。
- 阶段归属：`U8`。状态：候选。

### B13 AI 辅助接入新制作能力

来源：[Fermion：AI 辅助编程声明](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/README_CN.md#L5)。
文件为 `README_CN.md`，定位行 5。

- 源码事实与价值：README 声明 AI 辅助开发，检索范围内未发现模型提案或工具协议模块。
- Azure 现状：已有内容提案、领域适配器、观察注册和公共编辑契约。
- 拟采用内容：材质、纹理和物理新能力公开操作描述。领域提案调用相同校验与事务。
- 采用方式：本项目横向要求；非外部实现移植。契合度：高。成本：2–4 工作日。
- 风险：高：候选版本、资源规模和授权范围。
- 验收：固定响应、取消、过期、非法引用和失败恢复通过。关闭模型仍可制作。
- 阶段归属：`U13`。状态：候选。

### B14 Prefab 制作与覆盖面板

来源：[Hazel：Scene::Copy / DuplicateEntity](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Scene/Scene.cpp#L71)。
文件为 `Hazel/src/Hazel/Scene/Scene.cpp`，定位行 71。

- 源码事实与价值：场景复制说明持久标识与运行实例需要明确边界。
- Azure 现状：已有 Prefab 展开、实例覆盖、子树复制和内部引用重写。缺少集中制作界面。
- 拟采用内容：提供创建 Prefab、覆盖列表、单项还原与受控应用源资产。
- 采用方式：已有能力产品化；非外部 Prefab 移植。契合度：高。成本：3–5 工作日。
- 风险：高：源资产编辑与实例覆盖的事务关系。
- 验收：两个实例独立覆盖。源变更与失效字段报告清楚，撤销及保存重开一致。
- 阶段归属：`U13`。状态：候选。

## 可选与暂缓项

### C01 材质连接画布

来源：[Fermion：handleInteractions](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/MaterialEditorPanel.cpp#L384)。
文件为 `Boson/src/Panels/MaterialEditorPanel.cpp`，定位行 384。

- 源码事实与价值：节点创建、连线、删除及画布位置持久化。
- Azure 现状：已有着色器模块。独立材质资产由 U11 提案建设。
- 拟采用内容：先提供固定材质槽连接图。通用节点语言需单独定义类型、版本和编译诊断。
- 采用方式：可选接入上游画布；固定参数图适配。契合度：中。成本：5–9 工作日。
- 风险：高：用户期待与实际编译能力不匹配。
- 验收：非法连接被拒绝，画布重开与撤销正确。图与表单产生相同材质数据。
- 阶段归属：`U14`。状态：候选。

### C02 二维精灵与世界文字

来源：[Hazel：StartBatch / DrawString](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Renderer/Renderer2D.cpp#L271)。
文件为 `Hazel/src/Hazel/Renderer/Renderer2D.cpp`，定位行 271。

- 源码事实与价值：精灵、线、圆和多通道距离场文字采用批处理。
- Azure 现状：已有 RmlUi 游戏界面与编辑辅助。完整二维场景制作尚未列为当前目标。
- 拟采用内容：独立二维渲染模块组合精灵和世界文字。游戏界面继续使用现有服务。
- 采用方式：参考批处理；Vulkan 渲染重写。契合度：中。成本：8–14 工作日。
- 风险：高：排序、中文排版、纹理槽与资源预算。
- 验收：二维关卡与三维标签两个场景验证。透明排序、批次上限与中文明确验收。
- 阶段归属：`G14`。状态：候选。

### C03 异步 GPU 对象拾取

来源：[Hazel：ReadPixel](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazelnut/src/EditorLayer.cpp#L133)。
文件为 `Hazelnut/src/EditorLayer.cpp`，定位行 133。

- 源码事实与价值：整数对象缓冲提供可见片元的实体标识。
- Azure 现状：已有真实形变几何的 CPU 拾取。新增路径需证明遮挡或性能收益。
- 拟采用内容：按需读回对象标识，关联相机、文档和帧版本。保留 CPU 回退。
- 采用方式：参考对象标识；Vulkan 异步实现。契合度：中。成本：5–8 工作日。
- 风险：高：读回同步、迟到结果与透明语义。
- 验收：蒙皮、透明、MSAA、DPI 和多视图一致。过期读回不改变选择。
- 阶段归属：`R10`。状态：候选。

### C04 渲染图可视化与诊断

来源：[Fermion：compile / topologicalSort](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Renderer/RenderGraph/RenderGraphCompiler.cpp#L9)。
文件为 `Fermion/Sources/Renderer/RenderGraph/RenderGraphCompiler.cpp`，定位行 9。

- 源码事实与价值：编译器检查多生产者与环，错误包含 Pass 名称。
- Azure 现状：已有 Render Graph、资源状态、瞬态池与帧诊断。
- 拟采用内容：显示真实生产图的依赖、读写、寿命和耗时。诊断使用公共观察描述。
- 采用方式：参考错误展示；保留现有编译器。契合度：高。成本：3–5 工作日。
- 风险：中：大图快照和资源状态解释。
- 验收：环与非法资源夹具定位准确。查看和导出保持帧执行结果。
- 阶段归属：`U14`。状态：候选。

### C05 更多模型源格式

来源：[Fermion：ModelSourceImporter::importAsset](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Asset/Importer/ModelSourceImporter.cpp#L542)。
文件为 `Fermion/Sources/Asset/Importer/ModelSourceImporter.cpp`，定位行 542。

- 源码事实与价值：Assimp 转换模型源，生成模型、网格、材质和动画资产。
- Azure 现状：已有 glTF 与受控模型转换工具。格式扩展需实际素材需求。
- 拟采用内容：源格式适配器统一输出 glTF 与来源清单，运行时只消费统一产物。
- 采用方式：参考导入分解；可选工具适配。契合度：中。成本：6–10 工作日。
- 风险：高：单位、骨架、材质语义和格式许可。
- 验收：FBX 或 OBJ 与原生 glTF 比较。单位、轴、材质、动画和移动安装正确。
- 阶段归属：`G14`。状态：候选。

### C06 CPU 时间线导出

来源：[Hazel：WriteProfile](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Debug/Instrumentor.h#L75)。
文件为 `Hazel/src/Hazel/Debug/Instrumentor.h`，定位行 75。

- 源码事实与价值：作用域采样输出可供浏览器工具读取的时间线。
- Azure 现状：已有 CPU/GPU 计时和帧任务统计。需要时可补事件时间线。
- 拟采用内容：复用诊断通道，使用有界缓冲导出时间线。GPU 事件记录对应帧。
- 采用方式：小段格式与作用域适配。契合度：高。成本：2–3 工作日。
- 风险：中：锁、文件刷新和采样扰动。
- 验收：并发事件可解析，丢弃计数可见。关闭采样无持续开销。
- 阶段归属：`U14`。状态：候选。

### C07 自绘窗口标题栏

来源：[Fermion：DrawWindowButton / HandleWindowDrag](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Boson/src/Panels/MenuBarPanel.cpp#L199)。
文件为 `Boson/src/Panels/MenuBarPanel.cpp`，定位行 199。

- 源码事实与价值：菜单栏与最小化、最大化及窗口拖动组合。
- Azure 现状：已有原生窗口和停靠工作区。可用性重点为制作路径。
- 拟采用内容：仅在原生窗口体验存在已确认问题时立项。系统交互专项验收。
- 采用方式：暂缓；保留样式参考。契合度：中。成本：3–5 工作日。
- 风险：高：系统缩放、吸附、无障碍和多屏。
- 验收：多屏、DPI、最大化、系统吸附与键盘窗口操作全部通过。
- 阶段归属：`Deferred`。状态：候选。

### C08 整套底层模块替换

来源：[Fermion：AssetManagerBase](https://github.com/Yang-Junjie/Fermion/blob/eeac811ae4b6dd7827d24c255b4fa0241ac603af/Fermion/Sources/Asset/AssetManager/AssetManagerBase.hpp#L7)。
文件为 `Fermion/Sources/Asset/AssetManager/AssetManagerBase.hpp`，定位行 7。

- 源码事实与价值：编辑与运行资产接口包装同一静态管理器。
- Azure 现状：已有模块目标、实例服务、独立 Player 与设备回退。
- 拟采用内容：模块引入以独立能力和可替换契约为单位。ECS、RHI 与脚本保留现有主线。
- 采用方式：暂缓；仅作边界反例。契合度：低。成本：专项估算 工作日。
- 风险：高：全局状态、后端差异与已有承诺。
- 验收：任何替换需双项目、独立 Player 和全部受影响门禁的对等证据。
- 阶段归属：`Deferred`。状态：候选。

## 适合直接移植的局部实现

| 实现 | 直接采用边界 | 必要适配 |
| --- | --- | --- |
| ImViewGuizmo | 有许可的上游单头控件 | 主题、DPI、相机和输入占用 |
| 三轴属性行 | 表格排版、轴按钮和重置计算 | 元数据默认值、编辑事件和事务 |
| 基础几何生成 | 顶点、索引与局部几何算法 | 绕序、切线、预算和生成器输出 |
| 固定材质画布 | 节点布局与连接交互 | 注册槽位、资产文档和历史 |
| 时间线导出 | 事件格式与作用域计时概念 | 有界缓冲、JSON 转义和既有诊断 |

直接采用也需通过生产接口与来源核验。
控件返回编辑结果，由服务负责写入。
几何算法输出引擎资产，由管线负责资源上传。
第三方控件只进入编辑器构建目标。

## 需要重写服务的内容

内容浏览器依赖项目资产与虚拟路径。
可借鉴目录树和网格排版。
文件操作由公共资产服务执行。
类型化打开由注册表提供。

材质编辑依赖资产类型、版本和运行解释。
Fermion 的固定连接图可作为交互参考。
Azure 需同时覆盖 PBR 与风格材质。
着色器编译继续使用既有模块服务。

预览服务复用当前离屏渲染与缓存。
提供者新增材质、图片和领域资产。
缓存键记录资产及依赖指纹。
资源释放遵守 GPU 完成时序。

## 源码中应规避的实现边界

| 源码位置 | 观察事实 | Azure 实施规则 |
| --- | --- | --- |
| Fermion 视口开头 | 悬停调用 SetWindowFocus | 点击和捕获显式确定焦点 |
| 两仓库属性面板 | 部分控件直接改组件 | 写入经 EditService 与事务 |
| Fermion 材质面板头文件 | 固定 512 字节缓冲配合 strcpy | 使用已有动态 UTF-8 输入 |
| Fermion 内容浏览器 | 每帧枚举目录，按扩展名分派 | 使用缓存目录模型与注册描述 |
| Fermion 资产管理 | 编辑与运行包装共用静态状态 | 服务按会话拥有状态与能力 |
| Hazel Scene::Copy | AllComponents 列表驱动复制 | 复制与序列化消费组件注册 |
| Hazel 拾取、Fermion 拾取 | 同步 ReadPixel 路径 | GPU 拾取按需异步并检查版本 |
| Fermion 缩略图 | 缓存主要以路径或标识为键 | 依赖指纹和容量进入缓存契约 |
| Fermion 纹理默认设置 | sRGB 默认关闭 | 默认值来自纹理用途和通道语义 |
| Fermion 菜单构建 | UI 中组织文件复制 | 构建任务使用已有发布服务 |

Fermion 的 RenderGraphCompiler 检查多生产者和环。
其资源验证参数未用于完整资源存在性检查。
执行器按顺序提交图命令并回收瞬态帧缓冲。
这些源码不能直接证明 Vulkan 同步契约。

Fermion 的材质 compileMaterial 组装 MapAssets。
它把固定槽位参数交给 MaterialFactory。
源码未在该路径生成通用着色器程序。
通用材质图编译器需要单独设计与验收。

Fermion README 中的 AI 指 AI 辅助编程。
本轮检索未发现模型接入或提案执行模块。
B13 来源于 Azure 的既有 AI 原生约束。
它作为新制作功能的横向契约。

## 计划覆盖与执行入口

核心与推荐项进入九个候选阶段。
可选项进入三个专项候选阶段。
C07 与 C08 保持暂缓。
具体顺序、接口、目标文件与门禁见配套计划。

每项保留来源提交、文件和行号。
机器清单同时记录文件哈希与阶段归属。
后续实施只将有验证证据的项目标记完成。
当前所有实施条目保持 Planned 或 Deferred。
