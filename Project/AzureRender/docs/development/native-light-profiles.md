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
