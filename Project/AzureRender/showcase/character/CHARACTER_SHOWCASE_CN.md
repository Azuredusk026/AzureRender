# 角色场景展示

## 当前交付

本轮重点交付为两个独立的 16 秒视频：Face SDF 连续性与 Hair 体积轮廓修复后的最终渲染，以及 PCSS 阴影可见度。两段使用同一固定相机、脚部中心转台与 `2π/16 rad/s` 匀速角速度，从朝左开始完成 360° 旋转并回到朝左。最终渲染使用 `laevat_idle_material_face_sdf.glb`，其中 Face 材质已嵌入 1024x1024 SDF 并绑定 `Bip001_Head`；运行日志确认没有使用 2x2 回退纹理。

输出为原生 2K QHD：2560×1440、24 fps、SAR 1:1，不做插值放大或非等比拉伸。两个视频均完整解码 384 帧、16.00 秒，编码格式为 H.264 High、yuv420p、BT.709、DAR 16:9。旧五模式 80 秒视频继续保留为综合技术展示，不再作为本轮 Face SDF 修复证据。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260820-191912_角色面部连续与发丝AO正面.png` | 正面最终渲染；Face 连续、Hair AO 与内部发束描边可见 | `538E7E2344CC7A09F8C5F5E670E4F9140707E0C85D4C006097E06F6CA9AC5ED4` |
| `images/20260820-170550_角色PCSS正面阴影展示.png` | 同帧 PCSS 阴影可见度 | `304A945CEA5F2880977E121953C4CE57237888625228ED3EEB6388C4F16C7072` |
| `video/20260820-191912_角色面部连续与发丝AO最终渲染2K.mp4` | 连续 Face SDF、增强 Hair AO 与 HN 内部轮廓完整转台 | `57D52F9491E56C54C559DB008411BE096EDA8EC5D9C0F899E94C7C9DD43A77D7` |
| `video/20260820-170550_角色SDF阴影展示2K.mp4` | PCSS 阴影可见度完整转台 | `93E7EF3D8926360E3E6D734FB8C993FC0DE15642EF8ED6ADA3C28C694C071A13` |

## 历史综合展示

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260820-184607_角色明暗正面最终渲染.png` | Face SDF 硬切换修复前的正面图 | `71E982EAD3F7ED53B6587A9BF5AE0C8007F3CE4CDA1A73DED7FCE46968F8DA3F` |
| `video/20260820-184607_角色明暗最终渲染2K.mp4` | Face SDF 硬切换修复前的 Beauty 转台 | `B30A59DEFF0A52F69216C6F69A3DDFD8BA329FB831ABE09BF8366A542F72FCC1` |
| `images/20260820-170550_角色SDF正面最终渲染.png` | 调整全局主光对比前的 SDF 正面图 | `49F6C79338BA2B324E4D69874F1D1144CA98098855196C8FA420B2999C953DBD` |
| `video/20260820-170550_角色SDF最终渲染2K.mp4` | 调整全局主光对比前的 SDF Beauty 转台 | `122AC9A0EF00BA36C9847717C5B09164F65E9E4325D3420E814DD3DA37AACE88` |
| `images/20260820-001437_角色PCSS朝左起始.png` | 2K Beauty 首帧与朝左起始姿态 | `29E516C4D54591448A97F044FA47622EB6DF423A5F211D7E973A6F2CB5FA6F1E` |
| `images/20260820-001437_角色PCSS正面渲染.png` | 四分之一圈后的 2K 正面最终渲染 | `E4438345AD79EB9BE1A2A8B0D2DED831EE5834DE0A660E32C56926CE7043ED00` |
| `images/20260820-001437_角色2K原始模型.png` | Albedo 原始贴图与材质底色 | `E4C9ED990CC33A718B95EB4415CF73AABB2AE151E40DFDBABBD14A0677FCA6F0` |
| `images/20260820-001437_角色2K法线分布.png` | 世界空间法线 | `0FB1054B90310A3CD3B541D2E0AF9E59EB26F0B4E6785D82E6F2026E7B22404C` |
| `images/20260820-001437_角色PCSS阴影分布.png` | blocker search 与距离相关半影 | `304A945CEA5F2880977E121953C4CE57237888625228ED3EEB6388C4F16C7072` |
| `images/20260820-001437_角色2K材质分区.png` | Skin/Hair/Fabric/Overlay/Platform 分类 | `BCF582EC1DD1C823C5E9B614889CF5007A30A93A33DE9B446156C012D44E3304` |
| `video/20260820-001437_角色PCSS五模式2K展示.mp4` | 1920 帧、80.00 秒、5 个 2K 完整转台段落 | `73DF35AD7BA731A4E7CD532B4FB3092C89EFD17AC2EEFA6C52C3A432452DDC41` |

## PCSS 软阴影

- 引擎继续使用 2048×2048 方向光 Shadow Map，避免改动黑洞场景和公共资源所有权。
- 旧固定 3×3 PCF 只有约 1 texel 半径，在全身镜头中视觉上接近硬阴影。
- 当前先用 12 个 Poisson 样本搜索遮挡物平均深度，再用 16 个 Poisson 样本执行可变半径 PCF。
- 接触区域保持较窄半影；接收面与遮挡面分离时，半影逐渐扩大。
- 默认最大滤波半径为 8 texel，可通过编辑器 `Shadow Softness` 在 1–16 texel 范围调节。
- 参数属于 `RenderSettings v7`，会进入 `.azscene`、捕获状态哈希和 `capture_manifest.json`。
- 固定 Poisson 核不做逐帧随机旋转，避免转台视频出现阴影噪声闪烁。

## 眉毛与头发稳定性

眉毛/睫毛 primitive 的 34 个拓扑小岛、578 个顶点和相关蒙皮保持上一轮审计结果：无退化或非流形三角形、权重归一且左右对称，只受 `eye*`、`eyelash*` 和 `brow*` 骨骼影响。当前仅对 Face D 深红笔画执行纵向 2 texel UV 膨胀，不缩放或移动眉毛网格。

`T_actor_laevat_hair_01_D`、`T_actor_laevat_hair_01_HN` 和 `T_actor_laevat_hair_01_P` 继续分别用于 Base Color、发束法线/双层 Kajiya-Kay 方向与 packed 材质数据。本轮阴影修改没有改变这些材质输入。

最终 Beauty 使用 `1.52` 固定世界主光、`0.06` 填充和 `0.36` Toon 环境暗部下限。Face SDF 亮端收至 `0.78`，避免正面脸部脱离同侧肩胸皮肤；Hair 漫反射按 Base Color 色相回投并保留 ramp-aware 红色能量下限。五个四分之一圈关键方向均确认头发维持红色、背面服装保留纹理且受光/背光差异明显。

Face SDF 原始/镜像方向现以 `smoothstep(-0.18, 0.18, lateralLight)` 连续混合。原问题区间对应的 2K 第 144、152、160、168 帧已逐张检查，不再出现整脸纯亮跳变；720p 第 130-179 帧脸部区域最大相邻帧平均亮度变化为 `0.204/255`。Hair AO 叠加 Style Mask、掠射角和 HN 法线 cavity，内部描边使用 72% shaded normal 与 0.55 Hair participation；外轮廓壳宽度保持不变。

## 复现

两个重点视频均捕获 384 帧，`--qa-isolation` 分别使用 `beauty` 和 `shadow-visibility`：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type character `
  --asset .\assets_private\laevat_skinned\laevat_idle_material_face_sdf.glb `
  --portfolio --qa-light stylized-key `
  --qa-isolation beauty `
  --width 2560 --height 1440 `
  --capture-dir .\build\character_face_continuous_hair_ao_2k `
  --capture-frames 384 --capture-fps 24
```

将 `--qa-isolation beauty` 改为 `--qa-isolation shadow-visibility` 即可复现阴影展示。捕获前必须运行 `tools/audit_face_sdf_compatibility.py ... --require-compatible`，并确认运行日志包含 `texture=1024x1024, headNode=Bip001_Head`。

确定性捕获使用隐藏、无边框 surface，避免 Windows 工作区把 1440 高度压缩。私有角色模型、派生 GLB、纹理、截图和视频只用于本机验收，不进入 Git、CI、安装树或公开作品集。
