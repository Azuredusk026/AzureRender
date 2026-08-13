# AzureRender Toon Ramp / Shadow v1

## 1. 范围

CQ-2 用版本化、Renderer-owned Ramp Atlas 替换全材质共享的
`smoothstep(N dot L)`，并把 Direct Diffuse、Ambient、Shadow Map
Visibility、AO 与 Material Shadow Tint 拆分为可独立诊断的层级。本节点不实现
Face SDF、最终 Hair KK、Bloom 或最终调色。

## 2. 数据格式

- 源配置：`assets_public/toon_ramp_profiles.json`；
- 生成 Atlas：`assets_public/toon_ramp_atlas.ppm`；
- 格式：10 行 Material Class x 64 列线性 RGB；
- `linear` 用于 Skin/Face 等柔和分区；
- `step` 用于 Hair/Fabric/Metal/Eye 等两段或三段明暗；
- 生成与检查：`python tools/build_toon_ramp_atlas.py [--check]`。

Atlas 使用 `VK_FORMAT_R8G8B8A8_UNORM`，Descriptor binding 11。Material Class
只选择行，不按材质序号硬编码。现有 128-byte Material Push Constant 不扩展。

## 3. 光照分层

Shader 先计算独立分量，再组合：

1. `N dot L` 与 Style Mask 偏移形成 Ramp 坐标；
2. Material Class 选择 Ramp 行；
3. Ramp 只控制 Direct Diffuse 响应和 Ambient 暗部可见度；
4. Shadow Map Visibility 只衰减直接 Key Light；
5. Lam Shadow Color 只处理 Material Shadow Tint；
6. AO Color 处理 Ambient/Direct 的局部压暗；
7. Style Mask 同时影响 Ramp 坐标、AO/Shadow 权重与 Specular 权重。

`--qa-effect toon --qa-effect-state disabled` 只关闭 Ramp，不关闭 Rim、Hair KK、
Matcap 或其他风格效果。F9 仍是完整 Stylized Lighting 总开关。

## 4. QA

新增 `style-mask`、`ambient`、`direct-diffuse` 和 `shadow-tint` Isolation。
Manifest 记录 Ramp format/version/class count、Profile/Atlas 路径和 FNV-1a-64
Hash；QA Index 额外记录两个文件的 SHA-256。

## 5. 验收

- Debug/Release 构建成功；
- 公共资产 Debug Validation 120 帧通过；
- 私有显式 Profile 资产 Debug Validation 与 Release 各 120 帧通过；
- 60 帧 Lighting Sweep 完成，无 Validation warning/error；
- Toon A/B：Mean Absolute RGB Difference 0.981119，Changed Pixels
  170,526/921,600（18.503255%）；
- 所有代表 Capture Alpha 均为 255；
- 1920x1080 Release Beauty SHA-256：
  `7a90148069a8549d77837fc2587dd1da561668bde228bff2266b348e55100399`；
- Ramp Profile SHA-256：
  `0dea01e56149adf655d2210a6294c708d1b407e3caa154a35b2413dea3a262e1`；
- Ramp Atlas SHA-256：
  `97b1a475d1d2c342b5aaf59560a7b58388b8a5539695e6a8888c4f9b61963cce`。

评审图位于 `captures/cq2_review_v1/cq2_review_sheet.png`。下一节点为 CQ-3
Face SDF 与脸部 Overlay。
