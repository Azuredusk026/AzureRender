# 原生灯光倍率绑定

`tools/bind_native_light_profiles.py` 生成关卡快照。
来源为 `AzureEndfieldEcsLights/1` 灯光档案。
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
