# 对象属性与引用编辑

属性面板显示当前选择的共同组件。
空选择显示对象选择提示。
多选字段显示混合状态，并声明编辑权限。
对象名称与组件添加使用单对象选择。

## 修改数值

1. 在大纲选择一个或多个对象。
2. 展开属性面板中的共同组件。
3. 拖动数值，或双击数值输入准确值。
4. 单击 X、Y、Z 按钮重置对应坐标。
5. 用 Ctrl+Z 撤销，用 Ctrl+Y 重做。

位置单位为米，旋转单位为度。
单位、精度和批量策略来自反射字段描述。
三轴重置使用组件注册表的默认值。
单轴编辑保留每个对象的其他坐标。

连续拖动合并为一次撤销。
结束控件编辑后关闭合并范围。
整批修改经共同事务发布。
非法对象、数值或引用会拒绝整批修改。

## 设置引用

引用下拉提供名称或路径搜索。
节点可从大纲拖入，资产可从内容浏览器拖入。
候选类型遵守字段声明与项目内容校验。

单击 Pick 后选择大纲、视口或内容中的候选。
吸管捕获原有选择与文档版本。
候选写入保持属性所属对象的选择。
节点引用的 Reveal 在大纲选中目标。
资产引用的 Reveal 在内容浏览器揭示目标。

Esc、窗口失焦或文档切换取消吸管。
选择修改使捕获版本过期，交付时拒绝写入。
Clear 清空允许为空的引用。
相机目标使用有效节点与所属对象默认值。

## 公共操作

| 操作 | 参数 | 行为 |
| --- | --- | --- |
| component.batch-field | nodes、type、field、value | 原子修改共同字段 |
| component.batch-field | nodes、type、field、axis、value | 修改单轴 |
| component.batch-field | nodes、type、field、reset=true | 使用注册默认值 |
| reference.begin | nodes、type、field | 捕获引用会话 |
| reference.deliver | kind、id | 校验候选并交付 |
| reference.cancel | 无 | 取消引用会话 |
| reference.reveal | kind、id | 揭示引用目标 |

`axis` 的取值为 0、1、2。
`reset` 可与 `axis` 组合。
`kind` 使用 `node` 或 `asset`。
`nodes` 使用稳定对象标识，最多 4096 项。

扩展组件通过公共注册入口声明字段。
`AZURE_FIELD_EDITOR` 接收单位、精度与批量策略。
精度范围为 0 至 9。
脚本和自动工具消费相同字段描述与写入校验。

操作版本与事务见[编辑操作](edit-operations.md)。
阶段范围见[产品化计划](../plans/2026-10-09-editor-productization.md)。
