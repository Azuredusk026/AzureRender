# 栅格材质镜面输入

版本二材质画像可声明 `specularInput`。
该对象控制直接镜面与环境镜面的介电反射率。
支持通用、织物及金属类别。
路径追踪使用独立的镜面模型。

```json
{
  "schemaVersion": 2,
  "class": "fabric",
  "features": [],
  "specularInput": {
    "schemaVersion": 1,
    "source": "metallic-roughness-alpha",
    "scale": 0.5
  }
}
```

三项字段必须显式提供，并拒绝未知字段。
`source` 可取 `emissive-alpha` 或 `metallic-roughness-alpha`。
`scale` 为零至一的有限数值。
金属粗糙度来源须绑定实际纹理。

对象缺省时采用自发光 Alpha，倍率为一。
所选 Alpha 乘以倍率后，限制在零至一区间。
介电反射率按现有规则计算为该值的 `0.08` 倍。
金属度继续控制介电反射率与基色的混合。

该输入与自发光 RGB、法线及漫反射分别消费。
皮肤、头发、眼睛及叠层使用各自的表面契约。
不兼容的表面所有权在加载时触发错误。
不可变材质表第六行 Z、W 保存来源与倍率。

## 原生内容转换

`character_surfaces.py` 提供显式材质选择。
重复使用 `--specular-material` 指定源材质名称。
`--specular-only` 仅转换所选材质的参数贴图。
其他材质、动画、几何与自发光数据保持原值。

源参数图转换为 `(P.B, 255-P.A, P.R, P.G)`。
四通道分别保存 AO、粗糙度、金属度及镜面参数。
内容倍率为 `0.5`，对应源介电系数 `0.04`。
生成报告保存来源、通道、倍率及输入输出摘要。

验证范围见[服饰镜面输入验收](../acceptance/nr/cloth-specular-input.md)。
