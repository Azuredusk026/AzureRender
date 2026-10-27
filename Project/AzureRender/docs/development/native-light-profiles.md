# 原生灯光绑定

`tools/bind_native_light_profiles.py` 生成关卡快照。
来源包括 ECS 灯光档案及明确选择的截帧常量。
调用者须明确选择来源场景。
转换属于内容工具层，运行时使用现有灯光契约。

## 身份与参数

目标灯光采用 `native-light-<实体编号>` 身份。
源记录通过 `entity_id` 匹配。
`fields.specular_intensity` 写入 `shape.specularScale`。
倍率采用有限、非负的单精度浮点数。
已有方向、颜色、强度及灯盒保持各自配置。

源场景、格式、字段和身份须完整有效。
重复身份、缺失来源及非法倍率产生明确错误。
其他项目灯光保留配置，并列入收据。
环境主光按所属来源单独绑定。

## 命令

```powershell
python tools/bind_native_light_profiles.py `
  --level input.azurelevel --profiles lights.json `
  --source-scene base01_lv001 `
  --output bound.azurelevel --receipt binding.json
```

输出关卡与收据使用两个新路径。
工具保存输入摘要、逐灯身份、原值及目标值。
写入失败时清理本次已创建的输出文件。
源关卡和灯光档案保留原始字节。

公共函数为 `bind_specular_profiles`。
它返回关卡副本和绑定收据，适用于批处理与夹具。
运行行为见[逐灯镜面响应](../runtime/light-specular-response.md)。
实际来源及画面见[绑定验收](../acceptance/nr/local-light-profile-binding.md)。

## 活动主光辐照

调用者确认事件后，可提供 CB3 的前六行字节。
`decode_directional_header` 读取源世界方向与辐照输入。
颜色取第二行 RGB，镜面倍率取第五行 X。
颜色和强度按最大颜色通道拆分，保留原始字节。

主光绑定须同时提供 `--main-header`、`--main-light` 和 `--main-enabled`。
启用值为零或一，来自调用者确认的活动状态。
目标须为唯一的方向光。
工具更新颜色、强度、镜面倍率及启用状态。

`bind_directional_radiometry` 保留目标的局部方向。
调用者须核对源世界方向与关卡节点变换。
运行快照按节点矩阵转换局部方向。
常量、方向及实际图像见[主光验收](../acceptance/nr/main-light-runtime-binding.md)。

## 档案颜色空间

`--profile-color-encoding` 显式选择档案 RGB 的解释方式。
`authored-srgb` 接收零至一的源颜色。
`linear` 接收有限非负的单精度线性颜色。
省略该参数时只绑定镜面倍率。

源记录须提供 RGB 或 RGBA，以及明确的温度标志。
普通颜色写入现有灯光 RGB，强度保持各自配置。
sRGB 分支按来源公式逐次舍入为单精度。
RGBA 的透明分量保留在来源收据中。

温度混合灯保留当前颜色配置。
其身份列入 `unresolvedColorLights`。
`coloredLights` 记录具有明确转换的灯具数量。
调用结果及收据共同报告该边界。

实际颜色、失败输入及截帧对照见[颜色验收](../acceptance/nr/local-light-color-space.md)。

## 活动截帧快照

`--captured-constants` 与 `--profiles` 互斥。
截帧常量采用已核验的 CB3 完整记录布局。
`--selection` 提供版本一的明确选择。
所有输入及选择文件的摘要进入来源收据。

选择包含以下字段：

| 字段 | 约束与职责 |
| --- | --- |
| `schemaVersion` | 固定为整数一 |
| `sourceScene`、`event` | 明确场景与正整数事件 |
| `namespace` | 最长 64 字符的灯光身份前缀 |
| `indices` | 唯一的零至 255 索引 |
| `origin` | 三个有限世界坐标 |
| `tileWords`、`depthWords` | 各八个无符号 32 位遮罩字 |
| `surfaceReceivers` | 明确选择普通表面接收 |
| `replaceLightIds` | 当前关卡中唯一且存在的替换身份 |

工具先校验活动遮罩，再解码所选记录。
坐标先减去来源原点，再执行来源约定的 X 反射。
新灯光使用世界根节点，方向及灯盒采用同一世界约定。
未选择的灯光及原有节点保持各自配置。

支持默认角色响应和普通模式十六。
普通模式的角色接收位为假。
Cookie、有限长度和其他专用模式要求独立消费证明。
输出灯光总量须符合运行时 128 盏的容量。

```powershell
python tools/bind_native_light_profiles.py `
  --level input.azurelevel --captured-constants lights.bin `
  --selection selection.json --source-scene base01_lv001 `
  --output active.azurelevel --receipt binding.json
```

截帧输出收据采用版本二。
它保存常量、选择、源事件、活动遮罩及逐记录字节。
ECS 档案输出继续采用版本一。
实际选择及画面见[快照验收](../acceptance/nr/captured-active-lights.md)。
