# 材质覆盖图元

`append_material_overlays.py` 追加分层无光照图元。
覆盖图元可表达表面发光和检视标注。
源材质槽、位置、骨骼、形变及动画保持完整。
表面深度规则见[无光照图层](unlit-texture-layers.md)。

## 配置

版本一配置包含以下字段。
纹理索引以配置表为域，工具负责追加时的重映射。
完整材质语义由资产加载器校验。

| 字段 | 内容 |
| --- | --- |
| `schemaVersion` | 整数一 |
| `images` | 嵌入的 PNG 数据 URI |
| `samplers` | glTF 采样器对象 |
| `textures` | 配置内的图像及可选采样器索引 |
| `overlays` | 一至六十四份覆盖绑定 |
| `overlays[].sourceSlot` | 源模型的稳定材质槽 |
| `overlays[].uvSet` | 零至七的 UV 集编号 |
| `overlays[].material` | 完整的分层无光照材质 |

覆盖材质具有独立名称，采用 `BLEND` 模式。
画像版本为二，类别为 `generic` 或 `emissive`。
特性为 `unlit-surface` 和 `unlit-texture-layers`。
表面字段遵守所属运行契约。

工具选择原模型中属于来源槽的全部三角图元。
追加图元共享原始访问器，并将指定 UV 绑定到 UV0。
同一来源槽可以具有不同名称的多层覆盖。
缺失槽、缺失 UV 和非法索引产生明确错误。

## 执行

输入与输出采用相同扩展名，并置于相同目录。
输出模型及其来源文件须为新路径。
输入资源的相对定位随模型目录保持一致。

```powershell
python tools/append_material_overlays.py `
  --input path/to/source.glb `
  --config path/to/overlays.json `
  --output path/to/source-overlays.glb
```

输出旁保存 `.overlays.json` 来源记录。
记录包含输入、配置、工具及结果的 SHA-256。
二进制块、源节点、源材质及动画数据保持完整。
写入前核对输入摘要，并保护已有来源记录。

验证范围见[覆盖图元验收](../acceptance/nr/material-overlays.md)。
