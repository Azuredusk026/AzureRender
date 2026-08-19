# 角色场景展示

## 当前交付

当前视频由五个 16 秒段落组成，顺序为最终渲染、原始模型（Albedo）、世界法线、阴影可见度和材质 ID 分区。每段均使用同一固定相机、脚部中心转台与 `2π/16 rad/s` 匀速角速度：角色从朝左开始，完成 360° 旋转并回到朝左。画面为 1600×900、24 fps、SAR 1:1，不做非等比拉伸。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-223233_角色眉毛蒙皮修复近景.png` | 原始拓扑与蒙皮下的连续红色眉毛近景 | `575EB6CD2175DD292738897BF47BBB8028F2CB90E2BE1A042A1BE5B77DCAC75A` |
| `images/20260819-223233_角色朝左起始.png` | 转台首帧，角色朝左 | `9C3648C172664C8B640DF3F3C150BA8B3A52F74114B5550065370AD844D3D1BF` |
| `images/20260819-223233_角色正面眉毛连续.png` | 四分之一圈后的正面眉毛与最终渲染 | `EC5F0C90F369275ECD38771B17D991449948037C5A13B428768897010D3824FF` |
| `images/20260819-223233_角色原始模型.png` | Albedo 原始贴图与材质底色 | `E34FFF7472EBF25800F77AE0DE1A5091264B2689EF9A1BDC12C9B604B5E307FE` |
| `images/20260819-223233_角色法线分布.png` | 世界空间法线 | `320C746A33F5B4221B34E5638A19EB41B1DBFD76C4B059BE9A77EA222513C892` |
| `images/20260819-223233_角色阴影分布.png` | 主光与 shadow-map 可见度 | `2AE6B6B37AABEE24A9E93412BB0726A626BD20789D1ED5C0A95D5863309D39AD` |
| `images/20260819-223233_角色材质分区.png` | Skin/Hair/Fabric/Overlay/Platform 分类 | `05562663CBC6168C79542337A9CCD2A0D3553DCC3634902B8F8F995F2080BEAF` |
| `video/20260819-223233_角色左向起转五模式展示.mp4` | 1920 帧、80.00 秒、5 个完整转台段落 | `C758421EDA688C9CCA98149E4EF88E74C1D60B5754957481A8C54C88F641709D` |

视频已完整解码 1920 帧。格式为 H.264 High、yuv420p/BT.709、1600×900、24 fps、SAR 1:1、DAR 16:9。

## 眉毛网格与蒙皮审计

- `M_actor_laevat_brow_01` 所在 primitive 共 578 顶点、1878 个索引和 626 个三角形。
- 网格包含 34 个独立拓扑小岛，是眉毛与睫毛卡片的合集；它并非一张应当整体连续的眉毛网格。
- 审计未发现退化三角形、非流形边、零权重顶点或权重和异常；左右权重分布对称。
- 使用骨骼仅属于 `eye*`、`eyelash*` 和 `brow*`，没有身体等无关额外骨骼导致位移。
- 旧方案围绕单一全局 pivot 放大整个 primitive，会把分别蒙皮的小岛拉开并产生断裂尖角，现已完全撤销。
- 当前仅在片元阶段对 Face D 的深红眉毛笔画做纵向 2 texel UV 膨胀；原始顶点、拓扑和权重不变。
- 保留 `0.01 m` 局部上移与沿视线 `0.04679 m` 的 WPO，避免与脸部重叠和 z-fighting。
- 审计工具：`tools/audit_brow_mesh.js`。

## 头发数据

- `T_actor_laevat_hair_01_D`：头发 Base Color。
- `T_actor_laevat_hair_01_HN`：Hair Data；RG 参与基础发束法线，BA 驱动双层 Kajiya-Kay 高光方向。
- `T_actor_laevat_hair_01_P`：Hair Master 的 packed 材质数据；注入器同时支持 `_P` 与布料使用的 `T_RGBA_P`。

## 复现

每种模式均捕获 384 帧。`--qa-isolation` 依次使用 `beauty`、`albedo`、`world-normal`、`shadow-visibility` 和 `material-id`。

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type character `
  --asset .\assets_private\laevat_skinned\laevat_idle_material.glb `
  --portfolio --qa-light stylized-key `
  --qa-isolation beauty `
  --width 1600 --height 900 `
  --capture-dir .\build\character_beauty `
  --capture-frames 384 --capture-fps 24
```

私有角色模型、派生 GLB、纹理、截图和视频只用于本机视觉验收，不进入 Git、CI、安装树或公开作品集。公共自动化回归继续使用 `assets_public/test_model.gltf`。
