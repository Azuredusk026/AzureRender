# 原生 UV 转换

原生场景工具读取探针的 OBJ 和 UV 边车文件。
`native_mesh_uvs.py` 恢复导入网格的第二组坐标。
结果采用明确的 `UV0` 和 `UV1` 名称。
每份收据保存源文件摘要及匹配误差。

## 三角形与接缝

位置和第一组坐标相同的角点可以属于不同 UV1 接缝。
匹配按完整三角形及角点建立对应。
比较允许 `3e-6` 米位置误差及 `1e-6` UV 误差。
缺失三角形或仍有歧义的接缝产生明确错误。

探针提供两份逐顶点的 `float2` UV 流。
流长度、顶点数及 OBJ 索引共同校验。
坐标转换沿用探针的 X 反射和 Blender 轴约定。
导入器在网格角点保存独立 UV1。

## 材质采样

显式配置适用于已核验的 Lit 三通道分支。
基础色读取 `_BaseUVSet`，法线读取 `_BasePbrMapUVSet`。
遮罩读取 `_MaskUVSet`。
有限的纹理缩放及偏移在各自坐标域执行。

选择器接受零或一，并绑定对应命名 UV。
材质记录 `NativeLitUvSelection/1` 及纹理选择。
其他着色分支按所属适配任务扩展。
颜色上传遵循[原生表面转换](../runtime/native-surface-conversion.md)。

Lit 法线的 XY 阈值及 Z 下限在解码节点中执行。
颜色、粗糙度和金属度保持各自采样与混合规则。
`test_native_surface_normals.py` 使用 CPU 烘焙检查六组输入。
强度的非法值同时检查错误及材质所有权。

## 网格烘焙

公共入口为 `bake_material(source, resolution, mesh=mesh, uv_layer='UV1')`。
目标使用实际网格及明确的 UV 展开。
独立输入 UV 保留各自采样域。
调用方持有原始数据，烘焙使用临时副本。

输出包含基础色、法线、金属粗糙度和自发光纹理。
输出纹理明确绑定目标 UV。
网格图集按八像素扩展边缘颜色及数据。
导出网格将目标设为活动 UV，并保存对应作者切线。
运行导入按[表面契约](../runtime/surface-uv-domain.md)消费。

单位平面入口处理周期 UV0 材质。
命名 UV 输入要求明确的实际网格。
视角、对象、顶点和时间输入由专用适配器处理。
目标分辨率范围为 `8` 至 `4096`。

## 场景入口

`tools/endfield_native/build_scene_preview.py` 支持以下组合。
命令还需提供来源、探针、清单和新输出目录。

```text
--bake-materials 2048 --mesh-bake-uv UV1 --exchange
```

来源块及 LOD 按各自明确规则选择。
烘焙按网格、子网格及材质组合复用。
审计保存 UV 流、目标 UV 及材质烘焙记录。
交换资源和合并 glTF 均导出对应作者切线。
