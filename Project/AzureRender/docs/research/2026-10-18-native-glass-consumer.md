# 中庭原生玻璃折射消费者

日期：2026-10-18。阶段：NR2、NR5。
材质：`M_common_glass+1_001_02`。
着色器：`HGRP/LitTransparent`。
通道：`ForwardLit`，透明队列为 `3000`。

## 来源与匹配范围

Shader 来源为以下原生对象：

`CAB-e695120be0773bc41d044f9018921638:9181137009890694473`。

本机原生 bundle 为：

`initial/43b443d89b878589762a8645.ab`。

嵌入类型树可完整导出原生着色数据。
LOD 600 的 D3D11 第 60 条程序包含像素着色器。
材质关键字为 `_GLASS_REFRACTION_SCENECOLOR`。
该变体还包含两个全局关键字：
`HG_ENABLE_MV`、`SRP_INSTANCING_ON`。
同关键字 Vulkan 程序可反编译到同一消费公式。
游戏当前全局关键字集合仍待运行时采集。

## 折射计算

原生着色器先计算基础透明度。
它由底色纹理 Alpha 与底色 Alpha 相乘得到。
顶点透明度通过独立参数参与插值。
本材质的顶点透明度参数为 `0`。

物理偏移使用视线、表面法线与 `_IoR`。
厚度通过 `_IsShell` 与片元深度修正。
自定义偏移读取折射图的两个线性通道。
两种偏移按 `_UseCustomRefractTex` 插值。
本材质该参数为 `1`，其偏移公式为：

```text
offset = (sample(RefractTex, uv0 * 0.7).rg * 2 - 1) * 0.008
```

原生折射图为 `T_detail_com_wrinkle+1_001_01a_NRO`。
源纹理标识为 `4462903074158448482`。
纹理缩放为 `(0.7, 0.7)`，偏移为零。

深度保护优先尝试完整偏移。
背景线性深度须大于等于当前片元深度。
完整偏移失败时尝试四分之一偏移。
两个候选均失败时使用原屏幕坐标。
场景颜色使用线性过滤与镜像寻址。

## 颜色与混合

原生像素程序包含以下消费公式：

```text
w = saturate((1 - alpha) / max(1 - contribution, 0.01))
background = sceneColor(depthSafeUV) * tint.rgb * brightness
surface = diffuseLighting + lerp(specularLighting, background, w)
```

本材质 `contribution=0.8`、`brightness=0.271`。
`tint=(1,1,1,1)`，源 Alpha 保留至输出。
后续计算继续应用原生雾与颜色控制。
目标颜色使用源透明度混合。
深度写入关闭，剔除关闭。

`w` 在 Alpha 为 `0.8` 时达到 `1`。
Alpha 为 `1` 时，`w` 为 `0`。
材质边框与透光区域共享源纹理透明度。

## 最小引擎契约

透明材质读取同帧不透明场景的 HDR 颜色快照。
折射颜色与表面光照使用同一线性工作空间。
深度采样提供可比较的线性相机深度。
快照在透明队列写入前完成。
资源状态切换遵守渲染图的读写依赖。

材质契约包含折射图、UV 变换与偏移强度。
颜色契约包含色调、亮度与混合贡献参数。
源纹理 Alpha、透明混合与表面光照分别保留。
物理偏移扩展保留折射率、厚度与壳体参数。
通用材质按配置选择折射能力。

## 证据与验证边界

运行证据为 `native-glass-consumer-evidence.json`。
运行目录为：

`build/runs/20261017-175206-nr-playable-integration-d4e789`。

像素汇编为 `glass-shader-bytecode/variant60.asm`。
折射位于第 163 至 205 行。
颜色混合位于第 2461 至 2469 行。
Vulkan 反编译文件为 `minimal-vulkan.hlsl`。
对应折射位于第 896 至 910 行。
颜色混合位于第 2924 行。
来源指纹与常量偏移均保存在运行证据中。

本调查完成静态公式与依赖识别。
引擎视觉验收、完整光照及雾仍需独立验证。
