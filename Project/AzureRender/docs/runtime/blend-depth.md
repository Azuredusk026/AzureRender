# 透明材质深度

版本二材质画像通过 `blendDepth` 声明深度配置。
该对象用于受光透明表面，版本号为一。
薄片、透明标牌和透明衣料可分别配置。

```json
{
  "schemaVersion": 2,
  "class": "fabric",
  "features": [],
  "blendDepth": {
    "schemaVersion": 1,
    "write": true,
    "compare": "less-or-equal"
  }
}
```

模型材质的 `alphaMode` 为 `BLEND`。
`write` 为布尔值，缺省为关闭。
`compare` 为 `less` 或 `less-or-equal`。
缺省比较方式为 `less`。

深度写入会影响随后提交片元的可见性。
包含同深度比较允许共面片元参与混合。
配置保持现行透明排序及透明阶段归属。
遮挡结果由片元深度、提交顺序及混合共同决定。

无光照表面采用[所属契约](unlit-texture-layers.md)。
乘法合成、场景染色及覆盖材质各自持有深度规则。
非法字段、类型、版本或归属在加载时拒绝。
程序生成的材质同样接受运行时合法性检查。

完整材质文档保留该对象及来源信息。
材质替换检查显式深度配置的保留情况。
GPU 材质布局及片元程序保持现行版本。
渲染器按需持有管线变体，并负责重建及释放。

执行范围见[实施计划](../plans/2026-11-03-blend-depth.md)。
