# 材质制作与槽位覆盖

> 文档类型：使用说明与接口参考
> 状态：生效
> 更新日期：2026-10-12

## 创建与编辑

在内容浏览器打开 `Import / Create`。
选择 `Create material`，填写文件名。
文件使用 `.azurematerial` 扩展名。
选择 PBR 或 Stylized 类型后进入参数表单。

双击已有材质也能打开参数表单。
表单展示材质球、参数和贴图槽。
参数范围来自材质类型注册描述。
合法编辑立即发布到材质资产。

材质文档历史归当前项目会话。
`Undo material` 与 `Redo material` 发布对应状态。
场景历史用于对象、组件和材质槽位覆盖。
外部文档变化使当前资产的会话历史失效。

## 使用材质画布

打开材质后，展开默认折叠的 `Material graph`。画布显示输入节点、材质输出和连接。拖动节点调整布局，下方的参数输入列表选择兼容数值节点。

选择 `Default` 让对应参数使用默认值。首次保存图时可点击 `Save graph`，后续布局和连接修改经公共编辑操作发布。参数表单与画布共用材质文档，也共用撤销和重做历史。

当前图支持类型化数值、纹理节点及一个注册材质输出。画布上的参数列表提供数值连接选择，纹理仍可通过表单编辑。任意着色程序图属于扩展范围，图的结果沿用已有材质渲染路径。

## 为对象赋值

在大纲选择模型对象，展开属性中的 `Materials`。
每个源槽位提供材质选择与重置入口。
选择材质资产，或将其拖到槽位控件。
多选对象时，赋值形成一个场景事务。

所有目标必须具备所选源槽位。
批量操作先校验全部对象，再提交场景。
保存场景后，覆盖随关卡或场景文档保存。
重置槽位恢复模型携带的源材质。

## 源槽位身份

模型材质的 `extras.azureSlotId` 提供显式身份。
唯一材质名称映射为 `name:<名称>`。
匿名或重复名称映射为 `index:<索引>`。
索引槽位适用于保持材质表顺序的源文件。

显式身份与唯一名称支持材质表重排。
同一模型的槽位身份必须唯一。
缺失源槽位产生关联对象与槽位的诊断。
属性面板提供清除失配覆盖的入口。

模型重导入返回 `slotMapping` 和 `slotDiagnostics`。
映射按源槽位身份保留关联，包含稳定性标记。
已赋值槽位失配时，操作在发布前拒绝。
源文件在任务期间变化时，任务报告失败。

## 材质文档

文档版本为 `schemaVersion: 1`。
`typeId` 选择已注册的运行解释。
`parameters` 包含类型声明的全部参数。
`textures` 使用资产 UUID 与用途声明。

```json
{
  "schemaVersion": 1,
  "typeId": "azure.pbr",
  "parameters": {
    "baseColor": [0.8, 0.8, 0.8, 1.0],
    "roughness": 0.75,
    "metallic": 0.0,
    "emissiveStrength": 0.0,
    "doubleSided": false
  },
  "textures": {}
}
```

| 参数 | 范围 | 默认值 |
| --- | --- | --- |
| baseColor | 四个线性颜色通道，各为 0 至 1 | 0.8、0.8、0.8、1 |
| roughness | 0 至 1 | 0.75 |
| metallic | 0 至 1 | 0 |
| emissiveStrength | 0 至 32 | 0 |
| doubleSided | 布尔值 | false |
| toonAmount | 风格材质，0 至 2 | 1 |
| specularAmount | 风格材质，0 至 2 | 1 |
| rimAmount | 风格材质，0 至 2 | 1 |

| 贴图槽 | usage | 运行用途 |
| --- | --- | --- |
| albedo | albedo | 基础颜色 |
| normal | normal | 切线空间法线 |
| metallicRoughness | roughness | G 通道粗糙度，B 通道金属度 |
| emissive | emissive | 自发光颜色 |

贴图字段形如 `{"asset":"UUID","usage":"albedo"}`。
UUID 必须引用项目中的可解码图片。
法线与金属粗糙度按线性数据采样。
过滤、包裹和 mip 规则消费图片导入设置。

图片文件上限为 64 MiB，单轴上限为 8192。
版本、非法参数和失效引用在写入前校验。
资产文件经内容事务的原子发布与日志恢复。
依赖关系包含关联贴图和场景槽位引用。

## 编辑操作

| 操作 | 参数 | 作用 |
| --- | --- | --- |
| material.types | 无 | 查询材质类型描述 |
| material.create | type、destination | 创建独立材质资产 |
| material.describe | asset | 查询文档、参数描述和历史状态 |
| material.graph | asset、graph | 校验并发布材质连接图 |
| material.field | asset、field、value | 发布合法参数或贴图编辑 |
| material.undo | asset | 撤销该资产的编辑 |
| material.redo | asset | 重做该资产的编辑 |
| material.save | asset | 校验当前已发布文档 |
| material.slots | node | 查询源槽位、覆盖和失配诊断 |
| material.assign | nodes、slotId、material | 批量赋值 |
| material.reset-slot | nodes、slotId | 批量重置 |
| preview.types | 无 | 查询已注册的预览资产类型 |

贴图编辑字段为 `textures.<槽名>`。
设置 `value: null` 清除该贴图引用。
创建目标必须位于已有项目目录。
重名创建与引擎只读资源写入均被拒绝。

## 预览与运行

模型、材质和图片各有注册预览提供者。
材质球和图片平面使用引擎基础几何生成器。
材质与贴图指纹共同决定预览缓存身份。
预览缓存最多占用两个渲染视图槽位。

场景实例的材质覆盖参与渲染资源布局身份。
相同模型与相同覆盖共享资源。
不同覆盖分别构建模型材质资源。
编辑宿主与 Player 消费同一材质解释。

场景格式版本为 4，读取兼容版本 1 至 3。
项目移动通过材质 UUID 和相对挂载恢复引用。
操作教学见[编辑器制作入门](../tutorials/editor-first-game.md)。
