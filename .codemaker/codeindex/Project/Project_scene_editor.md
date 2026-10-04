---
type: "Fragment"
id: Project/scene_editor
title: "场景数据与编辑器"
description: "`.azscene` 文档保存什么、迁移到什么版本，编辑器如何经会话与快照历史修改它，运行期如何把节点解析成世界变换与实例。"
parent: /Project/_overview.md
fragment: scene_editor
entity_names:
  constants:
    - name: SceneDocument::kSchemaVersion
      value: "3"
      source: Project/AzureRender/src/editor/SceneModel.hpp
    - name: SceneType::Character
      value: "0"
      source: Project/AzureRender/src/extensions/SceneType.hpp
    - name: SceneType::Blackhole
      value: "1"
      source: Project/AzureRender/src/extensions/SceneType.hpp
    - name: SceneType::Sample
      value: "2"
      source: Project/AzureRender/src/extensions/SceneType.hpp
    - name: SceneType::Count
      value: "3"
      source: Project/AzureRender/src/extensions/SceneType.hpp
    - name: ecs::kInvalidEntity
      value: "0"
      source: Project/AzureRender/src/ecs/Entity.hpp
    - name: NameComponent buffer size
      value: "64"
      source: Project/AzureRender/src/ecs/Components.hpp
    - name: SceneLight::radius 默认
      value: "5.0"
      source: Project/AzureRender/src/editor/SceneModel.hpp
    - name: SceneLight::color 默认
      value: "[1.0, 1.0, 1.0]"
      source: Project/AzureRender/src/editor/SceneModel.hpp
    - name: SceneNode::scale 默认
      value: "[1.0, 1.0, 1.0]"
      source: Project/AzureRender/src/editor/SceneModel.hpp
    - name: TransformComponent::rotation 单位
      value: "degrees"
      source: Project/AzureRender/src/ecs/Components.hpp
    - name: Undo 历史快照上限
      value: "100"
      source: Project/AzureRender/docs/assets-and-editor.md
retrieval_hints:
  - "一份 .azscene 文件里能存哪些字段，旧版本怎么读进来？"
  - "编辑器 Ctrl+Z 撤销到什么状态，历史什么时候被清空？"
  - "父节点移动后子节点和挂在节点上的点光源怎么跟着走？"
  - "保存场景为什么不会写出半个文件？"
  - "Asset Browser 手动 Reload Assets 做了什么，为什么会停顿？"
  - "⚠️ 你要找的是 RenderSettings 字段本身，不在这里，在 Project_render_core"
  - "⚠️ 你要找的是编辑器 Panel 的 ImGui 绘制代码归属，不在这里，在 Project_host 的编辑器层"
  - "本子系统的民间叫法是『场景文档 / Scene Document / Outliner / Inspector』，需求里的『关卡』『层级』对应这里"
  - "新增可序列化的场景字段必须落在 SceneModel 的读写与往返测试里，不得由 Renderer 自行解析 JSON"
architectural_role: "数据与交互层，场景文档的唯一序列化入口，禁止被渲染路径直接写入"
---

## 业务意图

这一层解决"美术与开发者摆出来的东西能否原样存下来、原样读回来、并在运行时以同一套变换渲染"的问题。文档对象负责序列化，渲染器只消费每帧的场景快照；编辑器把节点、光源、可见性与设置的变化收进可撤销的历史，保存时以原子替换保证进程中断也不会留下半个损坏的场景。

## 对外接口

| 接口 | 方向 | 关键字段 | 业务说明 | 入口符号 |
|------|------|---------|---------|---------|
| `.azscene` v3 | 磁盘↔内存 | `sceneId`, `resources[]{id,type,path}`, `nodes[]{id,name,parentId,resourceId,visible,translation,rotation,scale,prefabSource,instanceOf}`, `lights[]{id,nodeId,color,intensity,radius,enabled}`, `renderSettings` | 多资源多节点的权威载体，内嵌 `renderSettings` | `src/editor/SceneModel.cpp` |
| `SceneDocument::load` | 磁盘→内存 | `path` | 保存资产路径与设置；未知未来版本拒绝，v1/v2 迁移后光源列表为空 | `src/editor/SceneModel.cpp:load` |
| `SceneDocument::save` | 内存→磁盘 | `path` | 空路径抛 `invalid_argument`；`fromAsset` 的资产路径同样不得为空 | `src/editor/SceneModel.cpp:save` |
| `SceneDocument::fromAsset` | 单资产→场景 | `assetPath` | 单资源单节点退化场景，根节点使用稳定 `root` ID | `src/editor/SceneModel.cpp:fromAsset` |
| `sceneTypeFromString` | 名字→枚举 | `character`/`blackhole`/`sample` | 未知名在解析期抛，配置错误不在渲染期才暴露 | `src/extensions/SceneType.hpp` |
| `resolveNodeWorldTransforms` | 快照→矩阵数组 | `scene`, `cycleCount`（可空） | 返回与 `nodes` 等长且索引一致的世界矩阵数组 | `src/scene/TransformSystem.hpp` |
| `EditorSession::execute` | 界面→文档 | `EditorCommand{Save,ResetLayout,Reload,Undo,Redo,ReloadAssets,Capture}` | 返回 `false` 时 `lastError()` 携带可展示原因 | `src/editor/EditorSession.cpp` |
| `EditorContext` | 会话↔文档 | `scene()`, `renderSettings()`, `selectedNode()`, gizmo TRS, `dirty()` | 唯一持有可编辑文档，把节点与光源同步进 ECS | `src/editor/EditorContext.cpp` |
| `SceneDescription` / `SceneInstance` | 文档→Renderer | 资源列表、节点列表；实例含模型矩阵、世界包围体、来源顺序、资源键 | 逐帧重建，命令录制只读已完成列表 | `src/scene/SceneDescription.hpp` |

## 跨模块依赖

| 依赖 | 引用原因 | 关键符号 | confidence |
|------|---------|---------|------------|
| `Project_render_core` | `SceneDocument` 内嵌 `RenderSettings` 并保持其版本语义 | `RenderSettings`, `validateRenderSettings` | extracted |
| `Project_host` | 编辑器视口纹理、Gizmo 屏幕数据与 Capture 由宿主驱动 | `EditorContext`, `updateGizmoScreenData` | extracted |
| `Project_renderer_sdk` | `RendererSceneState.sceneState()` 提供可选资产与选择状态 | `RendererSceneState` | extracted |
| `Project_character` | 资源与快照在角色渲染器里实例化，`nodeId` 解析点光源世界位置 | `CharacterSceneRenderer` | extracted |
| `src/ecs` | Entity/Component 是编辑器与渲染快照的中间存储 | `World`, `ComponentArray` | extracted |
| `third_party/imgui`（docking 1.92.8） | Outliner/Inspector/Asset Browser 控件 | `IEditorPanel` | extracted |

> 反向依赖（谁调用了本子系统）：

| 调用方 | 调用场景 | 关键符号 |
|--------|---------|---------|
| `Project_host` | `--scene` / `--create-scene` / `--editor` 启动路径 | `SceneDocument::load`, `fromAsset` |
| `Project_character` | 加载快照并解析变换，供主相机与阴影相机分别剔除 | `SceneDescription`, `FrustumPlanes` |
| `tests/SceneModelTests`、`tests/SceneGraphTests`、`tests/EditorSessionTests` | 迁移、往返、撤销、变换与光源关联 | `SceneDocument::save`, `resolveNodeWorldTransforms` |

## 典型调用链

```
--editor path.azscene → parseCommandLine → AzureRenderApp
  → SceneDocument::load →（v1/v2 迁移）→ SceneDocument
    → EditorContext(document, scenePath) → syncToEcs()      ← 本子系统入口
      → World / TransformComponent / LightComponent::emitters
      → EditorSession::execute(…Command) → markDirty / beginEdit
      → 新编辑清空 Redo；reload() 清空整段历史；历史上限内保留完整 SceneDocument 快照
    → RenderContext.scene = SceneDescription 快照
      → ISceneRenderer::onLoad → CharacterSceneRenderer 按 resourceId 复用共享 GPU 资源
        → 每帧 resolveNodeWorldTransforms → 世界包围体 → 主/阴影相机两趟可见集
    → Save → saveDocumentAtomic（同目录唯一临时文件 → flush → rename 覆盖）
```

## 实现约束清单

### 必须定义的常量/枚举

| 标识符 | 值 | 所在文件 | 说明 | 约束由来 |
|-------|----|---------|------|---------|
| `SceneDocument::kSchemaVersion` | `3` | `SceneModel.hpp` | 权威版本（含 `lights[]`） | v1/v2 → 空光源列表；未来版本拒绝加载 |
| `node.id` | 字符串，文档内稳定 | `SceneModel.hpp` | 父子与光源关联以此为准 | 父 ID 找不到按根节点处理，空 ID 不建立父链接 |
| `SceneLight::nodeId` | 字符串 | `SceneModel.hpp` | 位置只由节点变换提供 | 保存/读取必须保持关联不变 |
| `prefabSource` / `instanceOf` | 仅存引用与 Transform 覆盖 | `SceneModel.hpp` | 独立的 Prefab 文件展开尚未实现 | 防止把引用当成已实现的展开语义 |
| `ecs::kInvalidEntity` | `0` | `Entity.hpp` | 0 保留为无效句柄 | 防止默认构造实体被当作有效实体渲染 |
| `NameComponent::name` | `64` 字节 | `Components.hpp` | 定长缓冲保持可平凡拷贝 | 变长字符串会破坏 `ComponentArray` 的存储假设 |
| Undo 历史上限 | `100` | `EditorContext.cpp` | 快照数量上限 | 见 `docs/assets-and-editor.md` |

### 必须包含的协议字段

| 命令/文件 | 字段 | 类型 | 说明 |
|-----------|------|------|------|
| `.azscene` v3 | `lights[].id` | string | 光源稳定标识，聚簇上传按此排序 |
| `.azscene` v3 | `lights[].nodeId` | string | 关联场景节点 |
| `.azscene` v3 | `lights[].color/intensity/radius/enabled` | float×3 / float / float / bool | 外观与作用范围，默认 `1,1,1 / 1.0 / 5.0 / true` |
| `.azscene` v3 | `nodes[].visible` | bool | 可见性，默认 `true` |
| `.azscene` v3 | `renderSettings` | object | 内嵌设置，`sceneType` 决定运行哪个 Renderer |

### 存档字段索引（不可裁减）

| 字段 | 说明 |
|------|------|
| `sceneId` / `resources[].id` / `nodes[].id` / `nodes[].resourceId` | 资源与节点交叉引用，任一缺失即无法重建场景 |
| `nodes[].parentId` | 层级；与 `resolveNodeWorldTransforms` 的循环检测语义绑定 |
| `lights[].nodeId` | 光源随动；丢失会造成光源世界位置错误 |
| `nodes[].prefabSource` / `instanceOf` | 目前仅承载引用与 Transform 覆盖 |

### 边界约束（能做 / 禁止）

- 禁止：Renderer 写回 `SceneDocument`，或直接从文档读 Vulkan 状态；渲染只读逐帧快照。
- 禁止：为了"兼容"而静默忽略未知字段或按旧默认值解释新数据。
- 禁止：`.azscene` 写入本机绝对路径作为必需依赖。
- 禁止：命令录制期间修改实例列表；GPU 实例缓冲按帧槽分配，CPU 写入前须等对应提交完成。
- 允许：节点循环时保留循环节点局部变换并由诊断/测试报告循环数，渲染不递归崩溃。
- 允许：父 ID 缺失按根节点处理。
- 边界：热重载在主线程记录请求，到安全帧边界等 Device Idle，再依次 `onUnload`/重载/`onLoad`；不修改 `.azscene` 路径，也不自动替换 Missing 资源；仅适合人工低频操作，不做后台监听。
- 边界：Capture 标签只允许字母、数字、连字符与下划线，输出到本地 `captures/`，它不会被识别为作品集证据；正式交付物的命名与哈希约束见 `Project_build_release`。

### 设计决策

| 决策点 | 选定方案 | 备选方案 | 选定理由 |
|--------|---------|---------|---------|
| 历史粒度 | 完整 `SceneDocument` 快照 | 差量历史 / 资源级事务 | 当前场景规模下实现最简且语义确定；大场景需换差量方案 |
| 序列化归属 | 文档对象负责序列化，渲染器只读快照 | 渲染器直接读 JSON | 编辑期与运行期解耦，Renderer 不依赖文件格式 |
| 光源位置来源 | 只由节点世界变换提供 | 光源自带世界坐标 | 保证编辑器、渲染与保存三处来源一致 |
| 保存方式 | 先写同目录唯一临时文件，确认字节一致后 `rename` 替换 | 原文件覆写 | 进程崩溃不产生半损文件；非写入者路径不触碰目标 |

## 变更风险

- 改 `.azscene` 字段或默认值：历史文件读不出来或被误解释；Schema 与往返测试不同步时 CI 与真实场景会分叉。递增只在语义不兼容时发生（来源 commit：`完成 AR-5.2 场景原子保存`、`feat(r0): 收口运行期场景数据`）。
- 改 `resolveNodeWorldTransforms`：Character、编辑器与点光源聚簇同时改变；须用父节点移动回归"子节点与附属光源随动、保存重开结果一致"。
- 改 `EditorCommand` 与快捷键映射：Undo/Redo、Reload、Capture 的用户语义会随之漂移，须重跑 `tests/EditorSessionTests.cpp`。
- 改资产路径解析：`assets_public` 在开发树、构建树与安装树的定位规则改变后需 `--check-resources` 验证。

> 📄 本节内容来源于仓库内置文档：`Project/AzureRender/docs/assets-and-editor.md`、`Project/AzureRender/docs/runtime/scene-data.md`、`Project/AzureRender/docs/reference.md`（原文已提炼，非完整转录）
