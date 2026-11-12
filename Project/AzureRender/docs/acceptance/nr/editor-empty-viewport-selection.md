# 空白视口选择

日期为 2026-11-12。

项目视口使用当前场景的可选对象执行拾取。
点击空白处时，普通点击清空选择，Ctrl 点击保留现有选择。
引用选择模式仅在命中可选对象时提交引用。

## 验收结果

Debug 与 Release 均完成全目标构建。
探索项目和场景检视项目通过普通空白点击、Ctrl 空白点击及场景文件检查。
两种配置均通过命中实际模型的对照，以及选择服务、引用选择和可玩编辑用例。

[结果清单](evidence/2026-11-12/editor-empty-viewport-selection/results.json)和
[验证归档](evidence/2026-11-12/editor-empty-viewport-selection/validation.zip)保存实际编辑输入、视口状态、构建和 CTest 记录。
