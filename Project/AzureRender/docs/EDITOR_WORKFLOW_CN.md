# 编辑器生产工作流

> 适用版本：`.azscene v2` / R5

## 场景与实例

Scene Outliner 支持多节点父子树。每个节点保存独立的可见性、translation、rotation、scale、`prefabSource` 和 `instanceOf`。`prefabSource` 表示可复用模板的来源，`instanceOf` 是实例目标的稳定 ID；当前版本保存引用与覆盖 transform，不包含独立 prefab 文件展开器。

`.azscene v2` 写入上述字段，并继续读取 v1 文件。保存采用同目录临时文件加原子替换，失败不会截断已有场景。

## Undo 与 Redo

- `Ctrl+Z` / Edit > Undo：恢复上一个完整 SceneDocument 快照。
- `Ctrl+Y` / Edit > Redo：恢复被撤销的快照。
- 节点添加/删除、名称、可见性、transform、prefab/instance 和 Inspector 渲染设置均进入历史。
- 新编辑会清空 redo 栈；历史上限为 100 个快照；重新加载场景会清空历史。

## 资产状态与热重载

Asset Browser 显示资源 ID、解析路径、存在状态和依赖节点数量。`Reload Assets` 比较 `last_write_time`，随后在安全帧边界等待 GPU idle，对当前 scene renderer 执行 `onUnload` 和 `onLoad`。它是显式主线程操作，不启动后台文件监听器。

缺失资源会显示 `Missing`；热重载不会修改场景文件，也不会自动替换资源路径。

## Capture

Capture 面板接受语义标签，只保留字母、数字、连字符和下划线。`Capture Viewport` 在下一帧输出 `captures/<label>.png`，相同标签会更新同一工作图。正式视觉基准仍需按资产与视觉 QA 文档人工检查、重命名、记录 manifest 和 SHA-256。

## 验收范围

EditorSession 自动化覆盖 Undo/Redo、节点树、资源变化检测、renderer reload 请求、Capture 请求、失败保存和重新加载；SceneModel 自动化覆盖 v1 迁移与 v2 transform/prefab/instance round-trip。GPU 资源重载和 PNG 输出由 Debug Validation 编辑器 smoke 补充验证。
