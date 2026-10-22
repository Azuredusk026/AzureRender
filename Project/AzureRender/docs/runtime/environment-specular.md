# 环境镜面响应

普通材质的环境镜面模型由渲染设置持有。
该能力适用于室内反射与独立材质预览。
全局环境和局部探针消费同一模型选择。
角色画像与直接照明遵循各自的表面契约。

## 配置与保存

`RenderSettings.environmentSpecularModel` 使用有类型枚举。
默认值为 `EnvironmentSpecularModel::LegacySchlick`。
拟合值为 `EnvironmentSpecularModel::GgxMultiscatterFit`。
非法枚举在设置校验时拒绝。

关卡字段为 `environmentSpecularModel`。
可选名称为 `legacy-schlick` 和 `ggx-multiscatter-fit`。
省略字段时保留目标设置的现值。
未知名称与非字符串值触发解析失败。

文本场景使用渲染设置版本 15。
版本 15 保存并读取模型名称。
版本 1 至 14 迁移为默认模型。
关卡解析失败保持目标设置完整。

## 拟合模型

拟合模型作用于启用场景照明的普通材质。
输入为线性基色、金属度、粗糙度和视角余弦。
环境介电反射率为 0.04。
金属反射率按基色与金属度混合。

```text
F0 = mix(0.04, baseColor, metallic)
Fenv = (F0*A+B) * (1 + (1-A-B)/(A+B)*F0)
```

`A` 与 `B` 使用粗糙度和视角余弦的有理拟合。
公共数学入口为 `ggxMultiscatterEnvironment`。
CPU 接口要求输入有限且位于 `[0,1]`。
GLSL 入口钳制粗糙度与视角余弦。

环境反射率独立于自发光纹理的透明通道。
纹理粗糙度决定既有预滤波层级选择。
源纹理、探针强度和盒投影分别持有配置。
拟合函数只负责镜面积分响应。

## 资源与验证

模型编号通过现有相机统一缓冲区传递。
该能力沿用环境与探针资源的生命周期。
材质预览及场景关卡共同验证公共接口。
GPU 入口为 `tools/test_environment_specular_gpu.py`。

静态数学参考由源指令的独立解释器生成。
运行中的全局倍率、湿度和 GI 补偿单独核验。
屏幕动态反射属于独立照明能力。
原生画面对照按 NR5 的完整门禁验收。
