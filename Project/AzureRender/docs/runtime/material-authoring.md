# 材质制作与槽位覆盖

普通 PBR 输入按[表面纹理坐标](surface-uv-domain.md)规范化。
跨 UV 的原生材质可通过实际网格烘焙生成统一表面。

> 文档类型：使用说明与接口参考
> 状态：生效
> 更新日期：2026-10-22

## 创建与编辑

在内容浏览器打开 `Import / Create`。
选择 `Create material`，填写文件名。
文件使用 `.azurematerial` 扩展名。
选择 PBR 或 Stylized 类型后进入参数表单。
选择带“参数覆盖”的条目可创建继承材质。
勾选参数后，该字段参与覆盖。
取消勾选使字段继承所绑定的源材质。

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
唯一的导入名称映射为 `name:<名称>`。
缺少名称时，导入器使用 `UnnamedMaterial`。
隐式材质使用 `FallbackMaterial`。

重复名称映射为 `index:<索引>`。
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
    "transmission": 0.0,
    "ior": 1.5,
    "thickness": 0.0,
    "attenuationDistance": 0.0,
    "attenuationColor": [1.0, 1.0, 1.0, 1.0],
    "doubleSided": false
  },
  "textures": {}
}
```

正值 `emissiveStrength` 启用运行材质的自发光功能位。
自发光颜色来自 `emissive` 纹理，缺省纹理为黑色。
PBR 与 Stylized 使用相同的自发光解释规则。

版本 1 的槽位赋值采用完整替换，并保留槽位身份。
替换丢弃特殊表面语义时，公共诊断记录槽位和字段。
诊断覆盖 SDF、色带、头发、眼睛及相关渲染状态。

版本 2 使用 `mode: parameters` 声明参数覆盖。
`parameters` 只保存显式覆盖的字段。
未声明的参数、特殊表面、贴图和渲染状态继承源材质。
该模式需要绑定源槽位，材质球使用生成球体的源材质。

```json
{
  "schemaVersion": 2,
  "mode": "parameters",
  "typeId": "azure.pbr",
  "parameters": {"emissiveStrength": 2.0},
  "textures": {}
}
```

`typeId` 声明可编辑字段，源材质类别保持继承。
基础颜色与金属粗糙度参数乘入已解码的源贴图。
显式贴图引用替换对应输入，再应用声明的乘数。
每次加载均以原始模型为基底，乘数不会累积。

移除覆盖贴图引用后，该槽恢复继承。
参数覆盖中的空值编辑表示移除字段。
保存的字段值必须符合类型，文档内空值会被拒绝。
材质图未连接的覆盖端口同样继承源值。

透射和厚度覆盖要求兼容的通用表面。
专用 SDF、头发、眼睛、镜面或环境表面拒绝此组合。

## 完整表面画像

版本 3 使用 `typeId: azure.profile`。
`surface` 沿用 glTF 2.0 的材质结构。
其中 `materials` 必须且只能包含一个材质。
图片、采样器及特殊表面由公共 glTF 解释器消费。

```json
{
  "schemaVersion": 3,
  "typeId": "azure.profile",
  "parameters": {},
  "textures": {},
  "surface": {
    "asset": {"version": "2.0"},
    "materials": [{
      "alphaMode": "MASK",
      "doubleSided": true,
      "extras": {"azureRenderMaterial": {
        "schemaVersion": 2,
        "class": "fabric",
        "features": ["silhouette-outline"]
      }}
    }]
  }
}
```

`textures` 是图片表，键为从零开始的连续数字字符串。
每项具有图片 `asset` UUID 和 `usage`。
`surface.textures` 的 `source` 索引该图片表。
材质中的纹理索引引用 `surface.textures`。

画像拥有颜色空间、采样器和纹理通道解释。
图片表的 `usage` 声明资源类别。
一次解释至多接收 64 张图片，原始字节总量至多 64 MiB。
几何、外部 URI 和缓冲声明由材质文档拒绝。

SDF 头节点按所绑定模型的节点名称解析。
`surface.nodes` 提供独立解释时的命名节点。
实际模型中的缺失节点和重名节点产生加载错误。
角色、场景与原生常量身份由项目内容维护。

创建菜单的 `Surface profile` 打开完整画像表单。
表单提供画像与图片表，点击“应用画像”发布事务。
材质图保存相同画像，撤销和重做涵盖完整文档。
共享编辑操作的字段为 `surface` 与 `images`。

结构说明见 `schemas/material_document.schema.json`。
画像版本见 `schemas/azure_render_material.schema.json`。
节点、纹理索引及合法组合由生产解释器校验。
工具通过同一解释器输出结果。

```text
AzureMaterialValidator model.glb
AzureMaterialValidator --slots model.glb
AzureMaterialValidator surface.azurematerial uuid-paths.json
python tools/validate_material_profiles.py model.glb
```

`--slots` 读取材质名称、显式身份及隐式材质。
槽位检查与运行绑定共用身份分配规则。
几何、纹理和动画由完整模型准备路径校验。

UUID 路径文件是 UUID 到本机图片路径的 JSON 对象。
`--describe` 返回已注册的材质字段。
转换函数 `complete_material_document` 保留完整画像。
实际角色对照见完整材质文档验收。

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
| material.create | type、destination、可选 mode | 创建材质；mode 为 replace 或 parameters |
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

场景格式版本为十二，读取兼容版本一至十一。
项目移动通过材质 UUID 和相对挂载恢复引用。
操作教学见[编辑器制作入门](../tutorials/editor-first-game.md)。

## 投影头发输入

图片的 `.azmeta` 在 `settings.texture` 声明采样设置。
完整画像按 UUID 解析图片及其元数据。
glTF 与完整画像共用最终输入槽的解释规则。
未声明的字段继承该槽的语义默认值。

双法线、参数图和条纹图保持线性颜色空间。
这些输入允许配置过滤、包裹、mip 和各向异性。
`mipFilter` 与缩放过滤独立，`mipBias` 声明层级偏移。
编辑器、重新导入和完整材质共用图片设置契约。
漫反射色带、投影高光表和影色表使用固定采样。
不兼容的查找表设置返回包含材质及槽号的诊断。

嵌入式解释器接收按图片索引排列的导入设置。
空项使用默认值，非空数组须匹配图片数量。
调用者拥有解析路径，解释器消费已提供的字节。
完整材质文档的资源定位继续使用 UUID 契约。

完整画像支持 `hair.projectedRamp.virtualLight`。
该对象声明版本、主灯混合量及对象增益。
独立条纹由 `hair.projectedRamp.line` 声明。
字段和范围见[头发条纹验收](../acceptance/nr/projected-hair-line.md)。

条纹纹理与其他完整画像纹理共同进入图片 UUID 表。
材质保存时校验共享槽的合法组合。
材质球、模型覆盖和 Player 共用同一解释器。

双面材质与镜像实例的绕序规则见[表面朝向](surface-facing.md)。
