# 编辑器产品化与基础制作实施计划

> 文档类型：正式实施计划
> 日期：2026-10-10
> 状态：Active，U8/U9 Complete，G12 实施中
> 执行方式：按阶段顺序实施，每阶段独立验收与提交
> 依据：[Hazel 与 Fermion 借鉴评估](../research/2026-10-09-hazel-fermion.md)
> 约束：仓库根目录的 `AGENT.md`
> 清单：[三十四项候选](hazel-fermion-manifest.json)

## 目标与架构

目标是让用户完成实际项目的内容制作与交付。
路径覆盖新建、导入、组织、编辑、预览与发布。
编辑器提供可发现的操作和明确的失败结果。
每项能力可用于探索项目与场景检视项目。

基础继续采用现有模块、注册表和编辑服务。
项目内容使用 UUID、虚拟路径和版本化文档。
界面消费只读快照并调用公共操作。
渲染预览与运行会话负责各自资源生命周期。

技术基础为 C++、Vulkan、Dear ImGui 和 Jolt。
脚本使用 Lua 与可选托管后端。
ImViewGuizmo 是可核验的新增控件候选。
节点画布与 Assimp 属于可选专项。

各阶段定义实施接口与验收责任。
现行 API 以源码与运行时文档为准。
候选清单保留三十四项的全部来源与评估。
本轮执行全部主线阶段及游戏验收阶段。

## 全局约束与执行前置

1. 引擎能力使用类型、配置、系统与模块组合。
2. 具体角色、关卡和任务规则归项目内容。
3. UI、脚本和自动工具消费相同公共操作。
4. 写入验证文档版本、只读规则与引用类型。
5. 连续编辑支持合并，批量操作有失败恢复。
6. 数据格式声明版本、默认值和迁移规则。
7. 新资产服务复用 AssetDatabase 与虚拟路径。
8. Player 构建保持独立于编辑器和控件依赖。
9. 既有视觉、性能、设备和发布门禁持续有效。
10. 来源协议、版权和修改记录进入安装清单。

执行前核对 P4 与角色渲染工作的实际状态。
P4 实体键鼠复核继续按其验收表记录。
当前源码重建后再生成新的性能与视觉证据。
已有在途修改按任务归属管理。

本计划阶段代码为正式实施编号。
正式启动时由开发总计划维护唯一 Active 阶段。
P4 与角色渲染计划的在途事项各自保留。
用户明确执行后，按正式排期连续推进。

每阶段完成同步 README、运行说明与变更记录。
更新候选清单的证据路径与完成提交。
提交前核验差异、暂存范围和本阶段门禁。
提交标题采用 `feat(<phase>): <中文摘要>`。

## 交互与数据约定

| 范围 | 拟议规则 |
| --- | --- |
| 大纲 | 单击选择，双击定位，F 定位选择集合 |
| 内容浏览器 | 单击选择资产，双击按类型打开 |
| 放置 | 拖到视口或使用显式放置操作 |
| 导航 | 右键拥有飞行输入，Shift 临时加速 |
| 工具 | 非导航状态下 W/E/R 切换操纵器 |
| 引用 | 支持搜索、拖放、吸管、清空和揭示 |
| 文档 | 保存只写对应文档，工作区状态按用户保存 |
| 场景设置 | 光照、环境和渲染配置归场景文档 |
| 项目设置 | 起始关卡、挂载和脚本配置归项目 |
| 用户偏好 | 布局、主题、键位和导航归用户 |
| 材质 | 源资产、实例覆盖和运行解释有明确边界 |
| 预览 | 默认保持编辑文档与未保存状态 |
| 运行 | Play 与 Simulate 有独立模式及共同停止契约 |

文件删除支持影响预览与确认。
默认拒绝破坏有效强引用的删除。
需要替换引用时，以显式批量操作表达。
操作先校验全体目标，再提交持久变更。

## 重点失败输入

| 输入或状态 | 预期结果 | 负责阶段 |
| --- | --- | --- |
| 中文长名称、窄面板与多屏 DPI | 内容可读、命中准确、拖放载荷有效 | U8/U9/U10/U12 |
| 磁盘失败、重名、外部移动与只读挂载 | UUID 稳定，失败恢复文件与数据库 | G12 |
| 混合值、失效引用与切换编辑目标 | 批量写入原子化，过期编辑被拒绝 | U9/U13 |
| 切项目、关闭预览与 GPU 在途提交 | 缓存可失效，资源在正确时序释放 | U11/U12 |
| 模拟启动失败、脚本重载与异常恢复 | 文档和会话归属正确，编辑态可恢复 | G13/U13/P5 |

## 阶段顺序与工作量

主线按表中顺序执行。
U9 的引用控件先消费现有资产目录。
G12 建立文件与导入契约后，U10 接入制作操作。
材质和预览依赖前述基础。

| 阶段 | 优先级 | 依赖 | 交付重点 | 候选范围 | 粗估工作日 |
| --- | --- | --- | --- | --- | --- |
| U8 工作区与项目入口 | P0 | 在途任务状态核对 | 职责分区、模板入口、面板装配 | A01、B06、B12 | 5–8 |
| U9 属性与引用体验 | P0 | U8 | 统一控件、吸管、混合值 | A02、A03、A04 | 4–7 |
| G12 内容操作与导入契约 | P0 | U9 | 文件事务、纹理设置、依赖诊断 | A07、A08、B05 | 8–14 |
| U10 内容浏览与基础创建 | P1 | G12 | 目录导航、类型打开、基础对象 | A05、A06、B04 | 5–8 |
| U11 材质制作与多类型预览 | P1 | U10 | 材质资产、槽位覆盖、预览 | A09、A10、A11 | 8–14 |
| U12 视口工具与辅助显示 | P1 | U11 | 正交方向控件、网格、速度反馈 | B01、B02、B03 | 5–9 |
| G13 物理模拟与制作机制 | P1 | U12 | 模拟会话、形状与约束 | A12、B09 | 6–10 |
| U13 完整制作工作流 | P1 | G13 | 文档、动画、脚本、Prefab、诊断 | B07、B08、B10、B11、B13、B14 | 8–14 |
| G15 探索体验与环境制作 | P1 | U13 | 动线、尺度、节奏与程序化材质 | 游戏专项 | 4–7 |
| G16 平台跳跃完整关卡 | P1 | G15 | 实测跳跃、承载、金币、重生与通关 | 游戏专项 | 6–10 |
| P5 产品化交付验收 | P0 门禁 | G16 | 双项目制作、发布与使用复核 | 全部主线条目 | 4–7 |

主线粗估为六十三至一百零八工作日。
估算按阶段范围和对应验收任务计算。
估算含阶段测试、失败处理与文档。
素材复杂度和在途功能收口会影响实际工期。

### 可选专项

| 阶段 | 前置 | 候选范围 | 启动条件 |
| --- | --- | --- | --- |
| U14 可视化制作与诊断 | P5；材质图依赖 U11 | C01、C04、C06 | 用户安排专项制作或诊断需求 |
| G14 二维内容与扩展导入 | P5、G12 | C02、C05 | 有明确二维或源格式使用项目 |
| R10 拾取专项 | P5、U12 | C03 | CPU 路径出现经测量确认的缺口 |
| Deferred | 专项准入 | C07、C08 | 系统交互或模块替换有充分证据 |

可选专项保留全部任务与验收依据。
它们不作为主线 P5 的完成前置。
C07 与 C08 单独保存暂缓理由。
阶段启动前固定其专项预算和交付范围。

## 公共契约与文件责任

下文路径相对 `Project/AzureRender/`。
新增文件用于明确责任，实施时保持同等边界。
现有类型通过受控扩展提供所需能力。
新增操作注册到当前 EditRegistry。

编辑操作名称和参数是实施目标。
共同结果沿用 `EditResult`。
共同版本沿用 `DocumentVersion`。
失败使用结构化错误并遵守恢复契约。

| 契约 | 状态所有者 | 消费者 |
| --- | --- | --- |
| ProjectOpenService | 项目会话与模板任务 | 欢迎页、菜单、自动工具 |
| PropertyEditorRegistry | 控件注册与元数据描述 | 检查器、设置与资产编辑 |
| ReferencePickerService | 当前引用编辑会话 | 属性控件、大纲、视口 |
| ContentOperationService | 磁盘事务与恢复日志 | 内容浏览器与自动工具 |
| AssetEditorRegistry | 按类型注册打开能力 | 内容浏览器与对象引用 |
| MaterialDocument | 材质数据与编辑历史 | 材质编辑器、渲染、Player |
| ThumbnailProviderRegistry | 类型预览描述 | 既有缩略图服务 |
| EditorViewDescriptor | 相机与视口状态 | 渲染、拾取、操纵器 |
| PreviewSessionMode | 已装配系统与输入策略 | 工具栏、运行与模拟 |
| EditorDocumentManager | 多文档版本、选择与历史 | 场景标签、保存和恢复 |
| StructuredDiagnostic | 来源与定位信息 | 问题列表、日志与观察服务 |

## U8：工作区与项目入口

文件：

- 修改 `src/editor/EditorWorkspace.hpp/.cpp`。
- 修改 `src/editor/EditorWorkspaceUI.cpp` 与 `EditorTheme.hpp/.cpp`。
- 修改 `src/editor/IEditorPanel.hpp` 与 `PanelContext.hpp`。
- 修改 `src/editor/panels/InspectorPanel.cpp` 与 `SettingsPanel.cpp`。
- 新建 `src/editor/projects/ProjectOpenService.hpp/.cpp`。
- 新建 `src/editor/panels/ProjectBrowserPanel.cpp`。
- 新建 `tests/EditorProjectFlowTests.cpp`。
- 新建 `tools/test_editor_productization.py`。

拟议接口：

`ProjectOpenService::create(templateId, destination)` 返回任务结果。
`open(path)` 返回项目切换结果。
`recent()` 返回已验证的最近项目记录。
切换消费 DocumentActionGuard 的用户决定。

工作区提供 `workspace.preset`，参数为 `id`。
项目操作为 `project.create` 与 `project.open`。
前者参数为 `templateId`、`destination`。
后者参数为 `path`，切换结果包含目标项目标识。

- [x] 建立新建、取消、重名和未保存切换的失败夹具。
- [x] 将模板生成接入现有任务及发布资源定位。
- [x] 完成欢迎页、最近项目与缺失项目恢复入口。
- [x] 完成制作、调试布局与四类设置职责分区。
- [x] 内置面板通过只读上下文与公共操作接入。
- [x] 验证独立扩展面板和双项目布局恢复。
- [x] 完成文档、清单与阶段提交。

复制模板采用临时目录并校验所有输出。
重名默认拒绝，取消回收本次临时产物。
最近项目属于用户配置，持久内容使用项目相对路径。
面板样式使用现有语义颜色及尺度。

验收覆盖 1280×720、1920×1080 和 2560×1440。
DPI 覆盖 100%、150%、200% 的既有组合。
文本、弹窗和跨面板悬停各验证焦点归属。
布局恢复损坏时使用可操作的默认布局。

## U9：属性控件、引用与批量编辑

文件：

- 修改 `src/editor/ui/Widgets.hpp/.cpp` 与 `UiMetrics.hpp`。
- 修改 `src/reflection/Registry.hpp/.cpp` 与 `Annotations.hpp`。
- 修改 `src/editor/panels/InspectorPanel.cpp`。
- 修改 `src/editor/commands/EditorOperations.cpp`。
- 新建 `src/editor/properties/PropertyEditorRegistry.hpp/.cpp`。
- 新建 `src/editor/properties/ReferencePickerService.hpp/.cpp`。
- 新建 `tests/PropertyEditingTests.cpp`。
- 新建 `tests/ReferencePickerTests.cpp`。
- 扩展 `tools/test_editor_productization.py`。

属性控件返回 `PropertyEditEvent`。
事件包含 `value`、`began`、`changed`、`ended`、`cancelled`。
数值范围、单位和默认值来自字段描述。
每个控件拥有稳定字段路径。

字段描述补充单位、精度与批量编辑策略。
组件默认值消费 ComponentRegistry 的 defaults。
特殊资产字段由所属类型提供默认值。
元数据变化同时验证生成注册与脚本绑定描述。

引用会话采用 `ReferencePickRequest`。
请求记录文档版本、字段路径、目标与合法类型。
`begin(request)`、`deliver(candidate)`、`cancel()` 管理会话。
引用写入最终消费 `component.field`。

新增 `component.batch-field`。
参数为 `nodes`、`type`、`field`、`value`。
新增 `reference.reveal`，参数为 `kind`、`id`。
批量修改前验证全部目标和共同字段策略。

- [x] 建立混合值、只读字段与部分非法目标失败夹具。
- [x] 实现公共属性行、三轴重置和连续编辑事件。
- [x] 实现搜索、拖放、吸管、清空与揭示。
- [x] 完成共同组件、混合值和空选择状态。
- [x] 将批量操作纳入统一事务与撤销。
- [x] 验证切目标、失焦、Esc 和过期引用请求。
- [x] 完成文档、清单与阶段提交。

引用下拉继续支持既有合法内容。
吸管按模式消费视口选择，不触发导航或定位。
不兼容字段以明确信息解释批量规则。
长 UTF-8 字符串使用现有动态控件。

## G12：内容文件事务与纹理契约

文件：

- 修改 `src/runtime/AssetDatabase.hpp/.cpp`。
- 修改 `src/runtime/AssetTypeRegistry.hpp`。
- 修改 `src/editor/AssetImportJob.hpp/.cpp`。
- 修改 `src/editor/commands/EditorOperations.cpp`。
- 新建 `src/editor/content/ContentOperationService.hpp/.cpp`。
- 新建 `src/assets/TextureImportSettings.hpp/.cpp`。
- 新建 `src/editor/panels/TextureAssetPanel.cpp`。
- 新建 `tests/ContentOperationTests.cpp`。
- 新建 `tests/TextureImportContractTests.cpp`。

内容操作分为 `plan(request)`、`apply(plan)` 与 `recover(journal)`。
计划记录项目、版本、源、目标和引用影响。
临时副本与事务日志支持中途失败恢复。
操作成功后刷新受影响资产及反向依赖。

注册操作包含：

| 操作 | 必需参数 | 结果 |
| --- | --- | --- |
| content.create-directory | parent、name | 虚拟路径 |
| asset.move | asset、destination | 影响计划与稳定 UUID |
| asset.rename | asset、name | 影响计划与稳定 UUID |
| asset.duplicate | asset、destination | 新 UUID 与重写引用 |
| asset.delete | assets、referencePolicy | 影响计划与删除结果 |
| asset.reimport | asset、settings、baseFingerprint | 导入任务标识 |
| asset.dependencies | asset、direction | 依赖快照 |
| texture.settings | asset、values | 设置文档与新指纹 |

预览影响使用 `dryRun`，确认应用使用计划标识。
计划标识关联版本和来源指纹。
撤销根据恢复日志处理内容和元数据。
源数据变化使过期计划明确拒绝。

纹理配置包含 `schemaVersion`、`usage`、`colorSpace`。
采样字段包含 `filter`、`wrapU/V`、`anisotropy`。
层级字段包含 `mipPolicy` 与用途相关处理。
设备能力限定各向异性上限。

颜色纹理按用途解释颜色空间。
法线层级重新归一化，遮罩保持覆盖语义。
导入器版本与设置参与派生产物指纹。
现有采样修复通过共同合同接入。

- [ ] 建立重名、只读、引用删除与磁盘故障失败夹具。
- [ ] 实现影响计划、日志、提交与恢复。
- [ ] 验证源文件、元数据与依赖的一致性。
- [ ] 建立纹理用途、采样和迁移夹具。
- [ ] 实现单项及可选择字段的批量设置。
- [ ] 验证取消重导入、失败恢复和依赖失效。
- [ ] 完成文档、清单与阶段提交。

复制资产默认生成新 UUID。
复制依赖树时先建立标识重写映射。
移动资产保留原 UUID 和合法引用。
发布包只包含已校验的派生产物与来源记录。

## U10：内容浏览器与基础创建

文件：

- 修改 `src/editor/panels/AssetBrowserPanel.cpp`。
- 修改 `src/editor/content/AssetCatalog.hpp/.cpp`。
- 修改 `src/runtime/AssetTypeRegistry.hpp`。
- 修改 `src/assets/GeneratorRegistry.hpp/.cpp`。
- 新建 `src/editor/content/ContentDirectoryModel.hpp/.cpp`。
- 新建 `src/editor/content/AssetEditorRegistry.hpp/.cpp`。
- 新建 `src/assets/generators/PrimitiveGenerator.cpp`。
- 新建 `tests/ContentNavigationTests.cpp`。
- 新建 `tests/AssetOpenTests.cpp`。
- 扩展 `tests/EditorAssetWorkflowTests.cpp`。

目录模型提供 `snapshot(mount, path, filter)`。
结果包含版本、目录、资产和诊断。
枚举通过缓存与增量刷新完成。
列表与网格使用可见范围裁剪。

`AssetEditorDescriptor` 包含 `type`、`label` 与 `openOperation`。
`asset.open` 参数为 `asset`。
返回目标编辑文档或预览句柄。
`asset.place` 保留明确射线和放置参数。

基础几何使用 `asset.generate`。
参数包括几何类型、尺寸与分段。
输出标准资产，并附生成来源及版本。
创建相机与光源使用注册组件模板。

- [ ] 建立目录导航与类型打开失败夹具。
- [ ] 实现目录树、面包屑、历史和缩略图尺寸设置。
- [ ] 接入 G12 的右键内容操作与影响预览。
- [ ] 完成资产选择、双击打开和显式放置语义。
- [ ] 接入基础几何与相机、灯光对象预设。
- [ ] 验证空目录、一万条资产及外部目录变动。
- [ ] 完成文档、清单与阶段提交。

大目录测试记录枚举次数、耗时和峰值内存。
验收需同时保持既有编辑器 CPU 预算。
目录模型只在来源或筛选版本变化时刷新。
文件管理器揭示通过平台服务传递路径。

## U11：材质资产、槽位与预览

文件：

- 修改 `src/runtime/AssetTypeRegistry.hpp`。
- 修改 `src/assets/GltfLoader.hpp/.cpp`。
- 修改 `src/editor/panels/InspectorPanel.cpp`。
- 修改 `src/editor/preview/AssetThumbnailService.hpp/.cpp`。
- 修改 `src/render/RenderViewService.hpp/.cpp`。
- 新建 `src/runtime/MaterialDocument.hpp/.cpp`。
- 新建 `src/editor/panels/MaterialAssetPanel.cpp`。
- 新建 `src/editor/preview/ThumbnailProviderRegistry.hpp/.cpp`。
- 新建 `tests/MaterialDocumentTests.cpp`。
- 新建 `tests/MaterialSlotEditingTests.cpp`。
- 新建 `tools/test_material_authoring.py`。

材质文档字段包含 `schemaVersion`、`typeId` 和 `parameters`。
贴图字段使用 UUID 与用途声明。
类型描述规定参数、贴图槽和运行解释。
PBR 与风格材质各自注册描述。

新增 `material.create`，参数为 `type`、`destination`。
新增 `material.field`，参数为 `asset`、`field`、`value`。
新增 `material.assign`，参数为 `nodes`、`slotId`、`material`。
新增 `material.reset-slot`，参数为 `nodes`、`slotId`。

独立资产文档的历史归文档会话。
对象的材质槽覆盖归关卡内容。
源模型槽位由稳定标识关联。
重导入输出槽位映射和失配诊断。

预览提供者返回 RenderViewDescriptor 或图片描述。
既有 ThumbnailKey 保留资产、指纹和设置。
依赖指纹包含关联纹理与材质解释版本。
缓存维持现有槽位和在途释放上限。

- [ ] 建立材质往返、非法参数与失效贴图失败夹具。
- [ ] 实现材质类型注册、参数表单和资源打开。
- [ ] 实现槽位赋值、重置与重导入映射。
- [ ] 接入材质球、图片与模型预览提供者。
- [ ] 验证依赖刷新、容量、关闭与切项目释放。
- [ ] 完成双类型材质的制作、运行和发布。
- [ ] 完成文档、清单与阶段提交。

首次交付以参数表单为主。
材质画布属于 U14 的可选任务。
风格参数来自注册描述和资产配置。
通用宿主消费材质类型与运行契约。

## U12：视口方向、网格与导航反馈

文件：

- 修改 `src/editor/EditorCameraController.hpp/.cpp`。
- 修改 `src/editor/viewport/EditorCameraService.hpp/.cpp`。
- 修改 `src/editor/ImGuiEditorLayer.cpp` 的视口调用。
- 修改 `src/render/CameraProjection.hpp`。
- 修改 `src/editor/input/EditorInputRouter.hpp/.cpp`。
- 新建 `src/editor/viewport/EditorViewDescriptor.hpp`。
- 新建 `src/editor/viewport/ViewportOverlayRegistry.hpp/.cpp`。
- 新建 `shaders/editor_grid.vert` 与 `editor_grid.frag`。
- 新建 `third_party/ImViewGuizmo/source.json` 与许可文件。
- 新建 `tests/EditorProjectionTests.cpp`。
- 扩展 `tools/test_editor_productization.py`。

视图描述包含 `projection`、`position`、`target`、`up`。
投影字段包括 `fov` 或 `orthoHeight`、`near`、`far`。
渲染、拾取、操纵器和构图读取同一描述。
方向切换使用 `viewport.orientation`，参数为 `axis`。

辅助提供者返回带类别的线、图标或网格描述。
每类支持隐藏、选中与全部显示策略。
辅助状态归工作区，内容数据归项目。
视口按住右键进入漂浮与环视模式。
鼠标移动以相机位置为中心改变朝向。
WASD 控制前后与侧向移动，E/Q 控制升降。
Shift 临时加速，滚轮调整基础飞行速度。
视口显示当前模式、速度和操作提示。
松开右键、按 Esc 或窗口失焦退出导航。
弹窗、预览、操纵器拖动期间由输入路由仲裁。
导航期间 W/E/R 用于移动，释放后恢复工具切换。
大纲单击选择，双击执行定位。

实现参考成熟引擎的编辑相机与输入捕获。
漂浮模式参考 Fermion 的 `EditorCamera::onUpdate`。
入口为 `Fermion/Sources/Renderer/Camera/EditorCamera.cpp`。
参考右键锁定、首帧差值和释放恢复的状态组织。
来源记录具体仓库版本、文件与许可。
公共相机服务支持不同项目和视口布局。
飞行调速消费现有输入路由与设置。

- [ ] 核验 ImViewGuizmo 文件许可、哈希与安装归属。
- [ ] 建立六向、正交构图及拾取一致性失败夹具。
- [ ] 接入方向控件和统一投影描述。
- [ ] 建立网格、相机和光源辅助提供者。
- [ ] 实现右键漂浮、原地环视与六向移动。
- [ ] 实现飞行滚轮调速与可读状态反馈。
- [ ] 验证右键方向、Shift 加速与工具快捷键仲裁。
- [ ] 验证松键、Esc 与失焦均终止输入捕获。
- [ ] 验证失焦、弹窗、拖出视口和尺寸变化。
- [ ] 完成文档、清单与阶段提交。

辅助工具命中区域先由输入路由消费。
悬停不强制改变活动面板。
六向切换保留合法中心与观察距离。
CPU 拾取先使用共同投影，GPU 专项另行安排。

## G13：独立模拟与物理制作

文件：

- 修改 `src/editor/EditorSession.hpp/.cpp`。
- 修改 `src/editor/EditorToolbar.hpp/.cpp`。
- 修改 `src/runtime/PhysicsWorld.hpp/.cpp`。
- 修改 `src/runtime/ComponentRegistry.hpp/.cpp`。
- 新建 `src/runtime/physics/PhysicsDescriptors.hpp`。
- 新建 `src/runtime/physics/PhysicsShapeFactory.hpp/.cpp`。
- 新建 `src/editor/panels/PhysicsAuthoringPanel.cpp`。
- 新建 `tests/PreviewSimulationTests.cpp`。
- 新建 `tests/PhysicsAuthoringTests.cpp`。

预览模式采用 `Edit`、`Play`、`Simulate`。
`preview.simulate` 与既有暂停、单步、停止共用会话入口。
模拟装配物理系统并保持编辑相机。
玩法脚本按模式策略显式装配。

形状描述支持盒、球、胶囊和静态网格。
运动类型和缩放合法性由描述校验。
约束描述先提供铰链、轴、端点和限制。
运行资源由 PhysicsWorld 持有。

- [ ] 建立模拟启动失败与编辑态恢复夹具。
- [ ] 实现模式装配、暂停、单步与停止。
- [ ] 拆分形状创建，扩展注册组件元数据。
- [ ] 实现形状和铰链制作及公共引用编辑。
- [ ] 加入碰撞与约束辅助显示。
- [ ] 验证端点删除、负缩放、非法轴与循环启停。
- [ ] 完成文档、清单与阶段提交。

两个验证项目为摆臂机构与旋转门机构。
它们使用同一铰链契约与不同参数。
门的玩法规则通过项目脚本表达。
物理模块只负责约束与模拟行为。

停止默认恢复编辑态。
运行结果应用需要另有显式操作及差异。
首轮模拟以恢复契约为完成条件。
任何结果写回均经编辑事务。

## U13：文档与完整制作工作流

文件：

- 修改 `src/editor/EditorSession.hpp/.cpp`。
- 修改 `src/editor/documents/DocumentActionGuard.hpp/.cpp`。
- 修改 `src/editor/panels/AnimationPanel.cpp` 与 `ConsolePanel.cpp`。
- 修改 `src/runtime/Prefab.hpp/.cpp`。
- 修改 `src/editor/ai/adapters/ProposalAdapters.cpp`。
- 新建 `src/editor/documents/EditorDocumentManager.hpp/.cpp`。
- 新建 `src/editor/panels/ScriptAuthoringPanel.cpp`。
- 新建 `src/editor/panels/PrefabAuthoringPanel.cpp`。
- 新建 `src/editor/panels/ProblemsPanel.cpp`。
- 新建 `src/diagnostics/StructuredDiagnostic.hpp`。
- 新建 `tests/EditorDocumentManagerTests.cpp`。
- 新建 `tests/AuthoringWorkflowTests.cpp`。

本阶段按六个可独立验证的任务推进。

| 任务 | 消费基础 | 拟议操作与参数 | 验证结果 |
| --- | --- | --- | --- |
| 文档工作区 B07 | 保存保护、备份与历史 | document.open(path)、activate(id)、save(id)、close(id) | 两关卡选择、版本和撤销相互独立 |
| 动画检视 B08 | 动画预览与状态机 | animation.play(node)、pause(node)、seek(node,time) | 两骨架独立播放与切对象恢复 |
| 脚本制作 B10 | 脚本注册与重载 | script.template(backend,path)、reveal(asset)、reload(backend) | Lua 与托管共用字段及错误契约 |
| 问题列表 B11 | 反馈、任务与诊断 | diagnostic.query(filter)、reveal(id) | 点击准确定位，来源与容量可查 |
| 领域提案 B13 | 提案、校验与操作描述 | 注册材质、纹理和物理领域 | 固定响应与普通 UI 产生同等结果 |
| Prefab 制作 B14 | 展开与覆盖 | prefab.create(nodes,path)、revert(instance,fields)、apply(instance,fields) | 源资产与实例覆盖各自可恢复 |

操作调用仍通过 EditRegistry。
独立文档拥有 EditService 与 DocumentVersion。
问题列表展示结构化字段和关联操作。
脚本模板按后端注册，不依赖固定类名。

Prefab 应用源资产采用跨文档事务。
先校验源版本和全部受影响实例。
用户能查看字段范围和引用影响。
过期源资产使应用明确拒绝。

- [ ] 为六项任务分别建立表中行为及失败夹具。
- [ ] 完成多文档会话和保存、恢复归属。
- [ ] 完成动画时间轴与脚本制作入口。
- [ ] 完成结构化问题与独立内容辅助面板。
- [ ] 完成 Prefab 覆盖列表及受控应用操作。
- [ ] 为新制作领域注册操作描述和提案适配器。
- [ ] 验证类型变化、源冲突、异常恢复和模型关闭。
- [ ] 完成文档、清单与阶段提交。

## G15：探索体验与环境制作

探索关卡先完成完整运行与通关记录。
记录起点、目标、岔路及关键路径。
测量通道宽度、移动时间、跳距和场景尺度。
相机、碰撞和速度问题进入可复现用例。

文件与职责：

- `assets_public/exploration/` 保存关卡与应用规则。
- `tools/generate_exploration_environment.py` 生成环境。
- `tools/test_exploration_completion.py` 验证完整流程。
- `docs/acceptance/g15/` 保存游玩与性能证据。

- [ ] 完整游玩并建立路线、距离及时间基线。
- [ ] 调整探索、观察、移动和互动节奏。
- [ ] 通过地形、光照、颜色和地标引导路径。
- [ ] 配置地面、岩石、植被和建筑残骸。
- [ ] 检查近中远景、重复、穿插、悬浮与比例。
- [ ] 优先使用本地 Blender 与脚本生成素材。
- [ ] 记录生成输入、工具版本与资源来源。
- [ ] 验证 Albedo、Normal 和 Roughness 的解释。
- [ ] 补齐实际需要的材质能力或兼容路径。
- [ ] 运行通关并复测相机、碰撞及场景性能。
- [ ] 更新设计、操作说明并提交阶段成果。

## G16：平台跳跃完整关卡

平台关卡接在探索通关之后。
它也提供独立启动与重新开始入口。
角色、场景与资源机制复用公共服务。
胜负和关卡规则归项目脚本或应用模块。

文件与职责：

- `assets_public/platformer/` 保存独立项目与关卡。
- `src/runtime/PhysicsWorld.cpp` 负责承载及碰撞。
- `tools/measure_character_jump.py` 输出实测参数。
- `tools/generate_platformer.py` 消费参数生成关卡。
- `tools/test_platformer_completion.py` 验证游玩流程。
- `docs/acceptance/g16/` 保存测量与通关证据。

### 测量与布局

- [ ] 实测速度、跳高、滞空和水平最大距离。
- [ ] 记录碰撞体尺寸、输入延迟与移动惯性。
- [ ] 联合验证每个必经跳跃的水平距离与高度。
- [ ] 基础间隙采用实测最大距离的 45%–60%。
- [ ] 常规间隙采用 60%–75%，并验证高低差。
- [ ] 困难间隙采用 75%–85%，提供清晰落点。
- [ ] 前期落点宽阔，后期尺寸考虑碰撞与惯性。
- [ ] 按认识、练习、组合、挑战和奖励安排节奏。
- [ ] 主路清晰，金币提供少量可选风险路线。
- [ ] 控制连续困难跳跃和单一机制重复次数。

### 机制与反馈

- [ ] 实现移动、跳跃、落地及稳定地面检测。
- [ ] 验证静止平台、边缘判定和掉落检测。
- [ ] 配置水平及少量垂直移动平台。
- [ ] 平台明确起终点、速度、周期及停留时间。
- [ ] 首个平台运动缓慢，允许安全观察和练习。
- [ ] 验证移动平台承载和运动期间碰撞。
- [ ] 配置金币视觉、拾取范围及收集反馈。
- [ ] 提供检查点、失败反馈和快速重试。
- [ ] 终点门远距离可辨识，并触发通关表现。
- [ ] 配置灯光、动画、音效或粒子通关反馈。
- [ ] 提供金币数量、完成提示及重开界面。
- [ ] 配置跳跃、落地、金币、重生和通关音效。
- [ ] 控制音量并抑制连续重复播放。
- [ ] 验证探索切换、独立启动及最终完成流程。
- [ ] 验证重生与切换后的对象及资源释放。
- [ ] 提供碰撞可视化、运动参数和性能检查。
- [ ] 更新设计、操作说明并提交阶段成果。

### 游戏整体验收

两个 Demo 均执行真实运行验证。
自动验证通过正式输入驱动角色运动。
替代验证记录执行路径和未验证的部分。
截图只用于布局与视觉检查。

必经平台须具备实际可达性证据。
检查穿模、卡住和掉落后的恢复行为。
收集、承载、音效及通关均验证完整流程。
新增机制也通过独立工作流验证复用。

## P5：产品化交付验收

修改测试与构建记录，创建 `docs/acceptance/p5/`。
更新 README、使用教程、操作参考与发布清单。
双项目包含角色探索与机械场景检视。
两个项目均从可交付模板创建。
探索与平台游戏同时执行完整流程回归。

### 用户制作任务

- [ ] 首次启动并创建项目，选择起始关卡。
- [ ] 导入模型与纹理，建立内容目录。
- [ ] 移动和重命名资产，验证引用保持。
- [ ] 创建基础几何、光源与相机。
- [ ] 创建两类材质并赋给不同子网格。
- [ ] 编辑多选字段，使用对象吸管与资源揭示。
- [ ] 创建 Prefab，分别修改两个实例覆盖。
- [ ] 播放动画、编辑脚本字段并检查失败诊断。
- [ ] 建立物理机构并运行独立模拟。
- [ ] 保存两份文档，退出重开和恢复备份。
- [ ] 打包项目，移动目录并启动独立 Player。
- [ ] 执行实体键鼠操作并记录实际结果。

每项保存 UI 输入、公共操作与内容哈希。
截图用于核验布局和预览。
输入回放验证生产交互。
实体键鼠复核单独记录操作者与结果。

### 回归和预算

Debug 与 Release 分别完成全部 CTest。
GPU 门禁按串行顺序执行。
记录独显与 Intel 核显的能力及释放结果。
构建来源包含源码与着色器哈希。

沿用 P4 的正式预算：
GPU P95 至多 16.6 毫秒。
Player CPU P95 至多八毫秒。
编辑器 CPU P95 至多十毫秒。

物理 P95 至多两毫秒。
帧丢弃、工作集与设备本地容量采用既有规定。
负载与测量方法沿用正式性能验收。
新增大目录与预览负载另附局部计时。

许可与来源进入安装包。
隔离 PATH、移动安装和缺失输入目录均验证。
所有失败项修复复测后才能标记 Complete。
候选清单记录测试、环境、证据和阶段提交。

## 可选任务的实施边界

### U14：材质画布、渲染图与时间线

C01 新建 `src/editor/graph/MaterialGraphModel.hpp/.cpp`。
画布控件只负责节点、连接和选择。
图模型保存版本与稳定节点标识。
固定连接图生成 U11 的材质文档。

C04 新建 `src/editor/panels/RenderGraphPanel.cpp`。
消费现有 RenderGraph 的只读编译快照。
显示 Pass、资源读写、寿命和计时。
失败信息连接实际 Pass 与来源。

C06 新建 `src/diagnostics/TraceExport.hpp/.cpp`。
消费有界事件缓冲，并导出标准时间线 JSON。
线程、帧与 GPU 提交关联有明确标识。
转义、丢弃计数与采样成本进入验收。

- [ ] 建立非法图连接、损坏布局和重开夹具。
- [ ] 核验节点编辑器的版本、许可与构建。
- [ ] 实现画布与材质表单的一致数据转换。
- [ ] 验证图诊断和并发事件导出。
- [ ] 单独保存收益、开销和完整回归证据。

### G14：二维内容与模型源适配

C02 新建 `src/render/Renderer2D.hpp/.cpp`。
精灵、文字和图元各声明排序及批次契约。
纹理槽上限来自设备能力。
中文排版覆盖字形选择、换行和组合需求。

C05 新建 `src/assets/importers/ModelSourceAdapter.hpp/.cpp`。
工具转换输出统一资产与来源清单。
运行时加载继续消费同一格式。
缓存记录转换器版本、单位与轴配置。

- [ ] 分别定义二维与源格式专项范围。
- [ ] 核验依赖、素材许可和支持设备。
- [ ] 验证二维关卡与三维标签复用。
- [ ] 验证源转换与原生 glTF 的内容一致性。
- [ ] 保存纹理、动画、内存和发布回归证据。

### R10：对象拾取专项

新建 `src/editor/viewport/ObjectPickService.hpp/.cpp`。
按需提交像素或区域读回请求。
结果附文档、视图、相机与帧版本。
生产选择继续经 SelectionService 应用。

- [ ] 保存 CPU 路径的性能或遮挡缺口证据。
- [ ] 定义透明、蒙皮、MSAA 和工具标识策略。
- [ ] 实现异步读回与支持设备回退。
- [ ] 验证迟到结果、窗口变化和跨文档请求。
- [ ] 证明收益并保留全套选择与性能门禁。

C07 和 C08 的来源与评估保留在研究清单。
专项启动需要明确范围与对等验收方案。

## 验证命令与记录方式

以下命令在 `Project/AzureRender/` 下运行。
Windows 构建需已配置 MSVC 与 vcpkg 环境。
新测试注册名称与下列阶段表达式一致。
各阶段执行对应构建、行为及回归门禁。

构建和完整 CPU/GPU 回归：

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --test-dir build/ninja-msvc-debug --output-on-failure
cmake --preset msvc-release
cmake --build --preset msvc-release
ctest --test-dir build/ninja-msvc-release --output-on-failure
```

局部阶段测试：

```powershell
ctest --test-dir build/ninja-msvc-debug -R "AzureEditor.(ProjectFlow|PropertyEditing|ReferencePicker|ContentOperation|TextureImport|ContentNavigation|AssetOpen|MaterialDocument|MaterialSlot|EditorProjection|PreviewSimulation|PhysicsAuthoring|DocumentManager|AuthoringWorkflow)" --output-on-failure
ctest --test-dir build/ninja-msvc-debug -R "AzureEditor.(UsabilityGpu|DocumentGuardGpu|ContentGpu|NativeInputGpu)" --output-on-failure
```

执行前使用 `ctest -N` 核验新测试已注册。
匹配为零个测试时，该阶段检查视为失败。
生产输入任务注册 `AzureEditor.ProductizationGpu`。
其测试按双项目生成可审阅证据。

文档检查：

```powershell
python tools/check_doc_style.py docs/research/2026-10-09-hazel-fermion.md docs/plans/2026-10-09-editor-productization.md
python -m mkdocs build --strict --site-dir build/docs-productization
```

局部测试的预期结果为零失败。
完整阶段记录实际发现数和执行数。
每项证据同时记录来源、配置、设备及日期。
测试名单与候选清单保持一一对应。

## 完成定义与计划自检

每项完成需同时具备以下证据：

- 公共契约、数据版本与使用文档。
- 探索项目与独立工作流的复用结果。
- UI、自动工具和受影响脚本入口的一致性。
- 保存重开、撤销、失败恢复与过期拒绝。
- 性能、视觉、设备及发布范围的对应检查。
- 来源归属、安装清单和独立阶段提交。

计划已覆盖十二项核心和十四项推荐。
八项可选或暂缓均保留执行边界。
五类重点失败输入各有任务归属。
现有交互约定与阶段状态保持独立可查。

## 实施证据

U8 验收见[项目工作区记录](../acceptance/u8/2026-10-10.md)。
U9 验收见[属性制作记录](../acceptance/u9/2026-10-10.md)。
G12 为当前实施阶段。
其余主线阶段按计划顺序执行。
