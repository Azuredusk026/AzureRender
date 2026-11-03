# 不透明表面覆盖

分层无光照表面通过 `submission` 声明提交阶段。
缺省值为 `transparent`。
`opaque-overlay` 表达附着表面的加法辐亮度。
表面发光和检视标记可采用该阶段。

```json
{
  "submission": "opaque-overlay",
  "composite": "additive",
  "depthCompare": "less-or-equal",
  "softIntersection": {
    "enabled": false,
    "distance": 1,
    "bias": 0
  },
  "fogMode": "none"
}
```

这些字段属于版本二分层表面的完整配置。
基础输入见[无光照图层](unlit-texture-layers.md)。
非法阶段、混合、深度及雾组合在加载时拒绝。
程序生成材质同样接受运行时合法性检查。

底层几何持有深度、法线和显示变换。
阴影投射由底层几何持有。
覆盖仅向 HDR 颜色附件累加，并关闭深度写入。
颜色附件的透明度通道保持底层覆盖。

覆盖几何通过深度比较接受底层的遮挡。
资产应提供位置一致且具有对应动画的覆盖图元。

主视图在全部不透明表面之后提交覆盖。
后续透明表面可以覆盖这份颜色贡献。
反射读取的不透明快照同样累加覆盖。
覆盖图元按资产索引顺序绘制。

角色表面的独立发光层可以指定 `colorBlendResponse` 为 `attenuate-emission`。
启用角色风格化照明时，每盏接收角色的混色灯按位置、朝向和强度计算权重。
权重为 `w` 时，发光层的辐亮度乘以 `1 − w`。
灯光目标色由底层表面合成一次。多个灯的衰减依次相乘。
该选项要求 `opaque-overlay` 提交阶段，缺省响应为 `none`。

场景雾由主视图按底层深度统一合成。
`fogMode` 为 `none`，由全局不透明合成持有雾。
透明效果的独立雾配置见[场景雾契约](unlit-texture-layers.md)。

顶点颜色、UV、节点透明度及骨骼采用既有输入。
完整材质文档保留提交阶段。
覆盖图元转换见[材质覆盖图元](material-overlays.md)。
实施范围见[阶段计划](../plans/2026-11-03-opaque-surface-overlays.md)。
