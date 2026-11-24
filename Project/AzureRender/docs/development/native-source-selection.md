# 原生场景来源选择

原生内容工具按明确的来源块路径选择输入。
每次选择保存路径、节点变化和输入摘要。
实际激活范围由目标截帧及内容来源共同确定。
相同矩阵的实体还需核对网格与材质。

## 清单与导入

`tools/endfield_native/scene.py` 接收重复的 `--source-chunk`。
参数采用 VFS 中的完整相对路径。
选择在读取块内容和解析实体之前执行。
清单的 `selection.source_chunks` 保存选择记录。

Blender 入口为 `tools/endfield_native/build_scene_preview.py`。
相同参数选择清单中的来源块。
实体、绘制、材质和子网格保留原始身份。
输出审计中的 `source_selection` 保存选择范围。

省略参数时，两个入口处理全部可用来源。
显式列表必须非空，且路径唯一并存在于输入中。
路径采用规范的相对形式和正斜线。
重复路径、越界路径和未知来源产生错误。

## 关卡快照

现有关卡使用以下入口生成独立快照。
所有命令在 `Project/AzureRender` 目录执行。

```powershell
python tools/select_native_scene_sources.py `
  --inventory <原生清单.json> --level <输入.azurelevel> `
  --source-chunk <完整来源块路径> `
  --output <新关卡.azurelevel> --receipt <新收据.json>
```

工具识别导入器的 `native-<实体>-draw-<序号>` 名称。
只有具有资源绑定的对应绘制节点参与选择。
选定来源保持节点现有可见状态。
排除来源通过公共 `azure.renderable` 可见字段隐藏。

未知实体保留现有状态，并列入 `unmappedEntities`。
输入关卡、几何、材质和资源绑定保持原值。
输出及收据使用两个不同的新文件。
收据保存输入摘要、变化节点及输出 SHA-256。

选择属于内容导入与快照生成。
引擎继续通过公共关卡和组件契约装配场景。
动态状态切换按所属场景机制单独验收。

## 验证

```powershell
python -m unittest discover -s tools -p 'test_endfield_native_scene*.py'
```

测试覆盖两个来源布局、错误输入及重复执行。
CLI 检查摘要、输出保护和失败时的输出状态。
Blender 与真实 Player 分别核对导入和最终画面。
具体截帧范围见[来源选择验收](../acceptance/nr/scene-source-selection.md)。
