# 编辑操作与文档事务

> 文档类型：开发参考
> 状态：生产接口
> 更新日期：2026-10-06
> 适用范围：Windows 编辑器与自动工具

## 操作入口

`EditorSession` 持有文档与 `EditService`。
菜单、快捷键、面板与自动命令使用同一服务。
领域处理器注册到 `EditRegistry`。
`describe()` 返回操作标识、版本和参数模式。

编辑服务在宿主线程执行。
调用方在该线程维护候选、世界与历史。
异步工具在宿主线程提交已完成的提案。
会话关闭时先结束预览和构建，再释放文档。

新增操作通过 `add(EditDescriptor, Handler)` 注册。
处理器接收编辑上下文与参数，并返回 JSON。
描述声明文档写入、事务能力和空闲要求。
文档写入要求预览与构建均处于空闲状态。

| 操作组 | 操作标识 | 主要参数 |
| --- | --- | --- |
| 节点创建 | `node.create`、`node.child` | `id` 或 `parent` |
| 资产放置 | `node.place`、`prefab.place` | `resource`，或 `asset` 与 `instance` |
| 节点属性 | `node.rename`、`node.visible` | `value` |
| 实例关系 | `node.prefab-source`、`node.instance` | `value` |
| 节点删除 | `node.remove`、`node.delete` | `index`，或当前选择 |
| 父级关系 | `node.reparent` | `id` 与 `parent` |
| 射线放置 | `asset.place` | `asset`、`origin` 与 `direction` |
| 内容目录 | `assets.catalog` | 可选 `type` |
| 组件恢复 | `component.remove`、`component.reset-field` | `type` 与可选 `field` |
| 任务与反馈 | `tasks.describe`、`feedback.describe/dismiss` | 空对象或错误 `id` |
| 路径服务 | `path.choose/history/remember` | 用途、路径与筛选 |
| 节点复制 | `node.duplicate` | 当前选择 |
| 变换 | `node.transform` | `translation`、`rotation`、`scale` |
| 选择 | `node.select`、`selection.click` | 身份或索引，以及 Ctrl、Shift 和可见排序 |
| 连续变换 | `viewport.gizmo-begin/update/commit/cancel` | 空间、枢轴与列主序世界矩阵 |
| 变换偏好 | `viewport.gizmo-options` | `space` 与 `pivot` |
| 组件 | `component.add`、`component.field` | `type`，或 `type`、`field`、`value` |
| 渲染 | `render.settings`、`render.preset` | `values` 或 `value` |
| 文档 | `document.save`、`document.reload` | 空对象 |
| 历史 | `history.undo`、`history.redo`、`history.end-edit` | 空对象 |
| 预览 | `preview.play`、`preview.pause`、`preview.resume` | 空对象 |
| 单步与停止 | `preview.step`、`preview.stop` | 空对象 |
| 预览切关 | `preview.level` | `value` |
| 工作区 | `workspace.reset`、`assets.reload`、`viewport.capture` | 空对象 |
| 视口状态 | `viewport.gizmo-mode`、`viewport.debug-overlay` | `value` 或 `enabled` |
| 项目 | `project.create`、`project.build` | 发现描述定义的路径与选项 |
| 导入 | `asset.import`、`asset.import-start` | `path` |
| 导入控制 | `asset.import-poll`、`asset.import-cancel` | 空对象 |
| 动画 | `animation.preview`、`animation.clear-preview` | 动画状态与采样参数，或空对象 |
| 项目脚本 | `script.write` | `path`、`value` |

参数模式检查类型、必填字段与未知字段。
数值必须有限，数组与文本具有大小上限。
组件属性继续使用运行时类型契约。
渲染、层级、引用与变换使用领域校验。

## 文档版本

`DocumentVersion` 包含三个字段。
`documentId` 标识当前文档实例。
`revision` 为单调递增修订号。
`contentHash` 为规范内容指纹。

比较同时检查实例、修订与内容指纹。
指纹采用 `fnv1a64:`，用于内容一致性检查。
授权范围由宿主按会话授权规则确定。
指纹的用途是发现内容变化。

保存、重开、撤销、重做与选择变化推进修订。
撤销后恢复相同内容仍会使旧请求过期。
空选择具有明确的无目标状态。
节点、组件、资源和全部渲染字段参与指纹。

## 请求与结果

`EditRequest` 包含请求与命令标识。
参数、基础版本与连续编辑合并键随请求提交。
调用 `execute()` 执行单项操作。
调用 `executeBatch()` 执行文档事务。

本机同步前端使用 `current()` 生成当前版本请求。
异步工具保留读取快照时的版本。
调用方按已授权的操作范围提交请求。
工具按注册描述发现扩展操作。

| 状态 | 含义 | 诊断码 |
| --- | --- | --- |
| Applied | 操作成功，结果包含新版本与差异 | 无错误码 |
| Rejected | 参数、启用条件或事务范围不符 | `RejectedOperation` |
| Stale | 基础版本与当前版本不符 | `StaleVersion` |
| Failed | 领域处理或候选校验失败 | `OperationFailed` |

结果中的 `diff` 使用 JSON Patch。
`value` 保存操作返回值。
`diagnostics` 保存诊断代码、消息或请求标识。
调用方根据状态决定展示、重新生成或修复。

## 事务与历史

批量操作上限为 128 项。
每项请求持有同一基础版本。
处理器依次修改候选上下文。
候选通过结构与引用校验后整体提交。

失败保持文档、选择、脏状态与历史。
成功批量形成一个撤销单元。
只读描述的处理器必须保持文档内容。
单项失败恢复文档状态与修订。

连续编辑共享合并键、选择与相邻修订。
同一次拖动形成一个历史单元。
松开控件、切换选择或执行会话操作结束合并。
撤销与重做恢复整次拖动结果。

文件、导入、构建与预览操作具有外部副作用。
这些操作以单项请求执行，并遵循领域恢复规则。
批量请求在执行前检查事务能力。
脚本写入仅接受项目挂载内的 Lua 文件。

脚本源码上限为 2 MiB。
构建期间脚本写入明确拒绝。
运行预览独立持有世界与脚本实例。
停止预览保持编辑文档状态。

## 选择与连续变换

`selection.click` 接收稳定身份和修饰键。
Shift 范围使用调用方提供的完整可见顺序。
当前对象为最近点击的范围端点。
选择服务维护锚点和大纲揭示请求。

`viewport.gizmo-begin` 保存起始文档与历史。
参数 `space` 使用 `world` 或 `local`。
参数 `pivot` 使用 `active` 或 `bounds`。
返回值包含枢轴矩阵和顶层选择数量。

`viewport.gizmo-update` 接收十六项列主序矩阵。
增量相对起始矩阵计算，经父级逆变换写入。
合法 TRS 分解遵守 `T * Rx * Ry * Rz * S`。
非有限、奇异及不可表示的剪切矩阵返回诊断。

`viewport.gizmo-commit` 结束单个撤销单元。
`viewport.gizmo-cancel` 恢复起始内容和完整历史。
取消同时恢复未保存标记与已有重做栈。
活动拖拽期间，其他制作与运行操作保持隔离。

ImGuizmo 负责绘制、命中和候选矩阵。
公共变换服务负责候选校验和文档写入。
源版本和许可见 `third_party/ImGuizmo/source.json`。
来源提交、适配说明与 MIT 许可随安装交付。

## 自动工具

帧回放的 `edit` 命令接收注册操作标识。
工具可执行注册扩展，无需增加命令分派。
示例使用当前本机版本提交同步请求。
异步提案直接使用 `EditRequest` 保留基础版本。

```json
[
  {
    "frame": 0,
    "command": "edit",
    "operation": "node.rename",
    "parameters": {"value": "检视对象"}
  }
]
```

## 验证入口

```powershell
ctest --test-dir build/ninja-msvc-debug `
  -R '^AzureEditor.(EditService|EditTransaction)$' --output-on-failure
```

契约用例验证各前端一致性和注册扩展。
事务用例验证失败恢复、合并与版本过期。
生产宿主与输入回放分别验证真实调用路径。
制作步骤见[编辑器教程](../tutorials/editor-first-game.md)。

内容引用、任务和放置见[内容制作服务](content-workflows.md)。

## 项目与工作区操作

项目、模板与布局消费共同操作服务。
参数和生命周期见[项目工作区](project-workspace.md)。
项目激活重建文档服务，并保留用户设置。

## 批量属性与引用操作

字段、单轴重置与吸管见[属性编辑](property-editing.md)。
批量操作使用共同事务和撤销入口。
引用请求携带捕获文档版本。
