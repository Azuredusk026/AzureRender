# 风格化漫反射照明

共享颜色响应支持调用者提供显式混合权重。
材质适配器拥有可见性和遮罩来源。
颜色核统一消费亮部、影部、色带及环境选择量。
原生片元的数值核对见[头发照明验证](../acceptance/nr/captured-hair-illumination.md)。

场景通过 `renderSettings.nprLighting` 声明照明组合。
消费者为显式声明 `nprDiffuse` 的材质。
材质颜色与入射照明分别计算。
皮肤、脸部及服饰共用该机制。

## 配置

渲染设置版本为 17，照明子对象版本为 1。
关卡 JSON 与文本场景共用编码和校验。
配置默认关闭，启用后采用明确的场景主灯。
主灯方向及身份沿用公共方向灯选择规则。

```json
{
  "nprLighting": {
    "schemaVersion": 1,
    "enabled": true,
    "ambientColor": [1, 1, 1],
    "ambientDirection": [0, 1, 0],
    "directionBias": 0,
    "directionGain": 0,
    "directionFloor": 1,
    "outdoorGain": 1,
    "shadowAmbientGain": 1,
    "indoorMix": 0,
    "litAmbientMix": 0,
    "mainColorMix": 0,
    "shadowOverride": 0,
    "useSceneIrradiance": false
  }
}
```

| 字段 | 合法输入与作用 |
| --- | --- |
| enabled、useSceneIrradiance | 布尔值，控制启用及环境来源 |
| ambientColor | 三个 0 至 64 的线性颜色分量 |
| ambientDirection | 三分量单位世界方向 |
| directionBias | -1 至 1，方向点积的偏移 |
| directionGain、directionFloor | 0 至 16，方向响应的倍率及底值 |
| outdoorGain、shadowAmbientGain | 0 至 16，两类照明贡献的倍率 |
| ambientWeight | 0 至 16，环境响应权重，默认 1 |
| indoorMix | 0 至 1，环境遮蔽响应的混合 |
| litAmbientMix | 0 至 1，环境量与单位填充的混合权重 |
| mainColorMix | 0 至 1，主灯颜色对可见环境项的调制 |
| shadowOverride | 0 至 1，实际主灯阴影向完全可见的混合 |

未知字段、非法版本及非有限输入触发错误。
非法解析保留调用方已有设置。
方向必须归一，保存时再次验证范围。
设置版本 1 至 16 迁移到默认关闭状态。

## 照明组合

方向响应先计算世界方向与环境法线的点积。
加上偏移后限制在 0 至 1，再应用倍率及底值。
环境法线消费材质声明的独立法线模式。
材质亮区遮罩参与环境量与单位填充的混合。

常量环境量来自 `ambientColor`。
启用 `useSceneIrradiance` 后，采用场景环境和辐照体积。
体积空间覆盖权重决定该环境量的组合。
两种来源均进入同一个照明函数。
`ambientWeight` 参与环境响应的遮蔽权重计算。
材质与屏幕遮蔽分别保留其来源和消费位置。
主灯的直接照明保持独立于该环境权重。

公共函数分别输出主灯与环境贡献。
场景阴影控制主灯能量与两类环境响应的选择。
材质遮罩独立控制亮区和颜色混合。
主灯颜色按该亮区遮罩调整饱和度。

选择的主灯通过稳定身份对应 GPU 灯光索引。
其漫反射由该组合统一计算。
其他灯的贡献经逐灯响应累加。
角色接收范围关闭时，主灯能量及颜色调制均被排除。

局部灯、镜面、自发光和专用头发响应各有归属。
输出隔离保留主灯漫反射与环境漫反射的独立结果。
画面调色和曝光继续消费完整线性输出。
原生颜色等价须同时对齐这些条件。

## 布局与记录

相机常量块绑定为 0，当前大小为 640 字节。
NPR 数据位于第 35 至 40 行，每行 16 字节。
前 34 行供现有相机、天空、探针和体积共同消费。
第 40 行 Z 分量保存全局环境权重。
该组合使用现有图像、采样器和材质表。

CPU 大小和偏移通过静态检查约束。
GPU 检查覆盖固定与索引描述符。
捕获清单保存实际照明设置，并将其纳入状态摘要。
原生来源与检查范围见[照明验收](../acceptance/nr/npr-illumination.md)。
