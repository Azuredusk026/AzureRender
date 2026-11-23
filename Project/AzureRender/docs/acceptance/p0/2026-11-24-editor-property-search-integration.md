# 属性编辑与搜索集成

组合基线为主线 `453159b`，编辑器交付为 `9baa14f`。
主线组合覆盖数值拖动取消、失焦恢复及三种搜索入口。
适用项目为探索与资产检视。
程序摘要和使用的源码清单见验收归档。

## 自动验收

Release 构建包含实际编辑器及两个受影响测试目标。
编辑事务和属性编辑的 CPU 检查通过。
两项目各通过属性工作流及引用搜索回放。

属性回放验证拖动失焦恢复、撤销重做及保存重开。
搜索回放验证字段过滤、输入焦点和引用选择。
主线 Release 批次 `2442` 执行综合回放。
双项目视口、六项文档恢复和渲染观察模式通过。
同一主线程序在批次 `2434` 通过双项目引用回放。
[综合回放收据](evidence/2026-11-24-composed-editor-workline-replays/results.json)记录主线结果。
[工作线回放](../../plans/editor-interaction-regression-2026-11-24/README.md)记录独立构建结果。
本地计划文件继续由开发总计划的维护范围管理。

主线 `0d56968` 的 Debug 构建通过四项关联 CPU 检查。
检查覆盖纹理层级、纹理缓存、编辑事务和属性编辑。
[双配置核对收据](evidence/2026-11-24-composed-debug-regression/results.json)记录受检程序摘要。

## 待人工复验

状态为待人工复验，执行安排由用户主动指定。
实际程序对应版本及摘要见本页结果清单。
操作步骤、通过条件和自动证据见[人工复验单](../../plans/editor-interaction-physical-review.md)。
复验范围包括实体键鼠、中文输入法及多屏缩放。
制作效率、反馈和错误恢复依照同一复验单记录。

[结果清单](evidence/2026-11-24-editor-property-search-integration/results.json)与
[验证归档](evidence/2026-11-24-editor-property-search-integration/verification.zip)
保存构建、产物、回放和来源核对记录。
