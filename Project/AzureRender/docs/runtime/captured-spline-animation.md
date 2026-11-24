# 截帧网格路径动画

内容转换入口为 `tools/build_captured_spline_clips.py`。
输入包含截帧 GLB、路径描述、世界矩阵和绑定表。
输出为每个发射器的 GLB 及来源收据。
运行时使用[刚体节点动画](rigid-node-animation.md)。

## 资源与变换

截帧资产包含一个单位变换节点及多个图元。
绑定表通过 `primitive` 与 `origin` 指定模板。
源世界矩阵须为有限、保向的刚体变换。
位置曲线使用路径世界坐标减模板的截帧原点。

模板保留截帧的朝向、尺寸、顶点和共享材质。
每个槽位按绑定顺序循环选择模板。
寿命使用缩放曲线，渐隐使用 `AZURE_node_opacity`。
节点的绑定缩放为一，运行时可采样零缩放。

## 输入与输出

绑定表的 `schemaVersion` 为一。
`emitters` 中每项提供来源 `identity` 及 `templates`。
路径输入使用 `NativeSplineRigidClip/1` 的编译结果。
源速度、寿命和透明度曲线由路径描述持有。

```powershell
python tools/build_captured_spline_clips.py `
  --input captured.glb --inventory logistics-production.json `
  --world logistics-world-transforms.json --bindings bindings.json `
  --output generated-flow --seed 20261022 --sample-rate 30
```

输出目录须为新目录。
收据保存输入、工具和输出的 SHA-256。
转换结束时复核来源文件保持一致。
失败收据保存可定位的异常信息。

## 验收范围

初始相位为创作指定的预热零时刻。
源激活集合、截帧全局相位和随机重采样单独验收。
重叠来源路径由绑定表显式选择来源身份。
输出收据保留这些来源条件的实际核验状态。

工具测试覆盖旋转路径及多个模板的组合。
非法矩阵、重复归属和不闭合曲线进入失败检查。
联合场景结果见[物流动态验收](../acceptance/nr/joint-conveyor-motion.md)。
