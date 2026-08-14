# AzureRender CQ-0 角色视觉 QA Harness

## 1. 目的与边界

CQ-0 不负责改善角色 Shader。它固定后续所有角色美术节点共同使用的测试条件，避免同一效果在不同机位、灯光、动画时间或参数下被错误比较。

当前 Harness 版本为 `CQ-0-v1`。每个 Capture Manifest 都记录：

- 固定 Camera Preset、Camera Position、Target 和模型旋转；
- Light Preset 与 Showcase Preset；
- 被测试效果、Enabled/Disabled/Isolation 状态；
- Isolation View、Diagnostic View；
- FNV-1a-64 状态哈希；
- 版本化 Face SDF、Bloom、Outline 与 Final Grade 设置；
- 资产、GPU、分辨率、FPS、动画和原有 Capture 信息。

状态哈希用于发现配置变化，不代替文件哈希。批量脚本的 `qa_index.json` 会额外保存资产、可执行文件和每个已编译 SPIR-V Shader 的 SHA-256。

## 2. 固定机位

| 名称 | 用途 |
|---|---|
| `full-body-front` | 正面全身、轮廓、整体材质与阴影。 |
| `face-front` | 正脸、Face SDF、眼部与刘海遮挡。 |
| `face-three-quarter` | 脸部体积、发束高光、Specular 与 Rim。 |
| `back-detail` | 背部机械结构、头发、衣料与 Emissive。 |
| `lighting-sweep` | 固定相机下连续旋转角色，检查效果稳定性与闪烁。 |

除 `lighting-sweep` 外，QA Capture 会把动画固定在零时刻并关闭自动旋转，从而保证 A/B 像素可比。

## 3. 固定灯光环境

| 名称 | 用途 |
|---|---|
| `neutral-material` | 中性材质检查，减少风格灯光干扰。 |
| `stylized-key` | 当前主要作品集 Key/Fill/Rim 组合。 |
| `specular-rim` | 降低正面填充并增强轮廓，用于 Hair/Specular/Rim。 |
| `rear-emissive` | 暗环境，用于背部 Emissive 亮度层级。 |

## 4. Isolation 与 A/B

`--qa-isolation` 当前支持：

`beauty|albedo|world-normal|depth|diffuse-band|shadow-visibility|hair-kk|rim|specular|emissive|outline|shadow-map|material-id|style-mask|ambient|direct-diffuse|shadow-tint|face-sdf|overlay|bloom`

`--qa-effect` 当前支持：

`toon|shadow|hair-kk|rim|specular|emissive|outline|face-sdf|overlay|bloom`

`--qa-effect-state` 支持：

- `enabled`：正常 Beauty 中启用该效果；
- `disabled`：只关闭目标效果，其他状态不变；
- `isolation`：单独显示目标效果；Outline 使用其已有屏幕空间诊断视图。

普通 Shader 分量 Isolation 会同时关闭倒壳轮廓与屏幕空间内部轮廓，避免其他效果污染输出。`outline` A/B 同时控制两条轮廓路径；Isolation 本身显示屏幕空间边缘响应。

Face SDF 与 Overlay 已在 CQ-3 接入同一 Harness。Overlay 为 authored Base Color/Alpha
透明层，不参与 Shadow Map、toon、specular、Matcap；两者均支持 enabled/disabled/isolation。
Material ID 已在 CQ-1 接入，CQ-2 增加 Style Mask、Ambient、Direct Diffuse 与 Shadow
Tint 分层视图，并把 Ramp Profile/Atlas Hash 写入 Manifest 与 QA Index。
CQ-6 将 Outline 强度、深度/法线阈值、颜色以及 Exposure、Tone Mapping、Saturation、
Contrast、Tint 写入 Manifest 和状态哈希。Outline enabled/disabled/isolation 继续复用
既有固定 QA Case，不另造不可比较的测试名称。

## 5. 单次 Capture

```powershell
./build/ninja-debug/AzureRender.exe `
  --asset ./assets_private/laevat_skinned/laevat_skinned_material.glb `
  --width 1280 --height 720 `
  --capture-dir ./captures/cq0_probe `
  --capture-frames 1 --capture-fps 60 `
  --qa-camera face-three-quarter `
  --qa-light specular-rim `
  --qa-effect hair-kk `
  --qa-effect-state isolation
```

## 6. 批量 Capture

```powershell
./tools/run_character_qa.ps1 `
  -Executable ./build/ninja-debug/AzureRender.exe `
  -Asset ./assets_private/laevat_skinned/laevat_skinned_material.glb `
  -OutputRoot ./captures/cq0_full `
  -Mode all
```

模式：

- `baseline`：5 机位 × 4 灯光；
- `isolation`：20 种 Beauty/Isolation 输出；
- `ab`：10 种效果的 Enabled/Disabled/Isolation；
- `all`：执行全部 54 个 Case。

脚本拒绝写入非空 Output Root，不覆盖已有证据。使用 `-Resume` 可复用完整 Case，并安全重建上次中断产生的不完整 Case。每个 Case 保留独立 PNG Manifest，根目录生成 `qa_index.json`。

Contact Sheet 工具：

```powershell
python ./tools/build_qa_contact_sheet.py `
  ./captures/cq0_full/qa_index.json `
  ./captures/cq0_full/contact_sheet.png
```

`build_image_comparison_sheet.py` 根据 JSON Layout 生成 Reference / Current / Isolation 三列评审图。Reference 只作为用户提供的视觉目标，不属于 Renderer 输出或可公开资产。

## 7. CQ-0 验收

CQ-0 只有在以下条件全部满足后才能 Complete：

- Debug/Release 构建成功；
- 公共资产 Smoke 与私有莱万汀代表 Capture 无 Validation Error；
- 五机位和四灯光均能被命令行确定性恢复；
- 每个现有可测效果都有 Enabled、Disabled、Isolation；
- Manifest 和 `qa_index.json` 可识别所有 Case，并包含状态与文件哈希；
- 代表输出完成视觉检查，没有 Alpha、Depth、遮挡或 Capture 回归。

## 8. CQ-0 v1 验收记录

2026-08-02 已完成：

- `captures/cq0_laevat_baseline_v2`：20 Case；四个 Lighting Sweep 各 60 帧；
- `captures/cq0_laevat_isolation_v2`：12 Case；
- `captures/cq0_laevat_ab_v1`：21 Case；
- `captures/cq0_review_v1/current_reference_isolation.png`：四行三列人工评审证据；
- `captures/cq0_public_smoke_v2`：公共资产 Debug Validation Smoke；
- `captures/cq0_release_representative_v1`：私有角色 Release 代表帧；
- Ninja Debug/Release 构建通过；Debug 批次未出现 Validation Warning/Error。

视觉审计结论不是“角色 Shader 已完成”：Toon 与 Hair KK 的 Enabled/Disabled 差异过弱；Emissive 只在少数部件成立；Depth 接近二值；Face SDF 与 Material ID 尚不存在。CQ-0 的完成含义仅是这些缺口现在能够被固定、复现和比较。

1280×720 A/B 首帧的全图 Mean Absolute RGB Difference / Changed Pixels 为：Toon 0.8091 / 12.986%，Shadow 0.1101 / 2.836%，Hair KK 0.0142 / 1.597%，Rim 1.6797 / 4.365%，Specular 6.0733 / 23.056%，Emissive 0.0277 / 0.244%，Outline 1.0557 / 4.260%。该数值只作为 CQ-0 起点；后续节点仍以局部 ROI、Reference 对照与人工视觉 Gate 为准。
