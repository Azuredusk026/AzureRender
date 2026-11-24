# 材质输出精度

材质画像版本二通过 `r11g11b10-output` 特性选择输出精度。
省略该特性时，表面采用原始单精度颜色路径。
该特性适用于受光的不透明和遮罩材质。
透明、叠加和无光照组合在解释时提供错误。

## 数据声明

```json
{
  "schemaVersion": 2,
  "class": "generic",
  "features": ["r11g11b10-output"]
}
```

特性占 `materialFeatures` 的第二十六位。
现行材质表为二十三行、368 字节。
完整材质文档将声明保存在 `surface.materials` 中。
保存与重开沿用 `azure.profile` 解释入口。

## 数值规则

片元的最终 RGB 转换使用五位指数。
R、G 使用六位尾数，B 使用五位尾数。
转换向零舍入，有限溢出饱和到最大有限值。
目标非规格化值采用精确的二次幂量化间距。

负值及负零转换为正零。
NaN 转换为规范化的安静 NaN，正无穷保留。
Alpha 按位保持原值。
编辑选择颜色和诊断隔离颜色使用同一最终出口。

转换规则依据 [Direct3D 浮点格式与转换规范](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#3.2.2%20Floating%20Point%20Conversion)。
该操作定义片元颜色的数值网格。
附件格式、混合规则和后处理继续由所属渲染目标控制。

## 公共使用

`quantizeMaterialOutputColor` 为 CPU 工具提供相同数值语义。
`azureMaterialOutputPrecision` 为片元和计算程序提供转换。
普通 PBR 与投影头发使用相同特性声明。
功能验收与原生完整场景等价性分别核对。
